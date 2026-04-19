#pragma once

#include <memory>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the SMAA 1x neighbourhood-blending pass (RP.11b.4).
/// Reads the input LDR colour + per-axis blend weights (from RP.11b.3) and
/// writes the final anti-aliased LDR pixel — the output of the SMAA chain
/// that RP.11c routes to the present pass in place of today's FXAA result.
///
/// `smaa_blend.frag` is a port of `SMAANeighborhoodBlendingPS` in
/// `external/smaa/SMAA.hlsl`. Uses hardware bilinear filtering (SMAA's
/// @BILINEAR_SAMPLING trick) to blend with the dominant neighbour in a
/// single sample.
class SmaaBlendPipeline {
public:
    // Push-constant layout matches the `push_constant` block in
    // `smaa_blend.frag`. 12 bytes: `rcpFrame` + a float `enabled` gate
    // (RP.11c) that short-circuits the shader to passthrough when SMAA is
    // toggled off by the panel — the edge + weights passes skip entirely
    // on that frame, and this flag stops the blend pass from multiplying
    // through stale weights.
    struct PushConstants {
        glm::vec2 rcpFrame;  // @ 0 — (1/width, 1/height)
        float enabled;       // @ 8 — 1.0 → run SMAA, 0.0 → passthrough LDR
    };

    SmaaBlendPipeline(const Device& device,
                      const Shader& vertexShader,
                      const Shader& fragmentShader,
                      VkRenderPass renderPass,
                      VkDescriptorSetLayout inputLayout);
    ~SmaaBlendPipeline();

    SmaaBlendPipeline(const SmaaBlendPipeline&) = delete;
    SmaaBlendPipeline& operator=(const SmaaBlendPipeline&) = delete;
    SmaaBlendPipeline(SmaaBlendPipeline&&) = delete;
    SmaaBlendPipeline& operator=(SmaaBlendPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
