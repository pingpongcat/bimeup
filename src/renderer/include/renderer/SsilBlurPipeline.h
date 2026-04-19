#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.7c — separable edge-aware blur over the RP.7b
/// half-res RGBA16F SSIL target. Single shader (`ssil_blur.comp`) runs twice
/// per frame: horizontal pass (direction = {1, 0}) then vertical pass
/// (direction = {0, 1}), with ping-ponged descriptor sets so the output of
/// pass 1 is the input of pass 2. 7-tap symmetric gaussian per pass; depth-
/// delta + normal-dot rejection preserves both silhouette discontinuities and
/// crease boundaries — RP.7b's per-tap `NormalRejectionWeight` already picked
/// these up, but an isotropic blur would smear bounce light across them.
///
/// Shape matches `SsaoBlurPipeline` / `SsaoPipeline`: owns its own layout +
/// pipeline directly.
///
/// Expected descriptor set (caller-owned, bound at set 0):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — RGBA16F SSIL
///              input (half-res; the loop `clamp`s taps so sampler-clamp
///              smear at the edge isn't visible).
///   binding 1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — linear depth
///              pyramid. Blur samples mip 0 and rejects taps with large
///              depth deltas (silhouette-preserve).
///   binding 2: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — oct-packed
///              view-space normal G-buffer. Rejects taps whose normal
///              doesn't align with the centre (crease-preserve).
///   binding 3: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — RGBA16F SSIL output
///              (same size as input, different image so ping-pong is safe).
///
/// Dispatch tiles are 8×8 sized to the half-res output.
class SsilBlurPipeline {
public:
    struct PushConstants {
        std::int32_t dirX;         // {1, 0} horizontal pass
        std::int32_t dirY;         // {0, 1} vertical pass
        float edgeSharpness;       // larger → more aggressive depth rejection
        float normalSharpness;     // exponent on the dot(nCentre, nTap) lobe
    };

    SsilBlurPipeline(const Device& device,
                     const Shader& computeShader,
                     VkDescriptorSetLayout descriptorSetLayout);
    ~SsilBlurPipeline();

    SsilBlurPipeline(const SsilBlurPipeline&) = delete;
    SsilBlurPipeline& operator=(const SsilBlurPipeline&) = delete;
    SsilBlurPipeline(SsilBlurPipeline&&) = delete;
    SsilBlurPipeline& operator=(SsilBlurPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
