#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.5c — separable edge-aware blur for the RP.5b AO
/// target. Single shader (`ssao_blur.comp`) runs twice per frame: horizontal
/// pass (direction = {1, 0}) then vertical pass (direction = {0, 1}), with
/// ping-ponged descriptor sets so the output of pass 1 is the input of pass
/// 2. 7-tap symmetric gaussian per pass; depth-delta rejection through the
/// depth pyramid preserves contact shadows at geometry edges.
///
/// Shape matches `SsaoPipeline` / `DepthLinearizePipeline`: owns its own
/// layout + pipeline directly.
///
/// Expected descriptor set (caller-owned, bound at set 0):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — R8 AO input
///              (half-res; uses bilinear on out-of-bounds but the loop
///              `clamp`s coords so edge smear isn't visible).
///   binding 1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — linear depth
///              pyramid. Blur samples mip 0 and rejects taps with large
///              depth deltas (edge-preserve).
///   binding 2: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — R8 AO output (same size
///              as input, different image so ping-pong is safe).
///
/// Dispatch tiles are 8×8 sized to the half-res output.
class SsaoBlurPipeline {
public:
    struct PushConstants {
        std::int32_t dirX;       // {1, 0} horizontal pass
        std::int32_t dirY;       // {0, 1} vertical pass
        float edgeSharpness;     // larger → more aggressive edge rejection
    };

    SsaoBlurPipeline(const Device& device,
                     const Shader& computeShader,
                     VkDescriptorSetLayout descriptorSetLayout);
    ~SsaoBlurPipeline();

    SsaoBlurPipeline(const SsaoBlurPipeline&) = delete;
    SsaoBlurPipeline& operator=(const SsaoBlurPipeline&) = delete;
    SsaoBlurPipeline(SsaoBlurPipeline&&) = delete;
    SsaoBlurPipeline& operator=(SsaoBlurPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
