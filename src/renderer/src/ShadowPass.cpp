#include <renderer/ShadowPass.h>

#include <renderer/Device.h>

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <stdexcept>

namespace bimeup::renderer {

namespace {

// Pick a stable up vector that is not parallel to `forward`. If the light points
// nearly along world +Y/-Y, fall back to +Z so lookAt does not degenerate.
glm::vec3 PickStableUp(const glm::vec3& forward) {
    constexpr float kParallelThreshold = 0.999F;
    if (std::abs(forward.y) > kParallelThreshold) {
        return glm::vec3(0.0F, 0.0F, 1.0F);
    }
    return glm::vec3(0.0F, 1.0F, 0.0F);
}

}  // namespace

ShadowDrawClassification ClassifyOpaqueVsTransmissive(
    std::span<const float> effectiveAlphas, float transmissiveCutoff) {
    ShadowDrawClassification buckets;
    buckets.opaqueIndices.reserve(effectiveAlphas.size());
    for (std::size_t i = 0; i < effectiveAlphas.size(); ++i) {
        if (effectiveAlphas[i] < transmissiveCutoff) {
            buckets.transmissiveIndices.push_back(i);
        } else {
            buckets.opaqueIndices.push_back(i);
        }
    }
    return buckets;
}

glm::mat4 ComputeLightSpaceMatrix(const glm::vec3& lightDirection,
                                  const glm::vec3& sceneCenter,
                                  float sceneRadius) {
    const glm::vec3 dir = glm::normalize(lightDirection);

    // Place the light "eye" at sceneCenter shifted back along -dir by sceneRadius,
    // so the full bounding sphere sits between near=0 and far=2*sceneRadius.
    const glm::vec3 eye = sceneCenter - dir * sceneRadius;
    const glm::vec3 up = PickStableUp(dir);
    const glm::mat4 view = glm::lookAtRH(eye, sceneCenter, up);

    // Zero-to-one depth range matches Vulkan's clip space.
    const glm::mat4 proj = glm::orthoRH_ZO(-sceneRadius, sceneRadius,
                                           -sceneRadius, sceneRadius,
                                           0.0F, 2.0F * sceneRadius);

    return proj * view;
}

namespace {

VkFormat PickDepthFormat(VkPhysicalDevice physicalDevice) {
    constexpr std::array<VkFormat, 2> kCandidates = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM};
    constexpr VkFormatFeatureFlags kRequired =
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    for (VkFormat fmt : kCandidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, fmt, &props);
        if ((props.optimalTilingFeatures & kRequired) == kRequired) {
            return fmt;
        }
    }
    throw std::runtime_error("No suitable depth format for shadow map");
}

}  // namespace

ShadowMap::ShadowMap(const Device& device, uint32_t resolution)
    : m_device(device), m_resolution(resolution) {
    if (resolution == 0) {
        throw std::runtime_error("Shadow map resolution must be non-zero");
    }

    m_format = PickDepthFormat(m_device.GetPhysicalDevice());

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_format;
    imageInfo.extent = {resolution, resolution, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(m_device.GetAllocator(), &imageInfo, &allocInfo,
                       &m_image, &m_allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate shadow map image");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_device.GetDevice(), &viewInfo, nullptr,
                          &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map image view");
    }

    // Comparison sampler for GLSL sampler2DShadow: depth comparison is done inside
    // the sampler (returns 1.0 if ref <= stored depth, 0.0 otherwise, filtered).
    // Border = opaque white so out-of-bounds UVs read as lit.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.maxLod = 1.0F;
    if (vkCreateSampler(m_device.GetDevice(), &samplerInfo, nullptr,
                        &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map sampler");
    }

    // RP.18.1 — transmission attachment: tinted light attenuation for windows.
    // Colour attachment + sampled-image usage; cleared to opaque white each
    // frame (= "no attenuation") and read by basic.frag via a regular sampler.
    VkImageCreateInfo transmissionImageInfo{};
    transmissionImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    transmissionImageInfo.imageType = VK_IMAGE_TYPE_2D;
    transmissionImageInfo.format = kTransmissionFormat;
    transmissionImageInfo.extent = {resolution, resolution, 1};
    transmissionImageInfo.mipLevels = 1;
    transmissionImageInfo.arrayLayers = 1;
    transmissionImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    transmissionImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    transmissionImageInfo.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    transmissionImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    transmissionImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vmaCreateImage(m_device.GetAllocator(), &transmissionImageInfo, &allocInfo,
                       &m_transmissionImage, &m_transmissionAllocation,
                       nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate shadow transmission image");
    }

    VkImageViewCreateInfo transmissionViewInfo{};
    transmissionViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    transmissionViewInfo.image = m_transmissionImage;
    transmissionViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    transmissionViewInfo.format = kTransmissionFormat;
    transmissionViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transmissionViewInfo.subresourceRange.baseMipLevel = 0;
    transmissionViewInfo.subresourceRange.levelCount = 1;
    transmissionViewInfo.subresourceRange.baseArrayLayer = 0;
    transmissionViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_device.GetDevice(), &transmissionViewInfo, nullptr,
                          &m_transmissionImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow transmission image view");
    }

    // Standard linear sampler (not comparison) — basic.frag reads the RGBA tint
    // directly and multiplies into the sun term. Border = opaque white so
    // samples outside the shadow frustum read as "no attenuation".
    VkSamplerCreateInfo transmissionSamplerInfo{};
    transmissionSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    transmissionSamplerInfo.magFilter = VK_FILTER_LINEAR;
    transmissionSamplerInfo.minFilter = VK_FILTER_LINEAR;
    transmissionSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    transmissionSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    transmissionSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    transmissionSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    transmissionSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    transmissionSamplerInfo.compareEnable = VK_FALSE;
    transmissionSamplerInfo.maxLod = 1.0F;
    if (vkCreateSampler(m_device.GetDevice(), &transmissionSamplerInfo, nullptr,
                        &m_transmissionSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow transmission sampler");
    }

    std::array<VkAttachmentDescription, 2> attachments{};
    VkAttachmentDescription& depthAttachment = attachments[0];
    depthAttachment.format = m_format;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentDescription& transmissionAttachment = attachments[1];
    transmissionAttachment.format = kTransmissionFormat;
    transmissionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    transmissionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    transmissionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    transmissionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    transmissionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    transmissionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    transmissionAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference transmissionRef{};
    transmissionRef.attachment = 1;
    transmissionRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &transmissionRef;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies = deps.data();
    if (vkCreateRenderPass(m_device.GetDevice(), &rpInfo, nullptr,
                           &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map render pass");
    }

    std::array<VkImageView, 2> fbAttachments = {m_imageView, m_transmissionImageView};
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(fbAttachments.size());
    fbInfo.pAttachments = fbAttachments.data();
    fbInfo.width = resolution;
    fbInfo.height = resolution;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(m_device.GetDevice(), &fbInfo, nullptr,
                            &m_framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map framebuffer");
    }

    // One-shot UNDEFINED → SHADER_READ_ONLY_OPTIMAL transition so the image has a
    // valid sampled layout from the moment it's bound to a descriptor set. Without
    // this, the main pass can sample the image before any shadow pass has ever run
    // against it (disabled shadows, or the frame the shadow map is first created),
    // which the validation layer flags as VUID-vkCmdDraw-None-09600.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = m_device.GetGraphicsQueueFamily();
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(m_device.GetDevice(), &poolInfo, nullptr, &pool) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map init command pool");
    }

    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = pool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_device.GetDevice(), &cbAlloc, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(m_device.GetDevice(), pool, nullptr);
        throw std::runtime_error("Failed to allocate shadow map init cmd buffer");
    }

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    std::array<VkImageMemoryBarrier, 2> barriers{};
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = m_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    barriers[1] = barriers[0];
    barriers[1].image = m_transmissionImage;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, static_cast<uint32_t>(barriers.size()),
                         barriers.data());

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_device.GetGraphicsQueue());

    vkDestroyCommandPool(m_device.GetDevice(), pool, nullptr);
}

ShadowMap::~ShadowMap() {
    VkDevice dev = m_device.GetDevice();
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(dev, m_framebuffer, nullptr);
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, m_renderPass, nullptr);
    }
    if (m_transmissionSampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, m_transmissionSampler, nullptr);
    }
    if (m_transmissionImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, m_transmissionImageView, nullptr);
    }
    if (m_transmissionImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_transmissionImage,
                        m_transmissionAllocation);
    }
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, m_sampler, nullptr);
    }
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(dev, m_imageView, nullptr);
    }
    if (m_image != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.GetAllocator(), m_image, m_allocation);
    }
}

}  // namespace bimeup::renderer
