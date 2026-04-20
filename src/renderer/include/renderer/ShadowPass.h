#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace bimeup::renderer {

class Device;

/// Compute a directional-light view-projection matrix ("light space matrix") that
/// tightly bounds a sphere of radius `sceneRadius` centered at `sceneCenter`.
///
/// `lightDirection` is the direction light *travels* (i.e. points FROM the light
/// source, matching the convention in `renderer::DirectionalLight::direction`).
/// It does not need to be normalized.
///
/// The returned matrix maps world-space points into Vulkan clip space with depth
/// range [0, 1] and NDC.xy in [-1, 1] — points inside the bounding sphere land
/// inside the shadow map, and `uv = ndc.xy * 0.5 + 0.5` gives the shadow-map
/// sample coordinate.
[[nodiscard]] glm::mat4 ComputeLightSpaceMatrix(const glm::vec3& lightDirection,
                                                const glm::vec3& sceneCenter,
                                                float sceneRadius);

/// GPU resources for a directional-light shadow map: a depth-only VkImage sized
/// `resolution` × `resolution`, its view and sampler (for the main pass to sample
/// from), plus the VkRenderPass and VkFramebuffer used by the depth-only pass.
///
/// The render pass clears depth on load, stores it, and transitions the image to
/// SHADER_READ_ONLY_OPTIMAL so the main pass can sample it in the same frame.
///
/// RP.18.1 — The render pass also carries a second attachment at the same
/// resolution: an `R16G16B16A16_SFLOAT` colour target cleared to opaque white
/// each frame. RP.18.2's `ShadowTransmissionPipeline` writes tinted attenuation
/// into it for transparent geometry (min-blended across overlapping panes); the
/// main pass then multiplies the sampled tint into the sun term to let sunlight
/// pass through `IfcWindow` glass. Opaque shadow draws leave it untouched
/// (`colorWriteMask = 0` in their pipeline).
class ShadowMap {
public:
    static constexpr VkFormat kTransmissionFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    ShadowMap(const Device& device, uint32_t resolution);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) = delete;
    ShadowMap& operator=(ShadowMap&&) = delete;

    [[nodiscard]] VkImage GetImage() const { return m_image; }
    [[nodiscard]] VkImageView GetImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler GetSampler() const { return m_sampler; }
    [[nodiscard]] VkRenderPass GetRenderPass() const { return m_renderPass; }
    [[nodiscard]] VkFramebuffer GetFramebuffer() const { return m_framebuffer; }
    [[nodiscard]] VkFormat GetFormat() const { return m_format; }
    [[nodiscard]] uint32_t GetResolution() const { return m_resolution; }

    [[nodiscard]] VkImage GetTransmissionImage() const { return m_transmissionImage; }
    [[nodiscard]] VkImageView GetTransmissionImageView() const { return m_transmissionImageView; }
    [[nodiscard]] VkSampler GetTransmissionSampler() const { return m_transmissionSampler; }
    [[nodiscard]] static VkFormat GetTransmissionFormat() { return kTransmissionFormat; }

private:
    const Device& m_device;
    uint32_t m_resolution = 0;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkImage m_transmissionImage = VK_NULL_HANDLE;
    VmaAllocation m_transmissionAllocation = VK_NULL_HANDLE;
    VkImageView m_transmissionImageView = VK_NULL_HANDLE;
    VkSampler m_transmissionSampler = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
