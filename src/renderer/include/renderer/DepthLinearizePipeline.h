#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.4b — linearises the main pass's non-linear depth
/// attachment into a view-space-depth storage image (mip 0 of the depth
/// pyramid that RP.4c will build on). Bimeup's first compute pipeline, so
/// unlike the graphics pipelines it does not route through `renderer::Pipeline`
/// — it owns its own `VkPipelineLayout` + `VkPipeline` directly.
///
/// Expected descriptor set (caller-owned):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — depth as sampler2D
///   binding 1: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — R32_SFLOAT linear-depth out
///
/// Push constants carry (nearZ, farZ). Dispatch in 8×8 tiles.
class DepthLinearizePipeline {
public:
    struct PushConstants {
        float nearZ;
        float farZ;
    };

    DepthLinearizePipeline(const Device& device,
                           const Shader& computeShader,
                           VkDescriptorSetLayout descriptorSetLayout);
    ~DepthLinearizePipeline();

    DepthLinearizePipeline(const DepthLinearizePipeline&) = delete;
    DepthLinearizePipeline& operator=(const DepthLinearizePipeline&) = delete;
    DepthLinearizePipeline(DepthLinearizePipeline&&) = delete;
    DepthLinearizePipeline& operator=(DepthLinearizePipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
