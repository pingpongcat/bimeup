#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.4c — downsamples one mip of the linear-depth image
/// to the next by taking the minimum of each 2×2 source neighbourhood. Build
/// the full 4-mip depth pyramid with one dispatch per level (mip 0 → 1,
/// mip 1 → 2, mip 2 → 3), rebinding a per-level descriptor set between
/// dispatches. Min (rather than max) is the SSAO-conservative choice: the
/// closer surface in a neighbourhood biases toward more occlusion, matching
/// the XeGTAO / Godot reference pyramid.
///
/// Expected descriptor set (caller-owned, one per level):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — source mip
///   binding 1: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — R32_SFLOAT destination mip
///
/// No push constants — the shader derives bounds from `imageSize(dest)`.
/// Dispatch in 8×8 tiles sized to the destination mip.
class DepthMipPipeline {
public:
    DepthMipPipeline(const Device& device,
                     const Shader& computeShader,
                     VkDescriptorSetLayout descriptorSetLayout);
    ~DepthMipPipeline();

    DepthMipPipeline(const DepthMipPipeline&) = delete;
    DepthMipPipeline& operator=(const DepthMipPipeline&) = delete;
    DepthMipPipeline(DepthMipPipeline&&) = delete;
    DepthMipPipeline& operator=(DepthMipPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
