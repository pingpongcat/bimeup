#pragma once

#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the SMAA 1x blending-weight calculation pass
/// (RP.11b.3). Reads the edges texture from RP.11b.2 plus the two vendored
/// LUTs (AreaTex + SearchTex from RP.11b.1) and writes a 4-channel weights
/// texture (RGBA8 in RP.11c wiring) consumed by the neighbourhood-blending
/// pass (RP.11b.4).
///
/// `smaa_weights.frag` is a port of `SMAABlendingWeightCalculationPS` in
/// `external/smaa/SMAA.hlsl`. RP.19 exposed the iryoku LOW/MEDIUM/HIGH preset
/// knobs via push-constant ints (`maxSearchSteps`, `maxSearchStepsDiag`) —
/// previously hard-coded to the HIGH preset (16 / 8). Diagonal + corner
/// detection are always on; both matter in BIM scenes for staircase artefacts
/// on 45° edges (stairs, roofs) and T-junctions at wall intersections.
class SmaaWeightsPipeline {
public:
    // Push-constant block mirrors the `push_constant` block in
    // `smaa_weights.frag`. Layout order puts `subsampleIndices` first so
    // the vec4 sits on its natural 16-byte boundary under std430 push-
    // constant alignment; `rcpFrame` follows at offset 16; the RP.19 quality
    // ints pack at 24 + 28. Total 32 bytes.
    //
    // `subsampleIndices` is always (0,0,0,0) for SMAA 1x — kept in the
    // contract so this shader can be reused verbatim if SMAA T2x ever lands.
    struct PushConstants {
        glm::vec4 subsampleIndices;  // @ 0
        glm::vec2 rcpFrame;          // @ 16 — (1/width, 1/height)
        int maxSearchSteps;          // @ 24 — iryoku preset: LOW=4, MED=8, HIGH=16
        int maxSearchStepsDiag;      // @ 28 — iryoku preset: LOW=2, MED=4, HIGH=8
    };

    SmaaWeightsPipeline(const Device& device,
                        const Shader& vertexShader,
                        const Shader& fragmentShader,
                        VkRenderPass renderPass,
                        VkDescriptorSetLayout inputLayout);
    ~SmaaWeightsPipeline();

    SmaaWeightsPipeline(const SmaaWeightsPipeline&) = delete;
    SmaaWeightsPipeline& operator=(const SmaaWeightsPipeline&) = delete;
    SmaaWeightsPipeline(SmaaWeightsPipeline&&) = delete;
    SmaaWeightsPipeline& operator=(SmaaWeightsPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
