#include <renderer/RenderLoop.h>
#include <renderer/Buffer.h>
#include <renderer/DepthLinearizePipeline.h>
#include <renderer/DepthMipPipeline.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SsaoBlurPipeline.h>
#include <renderer/SsaoKernel.h>
#include <renderer/SsaoPipeline.h>
#include <renderer/Swapchain.h>
#include <renderer/TonemapPipeline.h>
#include <tools/Log.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace bimeup::renderer {

RenderLoop::RenderLoop(const Device& device, Swapchain& swapchain,
                       const std::string& shaderDir,
                       VkSampleCountFlagBits samples)
    : m_device(device),
      m_swapchain(swapchain),
      m_shaderDir(shaderDir),
      m_samples(samples) {
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
    CreateRenderPass();
    CreatePresentRenderPass();
    CreateHdrResources();
    CreateDepthResources();
    CreateFramebuffers();
    CreateTonemapDescriptors();
    CreateTonemapPipeline();
    UpdateTonemapDescriptors();
    CreateDepthPyramidDescriptors();
    CreateDepthPyramidPipelines();
    CreateDepthPyramidResources();
    UpdateDepthPyramidDescriptors();
    CreateSsaoDescriptors();
    CreateSsaoPipelines();
    CreateSsaoResources();
    UpdateSsaoDescriptors();
    ClearAoImagesToWhite();
    UpdateTonemapDescriptors();  // re-run so the AO binding points at the freshly created images

    if (bimeup::tools::Log::GetLogger()) {
        LOG_INFO("RenderLoop created (max {} frames in flight, MSAA {}x, HDR resolve)",
                 MAX_FRAMES_IN_FLIGHT, static_cast<int>(m_samples));
    }
}

RenderLoop::~RenderLoop() {
    WaitIdle();
    Cleanup();
}

bool RenderLoop::BeginFrame() {
    VkDevice device = m_device.GetDevice();

    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        device, m_swapchain.GetSwapchain(), UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    if (m_preMainPass) {
        m_preMainPass(cmd);
    }

    // Clear values indexed to match CreateRenderPass:
    //   [0] scene colour, [1] normal ((0,0) → +Z after oct-decode),
    //   [2] outline stencil id (0 = background), [3] depth,
    //   [4..6] resolve targets (DONT_CARE → filler).
    std::array<VkClearValue, 7> clearValues{};
    clearValues[0].color = m_clearColor;
    clearValues[1].color = VkClearColorValue{{0.0F, 0.0F, 0.0F, 0.0F}};
    clearValues[2].color.uint32[0] = 0u;  // R8_UINT — integer clear path
    clearValues[3].depthStencil = {1.0F, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass;
    rpInfo.framebuffer = m_framebuffers[m_currentImageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_swapchain.GetExtent();
    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_frameStarted = true;
    return true;
}

bool RenderLoop::EndFrame() {
    VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
    VkExtent2D extent = m_swapchain.GetExtent();

    // End the main (HDR) render pass. finalLayout on the HDR attachment
    // transitions it to SHADER_READ_ONLY_OPTIMAL for the tonemap sampler.
    vkCmdEndRenderPass(cmd);

    // RP.4d — build the depth pyramid between the main pass and the
    // tonemap pass. No-op under MSAA (shader needs sampler2DMS); non-MSAA
    // path dispatches linearize then three mip downsamples with barriers.
    BuildDepthPyramid(cmd);

    // RP.5d — SSAO main + separable blur, also gated off under MSAA.
    // Produces the half-res AO target that the tonemap fragment samples.
    RunSsao(cmd);

    // Begin the tonemap/present pass targeting the swapchain image.
    VkRenderPassBeginInfo presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    presentInfo.renderPass = m_presentRenderPass;
    presentInfo.framebuffer = m_presentFramebuffers[m_currentImageIndex];
    presentInfo.renderArea.offset = {0, 0};
    presentInfo.renderArea.extent = extent;
    presentInfo.clearValueCount = 0;  // tonemap fullscreen-tri covers every pixel

    vkCmdBeginRenderPass(cmd, &presentInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_tonemapPipeline->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_tonemapPipeline->GetLayout(), 0, 1,
                            &m_tonemapDescriptorSets[m_currentImageIndex], 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    if (m_inPresentPass) {
        m_inPresentPass(cmd);
    }

    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentImageIndex]};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submitInfo,
                      m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer");
    }

    VkSwapchainKHR swapchains[] = {m_swapchain.GetSwapchain()};

    VkPresentInfoKHR presentSubmit{};
    presentSubmit.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentSubmit.waitSemaphoreCount = 1;
    presentSubmit.pWaitSemaphores = signalSemaphores;
    presentSubmit.swapchainCount = 1;
    presentSubmit.pSwapchains = swapchains;
    presentSubmit.pImageIndices = &m_currentImageIndex;

    VkResult result = vkQueuePresentKHR(m_device.GetPresentQueue(), &presentSubmit);

    m_frameStarted = false;
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false;
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    return true;
}

void RenderLoop::WaitIdle() {
    vkDeviceWaitIdle(m_device.GetDevice());
}

void RenderLoop::SetClearColor(float r, float g, float b, float a) {
    m_clearColor = {{r, g, b, a}};
}

void RenderLoop::SetPreMainPassCallback(PreMainPassCallback callback) {
    m_preMainPass = std::move(callback);
}

void RenderLoop::SetInPresentPassCallback(InPresentPassCallback callback) {
    m_inPresentPass = std::move(callback);
}

VkCommandBuffer RenderLoop::GetCurrentCommandBuffer() const {
    return m_commandBuffers[m_currentFrame];
}

uint32_t RenderLoop::GetCurrentFrameIndex() const {
    return m_currentFrame;
}

uint32_t RenderLoop::GetCurrentImageIndex() const {
    return m_currentImageIndex;
}

VkRenderPass RenderLoop::GetRenderPass() const {
    return m_renderPass;
}

VkRenderPass RenderLoop::GetPresentRenderPass() const {
    return m_presentRenderPass;
}

VkImageView RenderLoop::GetStencilImageView(uint32_t imageIndex) const {
    if (imageIndex >= m_stencilImageViews.size()) {
        return VK_NULL_HANDLE;
    }
    return m_stencilImageViews[imageIndex];
}

VkImageView RenderLoop::GetNormalImageView(uint32_t imageIndex) const {
    if (imageIndex >= m_normalImageViews.size()) {
        return VK_NULL_HANDLE;
    }
    return m_normalImageViews[imageIndex];
}

VkImageView RenderLoop::GetDepthPyramidView(uint32_t imageIndex) const {
    if (imageIndex >= m_depthPyramidSampledViews.size()) {
        return VK_NULL_HANDLE;
    }
    return m_depthPyramidSampledViews[imageIndex];
}

VkImageView RenderLoop::GetAoImageView(uint32_t imageIndex) const {
    if (imageIndex >= m_aoViewsA.size()) {
        return VK_NULL_HANDLE;
    }
    return m_aoViewsA[imageIndex];
}

void RenderLoop::SetProjection(const glm::mat4& proj, float nearZ, float farZ) {
    m_proj = proj;
    m_nearZ = nearZ;
    m_farZ = farZ;
}

VkSampleCountFlagBits RenderLoop::GetSampleCount() const {
    return m_samples;
}

void RenderLoop::SetSampleCount(VkSampleCountFlagBits samples) {
    if (samples == m_samples) {
        return;
    }
    WaitIdle();
    m_tonemapPipeline.reset();  // depends on the main pass sample count? only for parity — rebuild anyway
    CleanupFrameResources();
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device.GetDevice(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    m_samples = samples;
    CreateRenderPass();
    CreateHdrResources();
    CreateDepthResources();
    CreateFramebuffers();
    CreateDepthPyramidResources();
    CreateSsaoResources();
    CreateTonemapPipeline();
    UpdateDepthPyramidDescriptors();
    UpdateSsaoDescriptors();
    ClearAoImagesToWhite();
    UpdateTonemapDescriptors();
    if (bimeup::tools::Log::GetLogger()) {
        LOG_INFO("RenderLoop MSAA set to {}x", static_cast<int>(m_samples));
    }
}

void RenderLoop::RecreateForSwapchain() {
    WaitIdle();
    CleanupFrameResources();
    CreateHdrResources();
    CreateDepthResources();
    CreateFramebuffers();
    CreateDepthPyramidResources();
    CreateSsaoResources();
    UpdateDepthPyramidDescriptors();
    UpdateSsaoDescriptors();
    ClearAoImagesToWhite();
    UpdateTonemapDescriptors();
}

void RenderLoop::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_device.GetGraphicsQueueFamily();

    if (vkCreateCommandPool(m_device.GetDevice(), &poolInfo, nullptr, &m_commandPool) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void RenderLoop::CreateCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo,
                                 m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void RenderLoop::CreateSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_device.GetDevice(), &semaphoreInfo, nullptr,
                              &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device.GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) !=
                VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }

    uint32_t imageCount = m_swapchain.GetImageCount();
    m_renderFinishedSemaphores.resize(imageCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(m_device.GetDevice(), &semaphoreInfo, nullptr,
                              &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render-finished semaphore");
        }
    }
}

void RenderLoop::CreateRenderPass() {
    m_depthFormat = FindDepthFormat(m_device.GetPhysicalDevice());
    const bool multisampled = m_samples != VK_SAMPLE_COUNT_1_BIT;

    // Attachment layout (MRT — scene colour + oct-packed normal G-buffer +
    // outline stencil id):
    //   Non-MSAA: [0] HDR colour, [1] normal, [2] stencil id, [3] depth.
    //   MSAA:     [0] HDR MSAA, [1] normal MSAA, [2] stencil MSAA, [3] depth,
    //             [4] HDR resolve, [5] normal resolve, [6] stencil resolve.
    // Final layouts on the sampled single-sample targets (HDR + normal +
    // stencil resolve) are SHADER_READ_ONLY_OPTIMAL so tonemap / SSAO / SSIL /
    // outline can bind them straight as samplers.

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = HDR_FORMAT;
    colorAttachment.samples = m_samples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                           : VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = NORMAL_FORMAT;
    normalAttachment.samples = m_samples;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                            : VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription stencilAttachment{};
    stencilAttachment.format = STENCIL_FORMAT;
    stencilAttachment.samples = m_samples;
    stencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    stencilAttachment.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                             : VK_ATTACHMENT_STORE_OP_STORE;
    stencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencilAttachment.finalLayout = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                 : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = m_samples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Non-MSAA: keep depth and transition to SHADER_READ_ONLY so the RP.4
    // linearize compute pass can sample it. MSAA: the pyramid is gated off
    // (shader wants sampler2D, not sampler2DMS) so discarding depth at pass
    // end is fine and avoids an extra resolve.
    depthAttachment.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE
                                           : VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = multisampled ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                               : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription hdrResolveAttachment{};
    hdrResolveAttachment.format = HDR_FORMAT;
    hdrResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    hdrResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    hdrResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    hdrResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    hdrResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    hdrResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    hdrResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalResolveAttachment{};
    normalResolveAttachment.format = NORMAL_FORMAT;
    normalResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription stencilResolveAttachment{};
    stencilResolveAttachment.format = STENCIL_FORMAT;
    stencilResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    stencilResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    stencilResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencilResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkAttachmentReference, 3> colorRefs{};
    colorRefs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[2] = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference depthRef{};
    depthRef.attachment = 3;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 3> resolveRefs{};
    resolveRefs[0] = {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    resolveRefs[1] = {5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    resolveRefs[2] = {6, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;
    if (multisampled) {
        subpass.pResolveAttachments = resolveRefs.data();
    }

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Post-pass dependency: the RP.4 linearize compute shader samples the
    // depth attachment immediately after vkCmdEndRenderPass. Makes the
    // layout transition to SHADER_READ_ONLY visible to compute reads and
    // the depth writes themselves available. No-op on MSAA since we never
    // sample the multisample depth image.
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 7> attachments = {
        colorAttachment, normalAttachment, stencilAttachment, depthAttachment,
        hdrResolveAttachment, normalResolveAttachment, stencilResolveAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = multisampled ? 7U : 4U;
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    rpInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_device.GetDevice(), &rpInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void RenderLoop::CreatePresentRenderPass() {
    // Tonemap + UI pass targeting the swapchain (always 1× / no depth).
    VkAttachmentDescription color{};
    color.format = m_swapchain.GetFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // tonemap covers every pixel
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Two external deps folded into one: the swapchain image must transition
    // UNDEFINED → COLOR_ATTACHMENT_OPTIMAL before we write, and the HDR target
    // written by the main pass must be visible to the tonemap fragment read.
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &color;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device.GetDevice(), &rpInfo, nullptr, &m_presentRenderPass) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create present render pass");
    }
}

void RenderLoop::CreateHdrResources() {
    VkExtent2D extent = m_swapchain.GetExtent();
    uint32_t imageCount = m_swapchain.GetImageCount();

    m_hdrImages.assign(imageCount, VK_NULL_HANDLE);
    m_hdrImageViews.assign(imageCount, VK_NULL_HANDLE);
    m_hdrAllocations.assign(imageCount, VK_NULL_HANDLE);
    m_normalImages.assign(imageCount, VK_NULL_HANDLE);
    m_normalImageViews.assign(imageCount, VK_NULL_HANDLE);
    m_normalAllocations.assign(imageCount, VK_NULL_HANDLE);
    m_stencilImages.assign(imageCount, VK_NULL_HANDLE);
    m_stencilImageViews.assign(imageCount, VK_NULL_HANDLE);
    m_stencilAllocations.assign(imageCount, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = HDR_FORMAT;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;  // single-sample resolve target
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo,
                           &m_hdrImages[i], &m_hdrAllocations[i], nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create HDR image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_hdrImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = HDR_FORMAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr,
                              &m_hdrImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create HDR image view");
        }

        // Normal G-buffer target — single-sample (resolve destination when
        // MSAA is on, direct target otherwise). SAMPLED is required because
        // SSAO/SSIL later bind this view through a descriptor.
        VkImageCreateInfo normalInfo = imageInfo;
        normalInfo.format = NORMAL_FORMAT;
        if (vmaCreateImage(m_device.GetAllocator(), &normalInfo, &allocInfo,
                           &m_normalImages[i], &m_normalAllocations[i], nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create normal G-buffer image");
        }

        VkImageViewCreateInfo normalView = viewInfo;
        normalView.image = m_normalImages[i];
        normalView.format = NORMAL_FORMAT;
        if (vkCreateImageView(m_device.GetDevice(), &normalView, nullptr,
                              &m_normalImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create normal G-buffer image view");
        }

        // Outline stencil G-buffer — same shape as the normal target.
        // SAMPLED so the RP.6b outline pass can read via `usampler2D`.
        VkImageCreateInfo stencilInfo = imageInfo;
        stencilInfo.format = STENCIL_FORMAT;
        if (vmaCreateImage(m_device.GetAllocator(), &stencilInfo, &allocInfo,
                           &m_stencilImages[i], &m_stencilAllocations[i], nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create stencil G-buffer image");
        }

        VkImageViewCreateInfo stencilView = viewInfo;
        stencilView.image = m_stencilImages[i];
        stencilView.format = STENCIL_FORMAT;
        if (vkCreateImageView(m_device.GetDevice(), &stencilView, nullptr,
                              &m_stencilImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create stencil G-buffer image view");
        }
    }
}

void RenderLoop::CreateDepthResources() {
    VkExtent2D extent = m_swapchain.GetExtent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_depthFormat;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = m_samples;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // SAMPLED_BIT so the RP.4 depth-linearize compute shader can bind this
    // image as sampler2D post-render-pass (non-MSAA path; MSAA depth is
    // never sampled today). Cheap addition even when the pyramid is gated
    // off, since the image is always resident.
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo,
                       &m_depthImage, &m_depthAllocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr, &m_depthImageView) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view");
    }

    // Multisampled HDR colour target (transient — resolved into the 1× HDR
    // image each frame). Same format as m_hdrImages so subpass resolve is legal.
    if (m_samples != VK_SAMPLE_COUNT_1_BIT) {
        VkImageCreateInfo colorInfo{};
        colorInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        colorInfo.imageType = VK_IMAGE_TYPE_2D;
        colorInfo.format = HDR_FORMAT;
        colorInfo.extent = {extent.width, extent.height, 1};
        colorInfo.mipLevels = 1;
        colorInfo.arrayLayers = 1;
        colorInfo.samples = m_samples;
        colorInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
        colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vmaCreateImage(m_device.GetAllocator(), &colorInfo, &allocInfo,
                           &m_colorImage, &m_colorAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA color image");
        }

        VkImageViewCreateInfo colorView{};
        colorView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorView.image = m_colorImage;
        colorView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorView.format = HDR_FORMAT;
        colorView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorView.subresourceRange.baseMipLevel = 0;
        colorView.subresourceRange.levelCount = 1;
        colorView.subresourceRange.baseArrayLayer = 0;
        colorView.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device.GetDevice(), &colorView, nullptr,
                              &m_colorImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA color image view");
        }

        // Multisampled normal G-buffer (transient, same shape as MSAA colour).
        VkImageCreateInfo normalMsaaInfo = colorInfo;
        normalMsaaInfo.format = NORMAL_FORMAT;
        if (vmaCreateImage(m_device.GetAllocator(), &normalMsaaInfo, &allocInfo,
                           &m_normalMsaaImage, &m_normalMsaaAllocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA normal image");
        }

        VkImageViewCreateInfo normalMsaaView = colorView;
        normalMsaaView.image = m_normalMsaaImage;
        normalMsaaView.format = NORMAL_FORMAT;
        if (vkCreateImageView(m_device.GetDevice(), &normalMsaaView, nullptr,
                              &m_normalMsaaImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA normal image view");
        }

        // Multisampled outline-stencil G-buffer (transient, resolved into the
        // per-swap-image stencil target each frame).
        VkImageCreateInfo stencilMsaaInfo = colorInfo;
        stencilMsaaInfo.format = STENCIL_FORMAT;
        if (vmaCreateImage(m_device.GetAllocator(), &stencilMsaaInfo, &allocInfo,
                           &m_stencilMsaaImage, &m_stencilMsaaAllocation,
                           nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA stencil image");
        }

        VkImageViewCreateInfo stencilMsaaView = colorView;
        stencilMsaaView.image = m_stencilMsaaImage;
        stencilMsaaView.format = STENCIL_FORMAT;
        if (vkCreateImageView(m_device.GetDevice(), &stencilMsaaView, nullptr,
                              &m_stencilMsaaImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create MSAA stencil image view");
        }
    }
}

void RenderLoop::CreateFramebuffers() {
    const auto& swapImageViews = m_swapchain.GetImageViews();
    VkExtent2D extent = m_swapchain.GetExtent();
    const bool multisampled = m_samples != VK_SAMPLE_COUNT_1_BIT;

    m_framebuffers.resize(swapImageViews.size());
    m_presentFramebuffers.resize(swapImageViews.size());

    for (size_t i = 0; i < swapImageViews.size(); ++i) {
        // Main (HDR) framebuffer — attachment order must match CreateRenderPass:
        //   non-MSAA: [hdr, normal, stencil, depth]
        //   MSAA:     [hdr MSAA, normal MSAA, stencil MSAA, depth,
        //              hdr resolve, normal resolve, stencil resolve]
        std::array<VkImageView, 7> attachments{};
        uint32_t count = 0;
        if (multisampled) {
            attachments[0] = m_colorImageView;
            attachments[1] = m_normalMsaaImageView;
            attachments[2] = m_stencilMsaaImageView;
            attachments[3] = m_depthImageView;
            attachments[4] = m_hdrImageViews[i];
            attachments[5] = m_normalImageViews[i];
            attachments[6] = m_stencilImageViews[i];
            count = 7;
        } else {
            attachments[0] = m_hdrImageViews[i];
            attachments[1] = m_normalImageViews[i];
            attachments[2] = m_stencilImageViews[i];
            attachments[3] = m_depthImageView;
            count = 4;
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_renderPass;
        fbInfo.attachmentCount = count;
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(m_device.GetDevice(), &fbInfo, nullptr, &m_framebuffers[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        // Present/tonemap framebuffer — single swapchain colour attachment.
        VkFramebufferCreateInfo presentFb{};
        presentFb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        presentFb.renderPass = m_presentRenderPass;
        presentFb.attachmentCount = 1;
        presentFb.pAttachments = &swapImageViews[i];
        presentFb.width = extent.width;
        presentFb.height = extent.height;
        presentFb.layers = 1;

        if (vkCreateFramebuffer(m_device.GetDevice(), &presentFb, nullptr,
                                &m_presentFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create present framebuffer");
        }
    }
}

void RenderLoop::CreateTonemapPipeline() {
    m_tonemapVert = std::make_unique<Shader>(m_device, ShaderStage::Vertex,
                                             m_shaderDir + "/tonemap.vert.spv");
    m_tonemapFrag = std::make_unique<Shader>(m_device, ShaderStage::Fragment,
                                             m_shaderDir + "/tonemap.frag.spv");
    m_tonemapPipeline = std::make_unique<TonemapPipeline>(
        m_device, *m_tonemapVert, *m_tonemapFrag, m_presentRenderPass,
        m_tonemapSetLayout, VK_SAMPLE_COUNT_1_BIT);
}

void RenderLoop::CreateTonemapDescriptors() {
    // Sampler for the HDR resolve target — nearest filtering is fine since the
    // tonemap pass samples 1:1 onto the swapchain.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 0.0F;
    if (vkCreateSampler(m_device.GetDevice(), &samplerInfo, nullptr, &m_hdrSampler) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create HDR sampler");
    }

    // Two bindings: 0 = HDR colour resolve, 1 = post-blur AO (half-res R8).
    // tonemap.frag multiplies the AO scalar into the HDR colour before ACES.
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(m_device.GetDevice(), &layoutInfo, nullptr,
                                    &m_tonemapSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tonemap descriptor set layout");
    }

    // One descriptor set per swapchain image (each points at its matching HDR view).
    uint32_t imageCount = m_swapchain.GetImageCount();
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = imageCount * 2;  // HDR + AO per set

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = imageCount;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(m_device.GetDevice(), &poolInfo, nullptr,
                               &m_tonemapDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tonemap descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(imageCount, m_tonemapSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_tonemapDescriptorPool;
    allocInfo.descriptorSetCount = imageCount;
    allocInfo.pSetLayouts = layouts.data();
    m_tonemapDescriptorSets.assign(imageCount, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(m_device.GetDevice(), &allocInfo,
                                 m_tonemapDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate tonemap descriptor sets");
    }
}

void RenderLoop::UpdateTonemapDescriptors() {
    for (uint32_t i = 0; i < m_tonemapDescriptorSets.size(); ++i) {
        VkDescriptorImageInfo hdrInfo{};
        hdrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        hdrInfo.imageView = m_hdrImageViews[i];
        hdrInfo.sampler = m_hdrSampler;

        // AO binding — may be null on first call (pool created before SSAO
        // resources exist); ctor/resize path re-runs UpdateTonemapDescriptors
        // after CreateSsaoResources + ClearAoImagesToWhite so the binding
        // ends up pointing at a valid SHADER_READ_ONLY_OPTIMAL image before
        // the first frame dispatches.
        VkDescriptorImageInfo aoInfo{};
        aoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        aoInfo.imageView = i < m_aoViewsA.size() ? m_aoViewsA[i] : VK_NULL_HANDLE;
        aoInfo.sampler = m_aoSampler;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_tonemapDescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &hdrInfo;

        uint32_t writeCount = 1;
        if (aoInfo.imageView != VK_NULL_HANDLE && aoInfo.sampler != VK_NULL_HANDLE) {
            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_tonemapDescriptorSets[i];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo = &aoInfo;
            writeCount = 2;
        }
        vkUpdateDescriptorSets(m_device.GetDevice(), writeCount, writes.data(), 0, nullptr);
    }
}

void RenderLoop::CleanupFrameResources() {
    VkDevice device = m_device.GetDevice();

    CleanupSsaoResources();
    CleanupDepthPyramidResources();

    for (auto fb : m_framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_framebuffers.clear();

    for (auto fb : m_presentFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_presentFramebuffers.clear();

    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }

    if (m_depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_depthImage, m_depthAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthAllocation = VK_NULL_HANDLE;
    }

    if (m_colorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_colorImageView, nullptr);
        m_colorImageView = VK_NULL_HANDLE;
    }

    if (m_colorImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_colorImage, m_colorAllocation);
        m_colorImage = VK_NULL_HANDLE;
        m_colorAllocation = VK_NULL_HANDLE;
    }

    if (m_normalMsaaImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_normalMsaaImageView, nullptr);
        m_normalMsaaImageView = VK_NULL_HANDLE;
    }

    if (m_normalMsaaImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_normalMsaaImage, m_normalMsaaAllocation);
        m_normalMsaaImage = VK_NULL_HANDLE;
        m_normalMsaaAllocation = VK_NULL_HANDLE;
    }

    if (m_stencilMsaaImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_stencilMsaaImageView, nullptr);
        m_stencilMsaaImageView = VK_NULL_HANDLE;
    }

    if (m_stencilMsaaImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_stencilMsaaImage, m_stencilMsaaAllocation);
        m_stencilMsaaImage = VK_NULL_HANDLE;
        m_stencilMsaaAllocation = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < m_hdrImages.size(); ++i) {
        if (m_hdrImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_hdrImageViews[i], nullptr);
        }
        if (m_hdrImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_hdrImages[i], m_hdrAllocations[i]);
        }
    }
    m_hdrImages.clear();
    m_hdrImageViews.clear();
    m_hdrAllocations.clear();

    for (size_t i = 0; i < m_normalImages.size(); ++i) {
        if (m_normalImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_normalImageViews[i], nullptr);
        }
        if (m_normalImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_normalImages[i], m_normalAllocations[i]);
        }
    }
    m_normalImages.clear();
    m_normalImageViews.clear();
    m_normalAllocations.clear();

    for (size_t i = 0; i < m_stencilImages.size(); ++i) {
        if (m_stencilImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_stencilImageViews[i], nullptr);
        }
        if (m_stencilImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_stencilImages[i],
                            m_stencilAllocations[i]);
        }
    }
    m_stencilImages.clear();
    m_stencilImageViews.clear();
    m_stencilAllocations.clear();
}

void RenderLoop::CleanupTonemapDescriptors() {
    VkDevice device = m_device.GetDevice();
    if (m_tonemapDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_tonemapDescriptorPool, nullptr);
        m_tonemapDescriptorPool = VK_NULL_HANDLE;
    }
    m_tonemapDescriptorSets.clear();
    if (m_tonemapSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_tonemapSetLayout, nullptr);
        m_tonemapSetLayout = VK_NULL_HANDLE;
    }
    if (m_hdrSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_hdrSampler, nullptr);
        m_hdrSampler = VK_NULL_HANDLE;
    }
}

void RenderLoop::Cleanup() {
    VkDevice device = m_device.GetDevice();

    m_tonemapPipeline.reset();
    m_tonemapVert.reset();
    m_tonemapFrag.reset();
    m_depthLinearizePipeline.reset();
    m_depthMipPipeline.reset();
    m_depthLinearizeShader.reset();
    m_depthMipShader.reset();
    m_ssaoPipeline.reset();
    m_ssaoBlurPipeline.reset();
    m_ssaoMainShader.reset();
    m_ssaoBlurShader.reset();

    CleanupFrameResources();
    CleanupTonemapDescriptors();
    CleanupDepthPyramidDescriptors();
    CleanupSsaoDescriptors();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
    }
    if (m_presentRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_presentRenderPass, nullptr);
    }

    for (VkSemaphore sem : m_renderFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }
    m_renderFinishedSemaphores.clear();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device, m_inFlightFences[i], nullptr);
        }
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
    }
}

void RenderLoop::CreateDepthPyramidDescriptors() {
    // Nearest-filter sampler spanning all mips. The compute chain uses
    // texelFetch (unfiltered); downstream SSAO/SSIL will use textureLod
    // with an explicit mip selection, which needs minLod=0/maxLod=mips.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = static_cast<float>(DEPTH_PYRAMID_MIPS);
    if (vkCreateSampler(m_device.GetDevice(), &samplerInfo, nullptr,
                        &m_depthPyramidSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth pyramid sampler");
    }

    // Shared set layout: 0 = COMBINED_IMAGE_SAMPLER (source), 1 = STORAGE_IMAGE (dest r32f).
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(m_device.GetDevice(), &layoutInfo, nullptr,
                                    &m_depthPyramidSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth pyramid descriptor set layout");
    }

    // One set per (swap image, level). 4 levels = 1 linearize + 3 mip downsamples.
    const uint32_t imageCount = m_swapchain.GetImageCount();
    const uint32_t totalSets = imageCount * DEPTH_PYRAMID_MIPS;

    std::array<VkDescriptorPoolSize, 2> sizes{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = totalSets;
    sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[1].descriptorCount = totalSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = totalSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes = sizes.data();
    if (vkCreateDescriptorPool(m_device.GetDevice(), &poolInfo, nullptr,
                               &m_depthPyramidDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth pyramid descriptor pool");
    }

    std::vector<VkDescriptorSetLayout> layouts(totalSets, m_depthPyramidSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_depthPyramidDescriptorPool;
    allocInfo.descriptorSetCount = totalSets;
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> flat(totalSets, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(m_device.GetDevice(), &allocInfo, flat.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth pyramid descriptor sets");
    }

    m_depthPyramidSets.assign(imageCount, std::array<VkDescriptorSet, DEPTH_PYRAMID_MIPS>{});
    for (uint32_t i = 0; i < imageCount; ++i) {
        for (uint32_t level = 0; level < DEPTH_PYRAMID_MIPS; ++level) {
            m_depthPyramidSets[i][level] = flat[(i * DEPTH_PYRAMID_MIPS) + level];
        }
    }
}

void RenderLoop::CreateDepthPyramidPipelines() {
    m_depthLinearizeShader = std::make_unique<Shader>(
        m_device, ShaderStage::Compute, m_shaderDir + "/depth_linearize.comp.spv");
    m_depthMipShader = std::make_unique<Shader>(
        m_device, ShaderStage::Compute, m_shaderDir + "/depth_mip.comp.spv");
    m_depthLinearizePipeline = std::make_unique<DepthLinearizePipeline>(
        m_device, *m_depthLinearizeShader, m_depthPyramidSetLayout);
    m_depthMipPipeline = std::make_unique<DepthMipPipeline>(
        m_device, *m_depthMipShader, m_depthPyramidSetLayout);
}

void RenderLoop::CreateDepthPyramidResources() {
    VkExtent2D extent = m_swapchain.GetExtent();
    const uint32_t imageCount = m_swapchain.GetImageCount();
    m_depthPyramidExtent = extent;

    m_depthPyramidImages.assign(imageCount, VK_NULL_HANDLE);
    m_depthPyramidAllocations.assign(imageCount, VK_NULL_HANDLE);
    m_depthPyramidSampledViews.assign(imageCount, VK_NULL_HANDLE);
    m_depthPyramidMipViews.assign(imageCount, std::array<VkImageView, DEPTH_PYRAMID_MIPS>{});

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = DEPTH_PYRAMID_FORMAT;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = DEPTH_PYRAMID_MIPS;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // STORAGE for compute writes per-mip; SAMPLED for the full-chain
        // view that downstream SSAO/SSIL binds through textureLod.
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo,
                           &m_depthPyramidImages[i], &m_depthPyramidAllocations[i],
                           nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth pyramid image");
        }

        VkImageViewCreateInfo fullView{};
        fullView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        fullView.image = m_depthPyramidImages[i];
        fullView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        fullView.format = DEPTH_PYRAMID_FORMAT;
        fullView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        fullView.subresourceRange.baseMipLevel = 0;
        fullView.subresourceRange.levelCount = DEPTH_PYRAMID_MIPS;
        fullView.subresourceRange.baseArrayLayer = 0;
        fullView.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_device.GetDevice(), &fullView, nullptr,
                              &m_depthPyramidSampledViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth pyramid sampled view");
        }

        for (uint32_t level = 0; level < DEPTH_PYRAMID_MIPS; ++level) {
            VkImageViewCreateInfo mipView = fullView;
            mipView.subresourceRange.baseMipLevel = level;
            mipView.subresourceRange.levelCount = 1;
            if (vkCreateImageView(m_device.GetDevice(), &mipView, nullptr,
                                  &m_depthPyramidMipViews[i][level]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create depth pyramid mip view");
            }
        }
    }
}

void RenderLoop::UpdateDepthPyramidDescriptors() {
    const bool multisampled = m_samples != VK_SAMPLE_COUNT_1_BIT;
    const uint32_t imageCount = static_cast<uint32_t>(m_depthPyramidSets.size());
    for (uint32_t i = 0; i < imageCount; ++i) {
        for (uint32_t level = 0; level < DEPTH_PYRAMID_MIPS; ++level) {
            // Linearize (level == 0): source is the non-linear depth attachment
            // (non-MSAA only). Under MSAA we never dispatch this set but still
            // point it at a valid single-sample view (mip 0) so descriptor
            // validation is happy if the set is ever bound.
            // Downsample (level ≥ 1): source is the previous mip, sampled in GENERAL.
            VkImageView srcView = VK_NULL_HANDLE;
            VkImageLayout srcLayout = VK_IMAGE_LAYOUT_GENERAL;
            if (level == 0) {
                srcView = multisampled ? m_depthPyramidMipViews[i][0] : m_depthImageView;
                srcLayout = multisampled ? VK_IMAGE_LAYOUT_GENERAL
                                         : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else {
                srcView = m_depthPyramidMipViews[i][level - 1];
                srcLayout = VK_IMAGE_LAYOUT_GENERAL;
            }

            VkDescriptorImageInfo srcInfo{};
            srcInfo.imageLayout = srcLayout;
            srcInfo.imageView = srcView;
            srcInfo.sampler = m_depthPyramidSampler;

            VkDescriptorImageInfo dstInfo{};
            dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            dstInfo.imageView = m_depthPyramidMipViews[i][level];

            std::array<VkWriteDescriptorSet, 2> writes{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = m_depthPyramidSets[i][level];
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].pImageInfo = &srcInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_depthPyramidSets[i][level];
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].pImageInfo = &dstInfo;

            vkUpdateDescriptorSets(m_device.GetDevice(),
                                   static_cast<uint32_t>(writes.size()), writes.data(),
                                   0, nullptr);
        }
    }
}

void RenderLoop::BuildDepthPyramid(VkCommandBuffer cmd) {
    // MSAA path gated off — depth_linearize.comp wants sampler2D, not
    // sampler2DMS. Revisit when SSAO (RP.5) needs MSAA support.
    if (m_samples != VK_SAMPLE_COUNT_1_BIT) {
        return;
    }

    const uint32_t i = m_currentImageIndex;
    const VkExtent2D extent = m_depthPyramidExtent;

    // All mips UNDEFINED → GENERAL for this frame's writes. Prior contents
    // are discardable — every mip is fully overwritten below.
    VkImageMemoryBarrier pre{};
    pre.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pre.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pre.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    pre.srcAccessMask = 0;
    pre.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    pre.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre.image = m_depthPyramidImages[i];
    pre.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    pre.subresourceRange.baseMipLevel = 0;
    pre.subresourceRange.levelCount = DEPTH_PYRAMID_MIPS;
    pre.subresourceRange.baseArrayLayer = 0;
    pre.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &pre);

    // Linearize depth → pyramid mip 0.
    m_depthLinearizePipeline->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_depthLinearizePipeline->GetLayout(), 0, 1,
                            &m_depthPyramidSets[i][0], 0, nullptr);
    DepthLinearizePipeline::PushConstants push{m_nearZ, m_farZ};
    vkCmdPushConstants(cmd, m_depthLinearizePipeline->GetLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);
    vkCmdDispatch(cmd, (extent.width + 7) / 8, (extent.height + 7) / 8, 1);

    // Mip chain: each dispatch reads mip N-1 and writes mip N. One W→R
    // barrier per level on the source mip is sufficient.
    m_depthMipPipeline->Bind(cmd);
    for (uint32_t level = 1; level < DEPTH_PYRAMID_MIPS; ++level) {
        VkImageMemoryBarrier srcBarrier = pre;
        srcBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        srcBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        srcBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcBarrier.subresourceRange.baseMipLevel = level - 1;
        srcBarrier.subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &srcBarrier);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_depthMipPipeline->GetLayout(), 0, 1,
                                &m_depthPyramidSets[i][level], 0, nullptr);
        const uint32_t dstW = std::max(1U, extent.width >> level);
        const uint32_t dstH = std::max(1U, extent.height >> level);
        vkCmdDispatch(cmd, (dstW + 7) / 8, (dstH + 7) / 8, 1);
    }

    // Publish all mips to fragment-shader reads for downstream consumers
    // (SSAO/SSIL). No-op today but keeps the barrier correct when RP.5
    // lands — avoids a silent hazard.
    VkImageMemoryBarrier post = pre;
    post.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    post.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    post.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    post.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    post.subresourceRange.baseMipLevel = 0;
    post.subresourceRange.levelCount = DEPTH_PYRAMID_MIPS;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &post);
}

void RenderLoop::CleanupDepthPyramidResources() {
    VkDevice device = m_device.GetDevice();
    for (size_t i = 0; i < m_depthPyramidImages.size(); ++i) {
        for (auto& mipView : m_depthPyramidMipViews[i]) {
            if (mipView != VK_NULL_HANDLE) {
                vkDestroyImageView(device, mipView, nullptr);
                mipView = VK_NULL_HANDLE;
            }
        }
        if (m_depthPyramidSampledViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_depthPyramidSampledViews[i], nullptr);
        }
        if (m_depthPyramidImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_depthPyramidImages[i],
                            m_depthPyramidAllocations[i]);
        }
    }
    m_depthPyramidImages.clear();
    m_depthPyramidAllocations.clear();
    m_depthPyramidSampledViews.clear();
    m_depthPyramidMipViews.clear();
}

void RenderLoop::CleanupDepthPyramidDescriptors() {
    VkDevice device = m_device.GetDevice();
    if (m_depthPyramidDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_depthPyramidDescriptorPool, nullptr);
        m_depthPyramidDescriptorPool = VK_NULL_HANDLE;
    }
    m_depthPyramidSets.clear();
    if (m_depthPyramidSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_depthPyramidSetLayout, nullptr);
        m_depthPyramidSetLayout = VK_NULL_HANDLE;
    }
    if (m_depthPyramidSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_depthPyramidSampler, nullptr);
        m_depthPyramidSampler = VK_NULL_HANDLE;
    }
}

namespace {

struct SsaoUboLayout {
    glm::mat4 proj;
    glm::mat4 invProj;
    glm::vec4 kernel[SsaoPipeline::kKernelSize];
};

}  // namespace

void RenderLoop::CreateSsaoDescriptors() {
    // Linear sampler for the half-res AO target — tonemap samples at full
    // res so bilinear upsample keeps the AO edges smooth. Also used for the
    // blur's AO input; the blur uses texelFetch (unfiltered) internally so
    // filter mode doesn't matter for blur, but edge-clamp is critical.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0F;
    samplerInfo.maxLod = 0.0F;
    if (vkCreateSampler(m_device.GetDevice(), &samplerInfo, nullptr, &m_aoSampler) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create AO sampler");
    }

    // Main set layout: depth pyramid (CIS), normal G-buffer (CIS),
    // SsaoUbo (UBO), AO output (STORAGE_IMAGE). Matches SsaoPipeline docs.
    std::array<VkDescriptorSetLayoutBinding, 4> mainBindings{};
    mainBindings[0].binding = 0;
    mainBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    mainBindings[0].descriptorCount = 1;
    mainBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    mainBindings[1].binding = 1;
    mainBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    mainBindings[1].descriptorCount = 1;
    mainBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    mainBindings[2].binding = 2;
    mainBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    mainBindings[2].descriptorCount = 1;
    mainBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    mainBindings[3].binding = 3;
    mainBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    mainBindings[3].descriptorCount = 1;
    mainBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo mainLayoutInfo{};
    mainLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    mainLayoutInfo.bindingCount = static_cast<uint32_t>(mainBindings.size());
    mainLayoutInfo.pBindings = mainBindings.data();
    if (vkCreateDescriptorSetLayout(m_device.GetDevice(), &mainLayoutInfo, nullptr,
                                    &m_ssaoMainSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO main descriptor set layout");
    }

    // Blur set layout: AO input (CIS), depth pyramid (CIS), AO output (STORAGE_IMAGE).
    std::array<VkDescriptorSetLayoutBinding, 3> blurBindings{};
    blurBindings[0].binding = 0;
    blurBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurBindings[0].descriptorCount = 1;
    blurBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    blurBindings[1].binding = 1;
    blurBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurBindings[1].descriptorCount = 1;
    blurBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    blurBindings[2].binding = 2;
    blurBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    blurBindings[2].descriptorCount = 1;
    blurBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = static_cast<uint32_t>(blurBindings.size());
    blurLayoutInfo.pBindings = blurBindings.data();
    if (vkCreateDescriptorSetLayout(m_device.GetDevice(), &blurLayoutInfo, nullptr,
                                    &m_ssaoBlurSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO blur descriptor set layout");
    }

    // Pool: per swap image, 1 main set + 2 blur sets (H, V). Sized from
    // the per-set descriptor types.
    const uint32_t imageCount = m_swapchain.GetImageCount();
    const uint32_t mainSets = imageCount;
    const uint32_t blurSets = imageCount * 2;
    const uint32_t totalSets = mainSets + blurSets;

    std::array<VkDescriptorPoolSize, 3> sizes{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = (mainSets * 2) + (blurSets * 2);  // main: 2 CIS; blur: 2 CIS
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = mainSets;
    sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sizes[2].descriptorCount = mainSets + blurSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = totalSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes = sizes.data();
    if (vkCreateDescriptorPool(m_device.GetDevice(), &poolInfo, nullptr,
                               &m_ssaoDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO descriptor pool");
    }

    // Allocate: main sets first, then blur H, then blur V.
    std::vector<VkDescriptorSetLayout> mainLayouts(imageCount, m_ssaoMainSetLayout);
    VkDescriptorSetAllocateInfo mainAlloc{};
    mainAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    mainAlloc.descriptorPool = m_ssaoDescriptorPool;
    mainAlloc.descriptorSetCount = imageCount;
    mainAlloc.pSetLayouts = mainLayouts.data();
    m_ssaoMainSets.assign(imageCount, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(m_device.GetDevice(), &mainAlloc,
                                 m_ssaoMainSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate SSAO main descriptor sets");
    }

    std::vector<VkDescriptorSetLayout> blurLayouts(imageCount, m_ssaoBlurSetLayout);
    VkDescriptorSetAllocateInfo blurAlloc{};
    blurAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    blurAlloc.descriptorPool = m_ssaoDescriptorPool;
    blurAlloc.descriptorSetCount = imageCount;
    blurAlloc.pSetLayouts = blurLayouts.data();
    m_ssaoBlurSetsH.assign(imageCount, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(m_device.GetDevice(), &blurAlloc,
                                 m_ssaoBlurSetsH.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate SSAO blur H descriptor sets");
    }
    m_ssaoBlurSetsV.assign(imageCount, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(m_device.GetDevice(), &blurAlloc,
                                 m_ssaoBlurSetsV.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate SSAO blur V descriptor sets");
    }
}

void RenderLoop::CreateSsaoPipelines() {
    m_ssaoMainShader = std::make_unique<Shader>(
        m_device, ShaderStage::Compute, m_shaderDir + "/ssao_main.comp.spv");
    m_ssaoBlurShader = std::make_unique<Shader>(
        m_device, ShaderStage::Compute, m_shaderDir + "/ssao_blur.comp.spv");
    m_ssaoPipeline = std::make_unique<SsaoPipeline>(
        m_device, *m_ssaoMainShader, m_ssaoMainSetLayout);
    m_ssaoBlurPipeline = std::make_unique<SsaoBlurPipeline>(
        m_device, *m_ssaoBlurShader, m_ssaoBlurSetLayout);
}

void RenderLoop::CreateSsaoResources() {
    VkExtent2D fullExtent = m_swapchain.GetExtent();
    const uint32_t imageCount = m_swapchain.GetImageCount();
    // Half-res, rounded up so odd swapchain sizes still produce a valid
    // non-zero extent and every full-res pixel has a corresponding AO tap.
    m_aoExtent = {
        std::max(1U, (fullExtent.width + 1U) / 2U),
        std::max(1U, (fullExtent.height + 1U) / 2U)};

    m_aoImagesA.assign(imageCount, VK_NULL_HANDLE);
    m_aoImagesB.assign(imageCount, VK_NULL_HANDLE);
    m_aoAllocationsA.assign(imageCount, VK_NULL_HANDLE);
    m_aoAllocationsB.assign(imageCount, VK_NULL_HANDLE);
    m_aoViewsA.assign(imageCount, VK_NULL_HANDLE);
    m_aoViewsB.assign(imageCount, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = AO_FORMAT;
        imageInfo.extent = {m_aoExtent.width, m_aoExtent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // STORAGE for compute writes; SAMPLED for the blur + tonemap reads;
        // TRANSFER_DST for the init-time vkCmdClearColorImage that seeds
        // AO A at 1.0 under MSAA (so tonemap's multiply is a no-op when
        // SSAO is gated off).
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo,
                           &m_aoImagesA[i], &m_aoAllocationsA[i], nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create AO A image");
        }
        if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo,
                           &m_aoImagesB[i], &m_aoAllocationsB[i], nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create AO B image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = AO_FORMAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        viewInfo.image = m_aoImagesA[i];
        if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr,
                              &m_aoViewsA[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create AO A view");
        }
        viewInfo.image = m_aoImagesB[i];
        if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr,
                              &m_aoViewsB[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create AO B view");
        }
    }

    // UBO per swap image — proj/invProj refresh each frame, kernel once.
    m_ssaoUbos.clear();
    m_ssaoUbos.reserve(imageCount);
    const auto kernel = GenerateHemisphereKernel(SsaoPipeline::kKernelSize, 0);
    for (uint32_t i = 0; i < imageCount; ++i) {
        auto ubo = std::make_unique<Buffer>(
            m_device, BufferType::Uniform, sizeof(SsaoUboLayout), nullptr);
        auto* mapped = static_cast<SsaoUboLayout*>(ubo->Map());
        mapped->proj = m_proj;
        mapped->invProj = glm::inverse(m_proj);
        for (std::size_t k = 0; k < kernel.size(); ++k) {
            mapped->kernel[k] = glm::vec4(kernel[k], 0.0F);
        }
        ubo->Unmap();
        m_ssaoUbos.push_back(std::move(ubo));
    }
}

void RenderLoop::UpdateSsaoDescriptors() {
    const uint32_t imageCount = static_cast<uint32_t>(m_ssaoMainSets.size());
    for (uint32_t i = 0; i < imageCount; ++i) {
        // --- Main set: pyramid(CIS), normal(CIS), ubo(UBO), aoA(STORAGE_IMAGE).
        VkDescriptorImageInfo pyramidInfo{};
        pyramidInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;  // pyramid stays in GENERAL across compute reads
        pyramidInfo.imageView = m_depthPyramidSampledViews[i];
        pyramidInfo.sampler = m_depthPyramidSampler;

        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = m_normalImageViews[i];
        normalInfo.sampler = m_depthPyramidSampler;  // nearest/clamp — fine for the oct-packed normal

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = m_ssaoUbos[i]->GetBuffer();
        uboInfo.offset = 0;
        uboInfo.range = sizeof(SsaoUboLayout);

        VkDescriptorImageInfo aoAOutInfo{};
        aoAOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        aoAOutInfo.imageView = m_aoViewsA[i];

        std::array<VkWriteDescriptorSet, 4> mainWrites{};
        mainWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        mainWrites[0].dstSet = m_ssaoMainSets[i];
        mainWrites[0].dstBinding = 0;
        mainWrites[0].descriptorCount = 1;
        mainWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mainWrites[0].pImageInfo = &pyramidInfo;
        mainWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        mainWrites[1].dstSet = m_ssaoMainSets[i];
        mainWrites[1].dstBinding = 1;
        mainWrites[1].descriptorCount = 1;
        mainWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mainWrites[1].pImageInfo = &normalInfo;
        mainWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        mainWrites[2].dstSet = m_ssaoMainSets[i];
        mainWrites[2].dstBinding = 2;
        mainWrites[2].descriptorCount = 1;
        mainWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mainWrites[2].pBufferInfo = &uboInfo;
        mainWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        mainWrites[3].dstSet = m_ssaoMainSets[i];
        mainWrites[3].dstBinding = 3;
        mainWrites[3].descriptorCount = 1;
        mainWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        mainWrites[3].pImageInfo = &aoAOutInfo;
        vkUpdateDescriptorSets(m_device.GetDevice(),
                               static_cast<uint32_t>(mainWrites.size()), mainWrites.data(),
                               0, nullptr);

        // --- Blur H: read A (GENERAL), depth pyramid, write B.
        VkDescriptorImageInfo aoA_InGeneral{};
        aoA_InGeneral.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        aoA_InGeneral.imageView = m_aoViewsA[i];
        aoA_InGeneral.sampler = m_aoSampler;

        VkDescriptorImageInfo aoBOut{};
        aoBOut.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        aoBOut.imageView = m_aoViewsB[i];

        std::array<VkWriteDescriptorSet, 3> hWrites{};
        hWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        hWrites[0].dstSet = m_ssaoBlurSetsH[i];
        hWrites[0].dstBinding = 0;
        hWrites[0].descriptorCount = 1;
        hWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        hWrites[0].pImageInfo = &aoA_InGeneral;
        hWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        hWrites[1].dstSet = m_ssaoBlurSetsH[i];
        hWrites[1].dstBinding = 1;
        hWrites[1].descriptorCount = 1;
        hWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        hWrites[1].pImageInfo = &pyramidInfo;
        hWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        hWrites[2].dstSet = m_ssaoBlurSetsH[i];
        hWrites[2].dstBinding = 2;
        hWrites[2].descriptorCount = 1;
        hWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        hWrites[2].pImageInfo = &aoBOut;
        vkUpdateDescriptorSets(m_device.GetDevice(),
                               static_cast<uint32_t>(hWrites.size()), hWrites.data(),
                               0, nullptr);

        // --- Blur V: read B (GENERAL), depth pyramid, write A.
        VkDescriptorImageInfo aoB_InGeneral{};
        aoB_InGeneral.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        aoB_InGeneral.imageView = m_aoViewsB[i];
        aoB_InGeneral.sampler = m_aoSampler;

        VkDescriptorImageInfo aoAOutV{};
        aoAOutV.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        aoAOutV.imageView = m_aoViewsA[i];

        std::array<VkWriteDescriptorSet, 3> vWrites{};
        vWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vWrites[0].dstSet = m_ssaoBlurSetsV[i];
        vWrites[0].dstBinding = 0;
        vWrites[0].descriptorCount = 1;
        vWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        vWrites[0].pImageInfo = &aoB_InGeneral;
        vWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vWrites[1].dstSet = m_ssaoBlurSetsV[i];
        vWrites[1].dstBinding = 1;
        vWrites[1].descriptorCount = 1;
        vWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        vWrites[1].pImageInfo = &pyramidInfo;
        vWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vWrites[2].dstSet = m_ssaoBlurSetsV[i];
        vWrites[2].dstBinding = 2;
        vWrites[2].descriptorCount = 1;
        vWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        vWrites[2].pImageInfo = &aoAOutV;
        vkUpdateDescriptorSets(m_device.GetDevice(),
                               static_cast<uint32_t>(vWrites.size()), vWrites.data(),
                               0, nullptr);
    }
}

void RenderLoop::ClearAoImagesToWhite() {
    // One-shot command: transition every AO image UNDEFINED → TRANSFER_DST,
    // clear to (1,0,0,0) so the multiply-composite is a no-op when SSAO is
    // gated off (MSAA), then transition to SHADER_READ_ONLY_OPTIMAL so the
    // tonemap descriptor can sample it immediately.
    if (m_aoImagesA.empty()) {
        return;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate AO-clear command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    std::vector<VkImage> allImages;
    allImages.reserve(m_aoImagesA.size() + m_aoImagesB.size());
    for (VkImage img : m_aoImagesA) { allImages.push_back(img); }
    for (VkImage img : m_aoImagesB) { allImages.push_back(img); }

    std::vector<VkImageMemoryBarrier> toTransferDst;
    toTransferDst.reserve(allImages.size());
    for (VkImage img : allImages) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.baseMipLevel = 0;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount = 1;
        toTransferDst.push_back(b);
    }
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(toTransferDst.size()), toTransferDst.data());

    VkClearColorValue clearValue{};
    clearValue.float32[0] = 1.0F;
    clearValue.float32[1] = 0.0F;
    clearValue.float32[2] = 0.0F;
    clearValue.float32[3] = 0.0F;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    for (VkImage img : allImages) {
        vkCmdClearColorImage(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clearValue, 1, &range);
    }

    // AO A → SHADER_READ_ONLY_OPTIMAL (sampled by tonemap frag).
    // AO B → GENERAL (next frame's H-blur expects it in GENERAL as its CIS
    // source and STORAGE_IMAGE dest — both reachable from GENERAL).
    std::vector<VkImageMemoryBarrier> finalBarriers;
    finalBarriers.reserve(allImages.size());
    for (size_t i = 0; i < m_aoImagesA.size(); ++i) {
        VkImageMemoryBarrier b = toTransferDst[i];
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        finalBarriers.push_back(b);
    }
    for (size_t i = 0; i < m_aoImagesB.size(); ++i) {
        VkImageMemoryBarrier b = toTransferDst[m_aoImagesA.size() + i];
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        finalBarriers.push_back(b);
    }
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(finalBarriers.size()), finalBarriers.data());

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_device.GetGraphicsQueue());
    vkFreeCommandBuffers(m_device.GetDevice(), m_commandPool, 1, &cmd);
}

void RenderLoop::UploadSsaoUbo(uint32_t imageIndex) {
    if (imageIndex >= m_ssaoUbos.size()) {
        return;
    }
    // Only the matrices change per frame — kernel stays from the ctor write.
    auto* mapped = static_cast<SsaoUboLayout*>(m_ssaoUbos[imageIndex]->Map());
    mapped->proj = m_proj;
    mapped->invProj = glm::inverse(m_proj);
    m_ssaoUbos[imageIndex]->Unmap();
}

void RenderLoop::RunSsao(VkCommandBuffer cmd) {
    // MSAA path: SSAO inputs (pyramid, normal G-buffer) are either gated off
    // or resolved in shapes the compute shaders don't support cleanly.
    // Tonemap samples the pre-cleared (1.0) AO so the multiply is a no-op.
    if (m_samples != VK_SAMPLE_COUNT_1_BIT) {
        return;
    }

    const uint32_t i = m_currentImageIndex;
    UploadSsaoUbo(i);

    // Transition AO A from SHADER_READ_ONLY (tonemap ended last frame) → GENERAL for compute write.
    // Transition AO B from GENERAL (prior frame's V-blur output layout) → GENERAL (no-op, but still barrier for W→R/R→W hazards).
    // On the very first frame after ClearAoImagesToWhite, A is in SHADER_READ_ONLY and B is in GENERAL; same story.
    {
        std::array<VkImageMemoryBarrier, 2> barriers{};
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = m_aoImagesA[i];
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.layerCount = 1;

        barriers[1] = barriers[0];
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[1].image = m_aoImagesB[i];
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
            static_cast<uint32_t>(barriers.size()), barriers.data());
    }

    // --- Main SSAO: depth pyramid + normal → AO A.
    m_ssaoPipeline->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_ssaoPipeline->GetLayout(), 0, 1,
                            &m_ssaoMainSets[i], 0, nullptr);
    SsaoPipeline::PushConstants mainPush{0.5F, 0.025F, 1.0F, 1.5F};
    vkCmdPushConstants(cmd, m_ssaoPipeline->GetLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(mainPush), &mainPush);
    vkCmdDispatch(cmd, (m_aoExtent.width + 7) / 8, (m_aoExtent.height + 7) / 8, 1);

    // Barrier: AO A write → AO A read (H blur's CIS source).
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = m_aoImagesA[i];
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             1, &b);
    }

    // --- Blur H: A → B.
    m_ssaoBlurPipeline->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_ssaoBlurPipeline->GetLayout(), 0, 1,
                            &m_ssaoBlurSetsH[i], 0, nullptr);
    SsaoBlurPipeline::PushConstants hPush{1, 0, 4.0F};
    vkCmdPushConstants(cmd, m_ssaoBlurPipeline->GetLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(hPush), &hPush);
    vkCmdDispatch(cmd, (m_aoExtent.width + 7) / 8, (m_aoExtent.height + 7) / 8, 1);

    // Barrier: AO B write → AO B read (V blur source).
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = m_aoImagesB[i];
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             1, &b);
    }

    // Before the V-blur writes AO A again, make its prior read (H-blur
    // source) available to the coming write. Without this, the H-blur read
    // and V-blur write of A would be a RAW hazard — currently hidden only
    // because the H-blur samples A but writes B.
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = m_aoImagesA[i];
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             1, &b);
    }

    // --- Blur V: B → A.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_ssaoBlurPipeline->GetLayout(), 0, 1,
                            &m_ssaoBlurSetsV[i], 0, nullptr);
    SsaoBlurPipeline::PushConstants vPush{0, 1, 4.0F};
    vkCmdPushConstants(cmd, m_ssaoBlurPipeline->GetLayout(), VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(vPush), &vPush);
    vkCmdDispatch(cmd, (m_aoExtent.width + 7) / 8, (m_aoExtent.height + 7) / 8, 1);

    // Final: AO A → SHADER_READ_ONLY_OPTIMAL for the tonemap fragment sample.
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = m_aoImagesA[i];
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             1, &b);
    }
}

void RenderLoop::CleanupSsaoResources() {
    VkDevice device = m_device.GetDevice();
    for (size_t i = 0; i < m_aoViewsA.size(); ++i) {
        if (m_aoViewsA[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_aoViewsA[i], nullptr);
        }
        if (m_aoImagesA[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_aoImagesA[i], m_aoAllocationsA[i]);
        }
    }
    for (size_t i = 0; i < m_aoViewsB.size(); ++i) {
        if (m_aoViewsB[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_aoViewsB[i], nullptr);
        }
        if (m_aoImagesB[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_aoImagesB[i], m_aoAllocationsB[i]);
        }
    }
    m_aoViewsA.clear();
    m_aoViewsB.clear();
    m_aoImagesA.clear();
    m_aoImagesB.clear();
    m_aoAllocationsA.clear();
    m_aoAllocationsB.clear();
    m_ssaoUbos.clear();
}

void RenderLoop::CleanupSsaoDescriptors() {
    VkDevice device = m_device.GetDevice();
    if (m_ssaoDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_ssaoDescriptorPool, nullptr);
        m_ssaoDescriptorPool = VK_NULL_HANDLE;
    }
    m_ssaoMainSets.clear();
    m_ssaoBlurSetsH.clear();
    m_ssaoBlurSetsV.clear();
    if (m_ssaoMainSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_ssaoMainSetLayout, nullptr);
        m_ssaoMainSetLayout = VK_NULL_HANDLE;
    }
    if (m_ssaoBlurSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_ssaoBlurSetLayout, nullptr);
        m_ssaoBlurSetLayout = VK_NULL_HANDLE;
    }
    if (m_aoSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_aoSampler, nullptr);
        m_aoSampler = VK_NULL_HANDLE;
    }
}

VkFormat RenderLoop::FindDepthFormat(VkPhysicalDevice physicalDevice) {
    const std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported depth format");
}

}  // namespace bimeup::renderer
