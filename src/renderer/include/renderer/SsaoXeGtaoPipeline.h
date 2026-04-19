#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.12e — XeGTAO horizon-based SSAO (Intel XeGTAO
/// 2022 / Jimenez GTAO 2016). Replaces the RP.5b Chapman hemisphere-kernel
/// pass; quieter at fewer taps because the cosine-lobe integral is analytic
/// rather than a 64-sample Monte Carlo estimate. Shares the descriptor-set
/// shape with `SsaoPipeline` so the RP.12e.3 `RenderLoop` swap is a pipeline
/// swap + UBO-payload shrink, not a re-plumbing. Adaptive-base temporal
/// refinement from the paper is skipped here — no motion-vector infra yet.
///
/// Expected descriptor set (caller-owned, bound at set 0):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — linear depth
///              pyramid (full chain, sampled via `textureLod`).
///   binding 1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — oct-packed
///              view-space normal G-buffer (R16G16_SNORM from RP.3).
///   binding 2: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER — `XeGtaoUbo`:
///                 mat4 proj;     // view-pos → UV
///                 mat4 invProj;  // UV + linDepth → view-pos
///              (No kernel[] — XeGTAO walks screen-space slices, not a
///              pre-generated hemisphere point set.)
///   binding 3: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — R8 half-res AO output.
///   binding 4: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — transparency
///              stencil G-buffer (R8_UINT usampler2D, NEAREST). RP.12d
///              gate: bit 4 = "transparent surface" — centre transparent
///              → early-out AO=1, tap transparent → contribution 0.
///
/// Slice + step counts are compile-time in the shader (4 slices × 4 steps
/// = 16 taps on first pass — roughly 1/4 the classic SSAO tap count and
/// still quieter after the separable blur).
///
/// Dispatch tiles are 8×8 sized to the **half-res** AO output:
/// `ceil(halfSize.xy / 8)`.
class SsaoXeGtaoPipeline {
public:
    struct PushConstants {
        float radius;       // view-space sample radius (metres)
        float falloff;      // horizon tap falloff ratio in [0, 1] — taps at
                            // `dist >= radius` contribute 0; taps inside
                            // `falloff * radius` contribute full horizon.
        float intensity;    // 0 = no darkening, 1 = reference, >1 = punchier
        float shadowPower;  // exponent applied to the final AO term
    };

    SsaoXeGtaoPipeline(const Device& device,
                       const Shader& computeShader,
                       VkDescriptorSetLayout descriptorSetLayout);
    ~SsaoXeGtaoPipeline();

    SsaoXeGtaoPipeline(const SsaoXeGtaoPipeline&) = delete;
    SsaoXeGtaoPipeline& operator=(const SsaoXeGtaoPipeline&) = delete;
    SsaoXeGtaoPipeline(SsaoXeGtaoPipeline&&) = delete;
    SsaoXeGtaoPipeline& operator=(SsaoXeGtaoPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
