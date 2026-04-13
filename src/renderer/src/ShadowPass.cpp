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

    // Border-clamped, linear-filtered sampler. Comparison is wired in R.3c when PCF
    // is added; a plain sampler works for the resource-creation stage.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxLod = 1.0F;
    if (vkCreateSampler(m_device.GetDevice(), &samplerInfo, nullptr,
                        &m_sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map sampler");
    }

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_format;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &depthAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(deps.size());
    rpInfo.pDependencies = deps.data();
    if (vkCreateRenderPass(m_device.GetDevice(), &rpInfo, nullptr,
                           &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map render pass");
    }

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_imageView;
    fbInfo.width = resolution;
    fbInfo.height = resolution;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(m_device.GetDevice(), &fbInfo, nullptr,
                            &m_framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map framebuffer");
    }
}

ShadowMap::~ShadowMap() {
    VkDevice dev = m_device.GetDevice();
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(dev, m_framebuffer, nullptr);
    }
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(dev, m_renderPass, nullptr);
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
