#include <renderer/RenderLoop.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/Swapchain.h>
#include <renderer/TonemapPipeline.h>
#include <tools/Log.h>

#include <array>
#include <stdexcept>

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

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = m_clearColor;
    clearValues[1].depthStencil = {1.0F, 0};

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
    CreateTonemapPipeline();
    UpdateTonemapDescriptors();
    LOG_INFO("RenderLoop MSAA set to {}x", static_cast<int>(m_samples));
}

void RenderLoop::RecreateForSwapchain() {
    WaitIdle();
    CleanupFrameResources();
    CreateHdrResources();
    CreateDepthResources();
    CreateFramebuffers();
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

    // Attachment 0: scene colour. HDR_FORMAT (R16G16B16A16_SFLOAT). When
    // multisampled this is a transient MSAA image that resolves into the
    // single-sample HDR image at attachment 2; when 1×, this IS the HDR image.
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

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = m_samples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachment 2 (only if multisampled): single-sample HDR resolve target. The
    // tonemap pass reads it via sampler, so it transitions to SHADER_READ here.
    VkAttachmentDescription resolveAttachment{};
    resolveAttachment.format = HDR_FORMAT;
    resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference resolveRef{};
    resolveRef.attachment = 2;
    resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    if (multisampled) {
        subpass.pResolveAttachments = &resolveRef;
    }

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 3> attachments = {
        colorAttachment, depthAttachment, resolveAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = multisampled ? 3U : 2U;
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

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
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
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
        // color (MSAA or HDR), depth, [resolve = HDR 1×].
        std::array<VkImageView, 3> attachments{};
        uint32_t count = 0;
        if (multisampled) {
            attachments[0] = m_colorImageView;
            attachments[1] = m_depthImageView;
            attachments[2] = m_hdrImageViews[i];
            count = 3;
        } else {
            attachments[0] = m_hdrImageViews[i];
            attachments[1] = m_depthImageView;
            count = 2;
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

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(m_device.GetDevice(), &layoutInfo, nullptr,
                                    &m_tonemapSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tonemap descriptor set layout");
    }

    // One descriptor set per swapchain image (each points at its matching HDR view).
    uint32_t imageCount = m_swapchain.GetImageCount();
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = imageCount;

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
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_hdrImageViews[i];
        imageInfo.sampler = m_hdrSampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_tonemapDescriptorSets[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_device.GetDevice(), 1, &write, 0, nullptr);
    }
}

void RenderLoop::CleanupFrameResources() {
    VkDevice device = m_device.GetDevice();

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

    CleanupFrameResources();
    CleanupTonemapDescriptors();

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
