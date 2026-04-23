#include <renderer/RenderLoop.h>
#include <renderer/Buffer.h>
#include <renderer/DepthLinearizePipeline.h>
#include <renderer/DepthMipPipeline.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SmaaLut.h>
#include <renderer/SsaoBlurPipeline.h>
#include <renderer/SsaoXeGtaoPipeline.h>
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
                       const std::string& shaderDir)
    : m_device(device),
      m_swapchain(swapchain),
      m_shaderDir(shaderDir) {
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();
    CreateRenderPass();
    CreatePresentRenderPass();
    CreatePostRenderPass();
    CreateHdrResources();
    CreateDepthResources();
    CreateLdrResources();
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
    UpdateTonemapDescriptors();  // re-run so AO binding points at the freshly created image
    CreateSmaaRenderPass();
    CreateSmaaLuts();
    CreateSmaaDescriptors();
    CreateSmaaPipelines();
    CreateSmaaResources();
    UpdateSmaaDescriptors();
    ClearSmaaChainToZero();

    if (bimeup::tools::Log::GetLogger()) {
        LOG_INFO("RenderLoop created (max {} frames in flight)", MAX_FRAMES_IN_FLIGHT);
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
    //   [2] transparency stencil (0 = opaque), [3] depth,
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
    // transitions it to SHADER_READ_ONLY_OPTIMAL for the tonemap sampler;
    // the depth attachment also transitions to SHADER_READ_ONLY_OPTIMAL so
    // subsequent compute/RT passes can sample it.
    vkCmdEndRenderPass(cmd);

    // RP.4d — build the depth pyramid between the main pass and the
    // tonemap pass. Dispatches linearize then three mip downsamples with
    // barriers.
    BuildDepthPyramid(cmd);

    // RP.5d — SSAO main + separable blur. Produces the half-res AO target
    // that the tonemap fragment samples.
    RunSsao(cmd);

    // Post pass — tonemap fullscreen tri, writes to the per-swap LDR
    // intermediate. Its final layout is SHADER_READ_ONLY_OPTIMAL so the SMAA
    // edge pass (and later the blend pass) samples it without a manual
    // barrier.
    VkRenderPassBeginInfo postInfo{};
    postInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    postInfo.renderPass = m_postRenderPass;
    postInfo.framebuffer = m_postFramebuffers[m_currentImageIndex];
    postInfo.renderArea.offset = {0, 0};
    postInfo.renderArea.extent = extent;
    postInfo.clearValueCount = 0;  // tonemap fullscreen-tri covers every pixel

    vkCmdBeginRenderPass(cmd, &postInfo, VK_SUBPASS_CONTENTS_INLINE);

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
    vkCmdPushConstants(cmd, m_tonemapPipeline->GetLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(TonemapPipeline::PushConstants), &m_exposurePush);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    // RP.11c — SMAA edge + weights passes before the present pass begins,
    // gated by `m_smaaEnabled`. Each writes a per-swap RGBA8 target via the
    // shared SMAA render pass; final layouts are SHADER_READ_ONLY_OPTIMAL
    // so the blend pass inside the present pass samples the weights
    // without a manual barrier. When disabled the passes are skipped and
    // the blend shader's push-constant gate short-circuits to a
    // passthrough of the LDR input.
    if (m_smaaEnabled) {
        RunSmaaEdgeAndWeights(cmd);
    }

    // Present pass — SMAA blend reads the LDR intermediate + weights and
    // writes the swapchain, then the registered in-present callback draws
    // UI (ImGui) on top of the anti-aliased image. Disabling SMAA from the
    // panel clears the blend push-constant `enabled` field; the shader's
    // early-exit returns the LDR sample unchanged (single sample + write,
    // cheap texture copy).
    VkRenderPassBeginInfo presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    presentInfo.renderPass = m_presentRenderPass;
    presentInfo.framebuffer = m_presentFramebuffers[m_currentImageIndex];
    presentInfo.renderArea.offset = {0, 0};
    presentInfo.renderArea.extent = extent;
    presentInfo.clearValueCount = 0;  // SMAA blend fullscreen-tri covers every pixel

    vkCmdBeginRenderPass(cmd, &presentInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    SmaaBlendPipeline::PushConstants blendPush{};
    blendPush.rcpFrame = glm::vec2(1.0F / static_cast<float>(extent.width),
                                   1.0F / static_cast<float>(extent.height));
    blendPush.enabled = m_smaaEnabled ? 1.0F : 0.0F;
    m_smaaBlendPipeline->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_smaaBlendPipeline->GetLayout(), 0, 1,
                            &m_smaaBlendDescriptorSets[m_currentImageIndex], 0, nullptr);
    vkCmdPushConstants(cmd, m_smaaBlendPipeline->GetLayout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(SmaaBlendPipeline::PushConstants), &blendPush);
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

void RenderLoop::SetSmaaParams(bool enabled, float threshold, int maxSearchSteps,
                               int maxSearchStepsDiag) {
    m_smaaEnabled = enabled;
    m_smaaThreshold = threshold;
    m_smaaMaxSearchSteps = maxSearchSteps;
    m_smaaMaxSearchStepsDiag = maxSearchStepsDiag;
}

void RenderLoop::SetExposure(float exposure) {
    m_exposurePush.exposure = exposure;
}

void RenderLoop::SetSsaoParams(float radius, float falloff, float intensity,
                               float shadowPower) {
    m_ssaoRadius = radius;
    m_ssaoFalloff = falloff;
    m_ssaoIntensity = intensity;
    m_ssaoShadowPower = shadowPower;
}

void RenderLoop::RecreateForSwapchain() {
    WaitIdle();
    CleanupFrameResources();
    CreateHdrResources();
    CreateDepthResources();
    CreateLdrResources();
    CreateFramebuffers();
    CreateDepthPyramidResources();
    CreateSsaoResources();
    CreateSmaaResources();
    UpdateDepthPyramidDescriptors();
    UpdateSsaoDescriptors();
    UpdateSmaaDescriptors();
    ClearAoImagesToWhite();
    ClearSmaaChainToZero();
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

    // Attachment layout (MRT — scene colour + oct-packed normal G-buffer +
    // transparency stencil): [0] HDR colour, [1] normal, [2] stencil,
    // [3] depth. Final layouts on the sampled targets are
    // SHADER_READ_ONLY_OPTIMAL so tonemap / SSAO can bind them straight as
    // samplers.

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = HDR_FORMAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = NORMAL_FORMAT;
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription stencilAttachment{};
    stencilAttachment.format = STENCIL_FORMAT;
    stencilAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    stencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    stencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    stencilAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    stencilAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    stencilAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencilAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Keep depth and transition to SHADER_READ_ONLY so the RP.4 linearize
    // compute pass can sample it.
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkAttachmentReference, 3> colorRefs{};
    colorRefs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[2] = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference depthRef{};
    depthRef.attachment = 3;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

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
    // the depth writes themselves available.
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 4> attachments = {
        colorAttachment, normalAttachment, stencilAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
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

        // Normal G-buffer target — single-sample. SAMPLED is required
        // because SSAO later binds this view through a descriptor.
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

        // Transparency stencil G-buffer — same shape as the normal target.
        // SAMPLED so `ssao_xegtao.comp` can read via `usampler2D`.
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
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    // SAMPLED_BIT so the RP.4 depth-linearize compute shader can bind this
    // image as sampler2D post-render-pass.
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
}

void RenderLoop::CreateFramebuffers() {
    const auto& swapImageViews = m_swapchain.GetImageViews();
    VkExtent2D extent = m_swapchain.GetExtent();

    m_framebuffers.resize(swapImageViews.size());
    m_postFramebuffers.resize(swapImageViews.size());
    m_presentFramebuffers.resize(swapImageViews.size());

    for (size_t i = 0; i < swapImageViews.size(); ++i) {
        // Main (HDR) framebuffer — attachment order must match CreateRenderPass:
        //   [hdr, normal, stencil, depth]
        std::array<VkImageView, 4> attachments{};
        attachments[0] = m_hdrImageViews[i];
        attachments[1] = m_normalImageViews[i];
        attachments[2] = m_stencilImageViews[i];
        attachments[3] = m_depthImageView;

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_renderPass;
        fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbInfo.pAttachments = attachments.data();
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(m_device.GetDevice(), &fbInfo, nullptr, &m_framebuffers[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }

        // Post framebuffer — single LDR intermediate colour attachment
        // (sampled by the SMAA edge + blend passes).
        VkFramebufferCreateInfo postFb{};
        postFb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        postFb.renderPass = m_postRenderPass;
        postFb.attachmentCount = 1;
        postFb.pAttachments = &m_ldrImageViews[i];
        postFb.width = extent.width;
        postFb.height = extent.height;
        postFb.layers = 1;

        if (vkCreateFramebuffer(m_device.GetDevice(), &postFb, nullptr,
                                &m_postFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create post framebuffer");
        }

        // Present framebuffer — single swapchain colour attachment (SMAA blend + UI).
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
        m_tonemapSetLayout);
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

    // Two bindings: 0 = HDR colour resolve, 1 = XeGTAO post-blur AO
    // (half-res R8, RP.5d). tonemap.frag multiplies AO into HDR and runs ACES.
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    for (uint32_t i = 0; i < bindings.size(); ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

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
    poolSize.descriptorCount = imageCount * 2;  // HDR + XeGTAO AO per set

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
        const VkImageView xegtaoView =
            i < m_aoViewsA.size() ? m_aoViewsA[i] : VK_NULL_HANDLE;
        VkDescriptorImageInfo aoInfo{};
        aoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        aoInfo.imageView = xegtaoView;
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
            writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[writeCount].dstSet = m_tonemapDescriptorSets[i];
            writes[writeCount].dstBinding = 1;
            writes[writeCount].descriptorCount = 1;
            writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[writeCount].pImageInfo = &aoInfo;
            ++writeCount;
        }
        vkUpdateDescriptorSets(m_device.GetDevice(), writeCount, writes.data(), 0, nullptr);
    }
}

void RenderLoop::CleanupFrameResources() {
    VkDevice device = m_device.GetDevice();

    CleanupSmaaResources();
    CleanupSsaoResources();
    CleanupDepthPyramidResources();

    for (auto fb : m_framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_framebuffers.clear();

    for (auto fb : m_postFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_postFramebuffers.clear();

    for (auto fb : m_presentFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_presentFramebuffers.clear();

    for (size_t i = 0; i < m_ldrImages.size(); ++i) {
        if (m_ldrImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_ldrImageViews[i], nullptr);
        }
        if (m_ldrImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_ldrImages[i], m_ldrAllocations[i]);
        }
    }
    m_ldrImages.clear();
    m_ldrImageViews.clear();
    m_ldrAllocations.clear();

    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }

    if (m_depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_depthImage, m_depthAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthAllocation = VK_NULL_HANDLE;
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

void RenderLoop::CreatePostRenderPass() {
    // Render-pass-compatible with `m_presentRenderPass`: same format (swap),
    // same sample count (1), single colour attachment, no depth. The only
    // operational difference is the final layout — here the image transitions
    // to SHADER_READ_ONLY_OPTIMAL so the SMAA edge / blend passes can
    // sample it without a manual barrier. The tonemap pipeline (built
    // against `m_presentRenderPass`) binds here unchanged per Vulkan
    // render-pass compatibility rules.
    VkAttachmentDescription color{};
    color.format = m_swapchain.GetFormat();
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // tonemap covers every pixel
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Same dep shape as the present pass: HDR/SSAO outputs from prior
    // passes must be visible to the tonemap fragment shader before the
    // colour-write stage begins.
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

    if (vkCreateRenderPass(m_device.GetDevice(), &rpInfo, nullptr, &m_postRenderPass) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create post render pass");
    }
}

void RenderLoop::CreateLdrResources() {
    VkExtent2D extent = m_swapchain.GetExtent();
    uint32_t imageCount = m_swapchain.GetImageCount();

    m_ldrImages.assign(imageCount, VK_NULL_HANDLE);
    m_ldrImageViews.assign(imageCount, VK_NULL_HANDLE);
    m_ldrAllocations.assign(imageCount, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = m_swapchain.GetFormat();
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo, &m_ldrImages[i],
                           &m_ldrAllocations[i], nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create LDR intermediate image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_ldrImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchain.GetFormat();
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr, &m_ldrImageViews[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create LDR intermediate image view");
        }
    }
}

void RenderLoop::CreateSmaaRenderPass() {
    // Shared RGBA8 render pass for the SMAA edge + weights draws. Each
    // pass binds its own framebuffer pointing at a per-swap single-mip
    // RGBA8 view; attachment contents are CLEAR-then-STORE and transition
    // to SHADER_READ_ONLY_OPTIMAL on end so the downstream pass samples
    // the freshly written target without a manual barrier.
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Two deps: (a) prior pass's colour write (the post pass's LDR
    // intermediate, or the edge pass's result before weights reads it)
    // must be visible to the current pass's fragment shader; (b) our
    // colour write must be visible to the subsequent pass's shader read.
    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &color;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies = deps.data();

    if (vkCreateRenderPass(m_device.GetDevice(), &rpInfo, nullptr, &m_smaaRenderPass) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create SMAA render pass");
    }
}

namespace {

struct SmaaLutUploadJob {
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
};

SmaaLutUploadJob CreateAndUploadSmaaLut(const Device& device, VkCommandPool cmdPool,
                                        VkQueue queue, VkFormat format, uint32_t width,
                                        uint32_t height, const unsigned char* data,
                                        std::size_t sizeBytes) {
    SmaaLutUploadJob out{};

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &out.image,
                       &out.allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SMAA LUT image");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device.GetDevice(), &viewInfo, nullptr, &out.view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SMAA LUT view");
    }

    // Staging buffer — HOST_VISIBLE + TRANSFER_SRC. VMA_MEMORY_USAGE_AUTO
    // with HOST_ACCESS_SEQUENTIAL_WRITE_BIT lets VMA pick the right memory
    // type (usually HOST_VISIBLE + HOST_COHERENT).
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sizeBytes;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAlloc{};
    stagingAlloc.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAlloc.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                         VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VmaAllocation stagingMem = VK_NULL_HANDLE;
    VmaAllocationInfo stagingInfo{};
    if (vmaCreateBuffer(device.GetAllocator(), &bufInfo, &stagingAlloc, &stagingBuf,
                        &stagingMem, &stagingInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SMAA LUT staging buffer");
    }
    std::memcpy(stagingInfo.pMappedData, data, sizeBytes);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device.GetDevice(), &cmdAlloc, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate SMAA LUT upload cmd buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.image = out.image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toTransfer);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuf, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);

    VkImageMemoryBarrier toShader{};
    toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShader.image = out.image;
    toShader.subresourceRange = toTransfer.subresourceRange;
    toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toShader);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device.GetDevice(), cmdPool, 1, &cmd);
    vmaDestroyBuffer(device.GetAllocator(), stagingBuf, stagingMem);

    return out;
}

}  // namespace

void RenderLoop::CreateSmaaLuts() {
    // AreaTex (160×560 RG8) + SearchTex (64×16 R8) from the vendored
    // `iryoku/smaa` submodule via `renderer::SmaaAreaTex::Data()` /
    // `SmaaSearchTex::Data()`. Both upload once per RenderLoop, sampled by
    // `smaa_weights.frag` bindings 1 + 2 through the shared linear
    // CLAMP_TO_EDGE sampler created in `CreateSmaaDescriptors`.
    auto area = CreateAndUploadSmaaLut(
        m_device, m_commandPool, m_device.GetGraphicsQueue(),
        VK_FORMAT_R8G8_UNORM, static_cast<uint32_t>(SmaaAreaTex::kWidth),
        static_cast<uint32_t>(SmaaAreaTex::kHeight), SmaaAreaTex::Data(),
        SmaaAreaTex::kSizeBytes);
    m_smaaAreaImage = area.image;
    m_smaaAreaImageView = area.view;
    m_smaaAreaAllocation = area.allocation;

    auto search = CreateAndUploadSmaaLut(
        m_device, m_commandPool, m_device.GetGraphicsQueue(),
        VK_FORMAT_R8_UNORM, static_cast<uint32_t>(SmaaSearchTex::kWidth),
        static_cast<uint32_t>(SmaaSearchTex::kHeight), SmaaSearchTex::Data(),
        SmaaSearchTex::kSizeBytes);
    m_smaaSearchImage = search.image;
    m_smaaSearchImageView = search.view;
    m_smaaSearchAllocation = search.allocation;
}

void RenderLoop::CreateSmaaDescriptors() {
    // Shared linear sampler with CLAMP_TO_EDGE, maxLod = 0. Used by all
    // three SMAA passes for the LDR input + edges + weights + LUTs — the
    // SMAA algorithm relies on linear filtering (especially for the
    // bilinear-sampling blend trick in `smaa_blend.frag`).
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
    if (vkCreateSampler(m_device.GetDevice(), &samplerInfo, nullptr, &m_smaaSampler) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create SMAA sampler");
    }

    auto makeLayout = [&](uint32_t bindingCount, VkDescriptorSetLayout* out) {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        for (uint32_t i = 0; i < bindingCount; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = bindingCount;
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(m_device.GetDevice(), &layoutInfo, nullptr, out) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create SMAA descriptor set layout");
        }
    };

    // Edge = 1 binding (LDR input). Weights = 3 bindings (edges + AreaTex + SearchTex).
    // Blend = 2 bindings (LDR input + weights).
    makeLayout(1, &m_smaaEdgeSetLayout);
    makeLayout(3, &m_smaaWeightsSetLayout);
    makeLayout(2, &m_smaaBlendSetLayout);

    // Pool sized for per-swap set allocation: edges (1 CIS) + weights (3 CIS) + blend (2 CIS)
    // = 6 CIS per swap image.
    uint32_t imageCount = m_swapchain.GetImageCount();
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = imageCount * 6;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = imageCount * 3;  // edge + weights + blend set per image
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(m_device.GetDevice(), &poolInfo, nullptr,
                               &m_smaaDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SMAA descriptor pool");
    }

    auto allocSets = [&](VkDescriptorSetLayout layout,
                         std::vector<VkDescriptorSet>& sets) {
        std::vector<VkDescriptorSetLayout> layouts(imageCount, layout);
        sets.assign(imageCount, VK_NULL_HANDLE);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_smaaDescriptorPool;
        allocInfo.descriptorSetCount = imageCount;
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(m_device.GetDevice(), &allocInfo, sets.data()) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate SMAA descriptor sets");
        }
    };
    allocSets(m_smaaEdgeSetLayout, m_smaaEdgeDescriptorSets);
    allocSets(m_smaaWeightsSetLayout, m_smaaWeightsDescriptorSets);
    allocSets(m_smaaBlendSetLayout, m_smaaBlendDescriptorSets);
}

void RenderLoop::CreateSmaaPipelines() {
    m_smaaVertShader = std::make_unique<Shader>(m_device, ShaderStage::Vertex,
                                                m_shaderDir + "/smaa.vert.spv");
    m_smaaEdgeFragShader = std::make_unique<Shader>(m_device, ShaderStage::Fragment,
                                                    m_shaderDir + "/smaa_edge.frag.spv");
    m_smaaWeightsFragShader = std::make_unique<Shader>(
        m_device, ShaderStage::Fragment, m_shaderDir + "/smaa_weights.frag.spv");
    m_smaaBlendFragShader = std::make_unique<Shader>(m_device, ShaderStage::Fragment,
                                                     m_shaderDir + "/smaa_blend.frag.spv");
    // Edge + weights pipelines target the shared SMAA render pass; blend
    // pipeline targets the present pass (same shape as FXAA did).
    m_smaaEdgePipeline = std::make_unique<SmaaEdgePipeline>(
        m_device, *m_smaaVertShader, *m_smaaEdgeFragShader, m_smaaRenderPass,
        m_smaaEdgeSetLayout);
    m_smaaWeightsPipeline = std::make_unique<SmaaWeightsPipeline>(
        m_device, *m_smaaVertShader, *m_smaaWeightsFragShader, m_smaaRenderPass,
        m_smaaWeightsSetLayout);
    m_smaaBlendPipeline = std::make_unique<SmaaBlendPipeline>(
        m_device, *m_smaaVertShader, *m_smaaBlendFragShader, m_presentRenderPass,
        m_smaaBlendSetLayout);
}

void RenderLoop::CreateSmaaResources() {
    VkExtent2D extent = m_swapchain.GetExtent();
    uint32_t imageCount = m_swapchain.GetImageCount();

    m_smaaEdgesImages.assign(imageCount, VK_NULL_HANDLE);
    m_smaaEdgesImageViews.assign(imageCount, VK_NULL_HANDLE);
    m_smaaEdgesAllocations.assign(imageCount, VK_NULL_HANDLE);
    m_smaaEdgesFramebuffers.assign(imageCount, VK_NULL_HANDLE);
    m_smaaWeightsImages.assign(imageCount, VK_NULL_HANDLE);
    m_smaaWeightsImageViews.assign(imageCount, VK_NULL_HANDLE);
    m_smaaWeightsAllocations.assign(imageCount, VK_NULL_HANDLE);
    m_smaaWeightsFramebuffers.assign(imageCount, VK_NULL_HANDLE);

    auto makeTarget = [&](VkImage& image, VkImageView& view, VmaAllocation& alloc,
                          VkFramebuffer& fb) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // TRANSFER_DST so ClearSmaaChainToZero can clear the weights
        // target at creation / resize (ensures blend reads deterministic
        // zero until the first enabled frame lands a real weights map).
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo, &image, &alloc,
                           nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create SMAA intermediate image");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create SMAA intermediate view");
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_smaaRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &view;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(m_device.GetDevice(), &fbInfo, nullptr, &fb) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create SMAA framebuffer");
        }
    };

    for (uint32_t i = 0; i < imageCount; ++i) {
        makeTarget(m_smaaEdgesImages[i], m_smaaEdgesImageViews[i],
                   m_smaaEdgesAllocations[i], m_smaaEdgesFramebuffers[i]);
        makeTarget(m_smaaWeightsImages[i], m_smaaWeightsImageViews[i],
                   m_smaaWeightsAllocations[i], m_smaaWeightsFramebuffers[i]);
    }
}

void RenderLoop::UpdateSmaaDescriptors() {
    // Binding plan:
    //   edge set   [0] ← LDR intermediate (SHADER_READ_ONLY_OPTIMAL)
    //   weights set[0] ← edges target      (SHADER_READ_ONLY_OPTIMAL)
    //                [1] ← AreaTex LUT     (SHADER_READ_ONLY_OPTIMAL)
    //                [2] ← SearchTex LUT   (SHADER_READ_ONLY_OPTIMAL)
    //   blend set  [0] ← LDR intermediate
    //              [1] ← weights target    (SHADER_READ_ONLY_OPTIMAL)
    // LUTs are bound from the per-RenderLoop images so they're the same
    // across all swap slots.
    for (uint32_t i = 0; i < m_smaaEdgeDescriptorSets.size(); ++i) {
        if (i >= m_ldrImageViews.size() || m_ldrImageViews[i] == VK_NULL_HANDLE) {
            continue;
        }
        if (i >= m_smaaEdgesImageViews.size() ||
            m_smaaEdgesImageViews[i] == VK_NULL_HANDLE) {
            continue;
        }
        if (i >= m_smaaWeightsImageViews.size() ||
            m_smaaWeightsImageViews[i] == VK_NULL_HANDLE) {
            continue;
        }
        if (m_smaaAreaImageView == VK_NULL_HANDLE ||
            m_smaaSearchImageView == VK_NULL_HANDLE) {
            continue;
        }

        VkDescriptorImageInfo ldrInfo{};
        ldrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ldrInfo.imageView = m_ldrImageViews[i];
        ldrInfo.sampler = m_smaaSampler;

        VkDescriptorImageInfo edgesInfo{};
        edgesInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        edgesInfo.imageView = m_smaaEdgesImageViews[i];
        edgesInfo.sampler = m_smaaSampler;

        VkDescriptorImageInfo weightsInfo{};
        weightsInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        weightsInfo.imageView = m_smaaWeightsImageViews[i];
        weightsInfo.sampler = m_smaaSampler;

        VkDescriptorImageInfo areaInfo{};
        areaInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        areaInfo.imageView = m_smaaAreaImageView;
        areaInfo.sampler = m_smaaSampler;

        VkDescriptorImageInfo searchInfo{};
        searchInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        searchInfo.imageView = m_smaaSearchImageView;
        searchInfo.sampler = m_smaaSampler;

        std::array<VkWriteDescriptorSet, 6> writes{};

        // Edge set — 1 write.
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_smaaEdgeDescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &ldrInfo;

        // Weights set — 3 writes.
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_smaaWeightsDescriptorSets[i];
        writes[1].dstBinding = 0;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &edgesInfo;
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = m_smaaWeightsDescriptorSets[i];
        writes[2].dstBinding = 1;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].pImageInfo = &areaInfo;
        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = m_smaaWeightsDescriptorSets[i];
        writes[3].dstBinding = 2;
        writes[3].descriptorCount = 1;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[3].pImageInfo = &searchInfo;

        // Blend set — 2 writes.
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = m_smaaBlendDescriptorSets[i];
        writes[4].dstBinding = 0;
        writes[4].descriptorCount = 1;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[4].pImageInfo = &ldrInfo;
        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = m_smaaBlendDescriptorSets[i];
        writes[5].dstBinding = 1;
        writes[5].descriptorCount = 1;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[5].pImageInfo = &weightsInfo;

        vkUpdateDescriptorSets(m_device.GetDevice(), static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }
}

void RenderLoop::ClearSmaaChainToZero() {
    // Transition edges + weights images from UNDEFINED to
    // SHADER_READ_ONLY_OPTIMAL (matching the SMAA render pass's initial
    // layout and the descriptor writes above) via a TRANSFER_DST clear
    // to zero. Runs at creation and on swapchain resize so the
    // first blend sample of a disabled-SMAA frame reads deterministic
    // zeros (the blend shader short-circuits via the push-constant gate
    // anyway, but this keeps the weights texture in a known layout +
    // contents state between toggles).
    if (m_smaaEdgesImages.empty() && m_smaaWeightsImages.empty()) {
        return;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_device.GetDevice(), &allocInfo, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate SMAA-clear command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearColorValue zero{};
    auto clearOne = [&](VkImage image) {
        if (image == VK_NULL_HANDLE) {
            return;
        }
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.image = image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel = 0;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount = 1;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &toTransfer);

        VkImageSubresourceRange range = toTransfer.subresourceRange;
        vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1,
                             &range);

        VkImageMemoryBarrier toShader{};
        toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.image = image;
        toShader.subresourceRange = range;
        toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                             1, &toShader);
    };

    for (VkImage img : m_smaaEdgesImages) {
        clearOne(img);
    }
    for (VkImage img : m_smaaWeightsImages) {
        clearOne(img);
    }

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_device.GetGraphicsQueue());
    vkFreeCommandBuffers(m_device.GetDevice(), m_commandPool, 1, &cmd);
}

void RenderLoop::RunSmaaEdgeAndWeights(VkCommandBuffer cmd) {
    const uint32_t i = m_currentImageIndex;
    if (i >= m_smaaEdgesFramebuffers.size() || i >= m_smaaWeightsFramebuffers.size()) {
        return;
    }
    VkExtent2D extent = m_swapchain.GetExtent();

    auto runPass = [&](VkFramebuffer fb, VkDescriptorSet set, VkPipeline pipeline,
                       VkPipelineLayout layout, const void* pushData, size_t pushSize) {
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_smaaRenderPass;
        rpInfo.framebuffer = fb;
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = extent;
        VkClearValue clear{};
        clear.color = VkClearColorValue{{0.0F, 0.0F, 0.0F, 0.0F}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clear;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0,
                                nullptr);
        vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           static_cast<uint32_t>(pushSize), pushData);

        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    };

    SmaaEdgePipeline::PushConstants edgePush{};
    edgePush.rcpFrame = glm::vec2(1.0F / static_cast<float>(extent.width),
                                  1.0F / static_cast<float>(extent.height));
    edgePush.threshold = m_smaaThreshold;
    edgePush.localContrastFactor = 2.0F;
    runPass(m_smaaEdgesFramebuffers[i], m_smaaEdgeDescriptorSets[i],
            m_smaaEdgePipeline->GetPipeline(), m_smaaEdgePipeline->GetLayout(), &edgePush,
            sizeof(edgePush));

    SmaaWeightsPipeline::PushConstants weightsPush{};
    weightsPush.subsampleIndices = glm::vec4(0.0F);
    weightsPush.rcpFrame = edgePush.rcpFrame;
    weightsPush.maxSearchSteps = m_smaaMaxSearchSteps;
    weightsPush.maxSearchStepsDiag = m_smaaMaxSearchStepsDiag;
    runPass(m_smaaWeightsFramebuffers[i], m_smaaWeightsDescriptorSets[i],
            m_smaaWeightsPipeline->GetPipeline(), m_smaaWeightsPipeline->GetLayout(),
            &weightsPush, sizeof(weightsPush));
}

void RenderLoop::CleanupSmaaResources() {
    VkDevice device = m_device.GetDevice();
    for (size_t i = 0; i < m_smaaEdgesImages.size(); ++i) {
        if (m_smaaEdgesFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, m_smaaEdgesFramebuffers[i], nullptr);
        }
        if (m_smaaEdgesImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_smaaEdgesImageViews[i], nullptr);
        }
        if (m_smaaEdgesImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_smaaEdgesImages[i],
                            m_smaaEdgesAllocations[i]);
        }
    }
    for (size_t i = 0; i < m_smaaWeightsImages.size(); ++i) {
        if (m_smaaWeightsFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, m_smaaWeightsFramebuffers[i], nullptr);
        }
        if (m_smaaWeightsImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_smaaWeightsImageViews[i], nullptr);
        }
        if (m_smaaWeightsImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(m_device.GetAllocator(), m_smaaWeightsImages[i],
                            m_smaaWeightsAllocations[i]);
        }
    }
    m_smaaEdgesImages.clear();
    m_smaaEdgesImageViews.clear();
    m_smaaEdgesAllocations.clear();
    m_smaaEdgesFramebuffers.clear();
    m_smaaWeightsImages.clear();
    m_smaaWeightsImageViews.clear();
    m_smaaWeightsAllocations.clear();
    m_smaaWeightsFramebuffers.clear();
}

void RenderLoop::CleanupSmaaLuts() {
    VkDevice device = m_device.GetDevice();
    if (m_smaaAreaImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_smaaAreaImageView, nullptr);
        m_smaaAreaImageView = VK_NULL_HANDLE;
    }
    if (m_smaaAreaImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_smaaAreaImage, m_smaaAreaAllocation);
        m_smaaAreaImage = VK_NULL_HANDLE;
        m_smaaAreaAllocation = VK_NULL_HANDLE;
    }
    if (m_smaaSearchImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_smaaSearchImageView, nullptr);
        m_smaaSearchImageView = VK_NULL_HANDLE;
    }
    if (m_smaaSearchImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_smaaSearchImage, m_smaaSearchAllocation);
        m_smaaSearchImage = VK_NULL_HANDLE;
        m_smaaSearchAllocation = VK_NULL_HANDLE;
    }
}

void RenderLoop::CleanupSmaaDescriptors() {
    VkDevice device = m_device.GetDevice();
    if (m_smaaDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_smaaDescriptorPool, nullptr);
        m_smaaDescriptorPool = VK_NULL_HANDLE;
    }
    m_smaaEdgeDescriptorSets.clear();
    m_smaaWeightsDescriptorSets.clear();
    m_smaaBlendDescriptorSets.clear();
    if (m_smaaEdgeSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_smaaEdgeSetLayout, nullptr);
        m_smaaEdgeSetLayout = VK_NULL_HANDLE;
    }
    if (m_smaaWeightsSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_smaaWeightsSetLayout, nullptr);
        m_smaaWeightsSetLayout = VK_NULL_HANDLE;
    }
    if (m_smaaBlendSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_smaaBlendSetLayout, nullptr);
        m_smaaBlendSetLayout = VK_NULL_HANDLE;
    }
    if (m_smaaSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_smaaSampler, nullptr);
        m_smaaSampler = VK_NULL_HANDLE;
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
    m_smaaEdgePipeline.reset();
    m_smaaWeightsPipeline.reset();
    m_smaaBlendPipeline.reset();
    m_smaaVertShader.reset();
    m_smaaEdgeFragShader.reset();
    m_smaaWeightsFragShader.reset();
    m_smaaBlendFragShader.reset();

    CleanupFrameResources();
    // RP.16.8 — depth-pyramid / SMAA / SSAO allocate VMA images via their
    // CreateXxxResources paths (wired for resize). Teardown used to free
    // only the descriptors, leaking the images on dtor. Freeing the
    // resources too so VmaAllocator shuts down with an empty metadata
    // block.
    CleanupDepthPyramidResources();
    CleanupSmaaResources();
    CleanupSsaoResources();
    CleanupTonemapDescriptors();
    CleanupDepthPyramidDescriptors();
    CleanupSsaoDescriptors();
    CleanupSmaaLuts();
    CleanupSmaaDescriptors();

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_renderPass, nullptr);
    }
    if (m_postRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_postRenderPass, nullptr);
    }
    if (m_presentRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_presentRenderPass, nullptr);
    }
    if (m_smaaRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_smaaRenderPass, nullptr);
        m_smaaRenderPass = VK_NULL_HANDLE;
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
    // texelFetch (unfiltered); downstream SSAO will use textureLod
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
        // view that downstream SSAO binds through textureLod.
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
    const uint32_t imageCount = static_cast<uint32_t>(m_depthPyramidSets.size());
    for (uint32_t i = 0; i < imageCount; ++i) {
        for (uint32_t level = 0; level < DEPTH_PYRAMID_MIPS; ++level) {
            // Linearize (level == 0): source is the non-linear depth
            // attachment, sampled in SHADER_READ_ONLY_OPTIMAL after the
            // render pass end.
            // Downsample (level ≥ 1): source is the previous mip, sampled in GENERAL.
            VkImageView srcView = VK_NULL_HANDLE;
            VkImageLayout srcLayout = VK_IMAGE_LAYOUT_GENERAL;
            if (level == 0) {
                srcView = m_depthImageView;
                srcLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

    // Publish all mips to downstream consumers (SSAO compute, outline
    // fragment — RP.13b retired the tonemap-fog consumer). The chain
    // stays in GENERAL because the SSAO descriptors bind it at layout
    // GENERAL; the outline descriptors (see UpdateOutlineDescriptors)
    // also bind it at GENERAL so fragment sampling in that layout is
    // valid via the matching rules (the image was created with
    // USAGE_STORAGE). Each frame's `pre` barrier is `oldLayout =
    // UNDEFINED → GENERAL` so the prior layout is discarded before the
    // next compute dispatch.
    VkImageMemoryBarrier post = pre;
    post.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    post.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    post.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    post.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    post.subresourceRange.baseMipLevel = 0;
    post.subresourceRange.levelCount = DEPTH_PYRAMID_MIPS;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
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

// XeGTAO UBO (RP.12e) — proj + invProj only. The classic Chapman pass used
// a 64-entry hemisphere kernel here; XeGTAO walks screen-space slices so no
// kernel payload is needed. Layout mirrors `XeGtaoUbo` in `ssao_xegtao.comp`.
struct XeGtaoUboLayout {
    glm::mat4 proj;
    glm::mat4 invProj;
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

    // RP.12d — dedicated NEAREST sampler for the R8_UINT stencil binding on
    // the SSAO main set. Integer formats can't use FILTER_LINEAR, and the
    // gate reads raw texels so no filtering is wanted. CLAMP_TO_EDGE keeps
    // out-of-frustum taps from sampling garbage.
    VkSamplerCreateInfo stencilSamplerInfo{};
    stencilSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    stencilSamplerInfo.magFilter = VK_FILTER_NEAREST;
    stencilSamplerInfo.minFilter = VK_FILTER_NEAREST;
    stencilSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    stencilSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    stencilSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    stencilSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    stencilSamplerInfo.minLod = 0.0F;
    stencilSamplerInfo.maxLod = 0.0F;
    if (vkCreateSampler(m_device.GetDevice(), &stencilSamplerInfo, nullptr,
                        &m_ssaoStencilSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SSAO stencil sampler");
    }

    // Main set layout: depth pyramid (CIS), normal G-buffer (CIS),
    // XeGtaoUbo (UBO), AO output (STORAGE_IMAGE), stencil G-buffer (CIS,
    // RP.12d). Matches SsaoXeGtaoPipeline docs (RP.12e).
    std::array<VkDescriptorSetLayoutBinding, 5> mainBindings{};
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
    mainBindings[4].binding = 4;
    mainBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    mainBindings[4].descriptorCount = 1;
    mainBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

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
    // main: 3 CIS (pyramid, normal, stencil — RP.12d); blur: 2 CIS.
    sizes[0].descriptorCount = (mainSets * 3) + (blurSets * 2);
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
        m_device, ShaderStage::Compute, m_shaderDir + "/ssao_xegtao.comp.spv");
    m_ssaoBlurShader = std::make_unique<Shader>(
        m_device, ShaderStage::Compute, m_shaderDir + "/ssao_blur.comp.spv");
    m_ssaoPipeline = std::make_unique<SsaoXeGtaoPipeline>(
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
        // AO A at 1.0 so tonemap's multiply is a no-op before the first
        // SSAO dispatch lands.
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

    // UBO per swap image — XeGTAO carries just proj + invProj; the classic
    // hemisphere kernel is retired with the Chapman pass (RP.12e).
    m_ssaoUbos.clear();
    m_ssaoUbos.reserve(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        auto ubo = std::make_unique<Buffer>(
            m_device, BufferType::Uniform, sizeof(XeGtaoUboLayout), nullptr);
        auto* mapped = static_cast<XeGtaoUboLayout*>(ubo->Map());
        mapped->proj = m_proj;
        mapped->invProj = glm::inverse(m_proj);
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
        uboInfo.range = sizeof(XeGtaoUboLayout);

        VkDescriptorImageInfo aoAOutInfo{};
        aoAOutInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        aoAOutInfo.imageView = m_aoViewsA[i];

        // RP.12d — stencil G-buffer (R8_UINT, usampler2D) for the per-tap
        // transparency gate. The main render pass transitions the single-
        // sample stencil image to SHADER_READ_ONLY_OPTIMAL at end, so this
        // layout is valid every time SSAO dispatches.
        VkDescriptorImageInfo stencilInfo{};
        stencilInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        stencilInfo.imageView = i < m_stencilImageViews.size()
                                    ? m_stencilImageViews[i]
                                    : VK_NULL_HANDLE;
        stencilInfo.sampler = m_ssaoStencilSampler;

        std::array<VkWriteDescriptorSet, 5> mainWrites{};
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
        mainWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        mainWrites[4].dstSet = m_ssaoMainSets[i];
        mainWrites[4].dstBinding = 4;
        mainWrites[4].descriptorCount = 1;
        mainWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        mainWrites[4].pImageInfo = &stencilInfo;
        if (stencilInfo.imageView == VK_NULL_HANDLE) {
            // Stencil image not yet built for this swap slot — skip this
            // iteration's descriptor write rather than feed Vulkan a null
            // view. UpdateSsaoDescriptors is called again alongside
            // CreateSsaoResources after the stencil chain rebuilds.
            continue;
        }
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
    // clear to (1,0,0,0) so the multiply-composite is a no-op before the
    // first SSAO dispatch lands, then transition to SHADER_READ_ONLY_OPTIMAL
    // so the tonemap descriptor can sample it immediately.
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
    // XeGTAO UBO is just matrices; refresh them each frame so a zoom / FOV
    // change takes effect next dispatch.
    auto* mapped = static_cast<XeGtaoUboLayout*>(m_ssaoUbos[imageIndex]->Map());
    mapped->proj = m_proj;
    mapped->invProj = glm::inverse(m_proj);
    m_ssaoUbos[imageIndex]->Unmap();
}

void RenderLoop::RunSsao(VkCommandBuffer cmd) {
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
    // RP.12d architectural defaults carried over to XeGTAO (RP.12e): contact-
    // AO radius 0.35 m, intensity 0.5, shadowPower 1.5. `falloff` (new in the
    // XeGTAO push layout, replacing the Chapman `bias`) is the [0, 1] ratio
    // at which distant horizon taps begin fading toward the tangent-plane
    // (cos=0); 0.6 gives full horizon inside 60 % of the radius and a smooth
    // falloff over the outer 40 %.
    SsaoXeGtaoPipeline::PushConstants mainPush{m_ssaoRadius, m_ssaoFalloff,
                                                m_ssaoIntensity, m_ssaoShadowPower};
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
    if (m_ssaoStencilSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_ssaoStencilSampler, nullptr);
        m_ssaoStencilSampler = VK_NULL_HANDLE;
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
