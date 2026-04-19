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
    // `smaa_blend.frag` — only `rcpFrame` since the weights texture
    // already encodes everything else the blend pass needs. 8 bytes.
    struct PushConstants {
        glm::vec2 rcpFrame;  // (1/width, 1/height)
    };

    SmaaBlendPipeline(const Device& device,
                      const Shader& vertexShader,
                      const Shader& fragmentShader,
                      VkRenderPass renderPass,
                      VkDescriptorSetLayout inputLayout,
                      VkSampleCountFlagBits samples);
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
