#pragma once

#include <memory>

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the final HDR→LDR resolve pass.
/// Fullscreen triangle generated in the vertex shader from `gl_VertexIndex`
/// (no vertex buffer); fragment shader samples the HDR colour attachment
/// via a single combined-image-sampler descriptor (set 0, binding 0) and
/// runs the ACES-fitted tonemap. No depth test, no blend, CULL_NONE.
/// Target is the sRGB swapchain so the final gamma encode is implicit.
class TonemapPipeline {
public:
    /// RP.9b fog knobs fed into `tonemap.frag` via push constants. `w` of
    /// `fogColorEnabled` doubles as the enable flag (0.0 = disabled early-
    /// out, 1.0 = apply `mix(colour, fogColor, factor)` pre-ACES). Packed
    /// to match the 24-byte std430 push-constant block declared in the
    /// shader — `vec4` aligns to 16 at offset 0, two trailing floats pack
    /// naturally starting at offset 16.
    struct PushConstants {
        glm::vec4 fogColorEnabled{0.55F, 0.60F, 0.70F, 0.0F};
        float fogStart{20.0F};
        float fogEnd{120.0F};
    };

    TonemapPipeline(const Device& device,
                    const Shader& vertexShader,
                    const Shader& fragmentShader,
                    VkRenderPass renderPass,
                    VkDescriptorSetLayout hdrSamplerLayout,
                    VkSampleCountFlagBits samples);
    ~TonemapPipeline();

    TonemapPipeline(const TonemapPipeline&) = delete;
    TonemapPipeline& operator=(const TonemapPipeline&) = delete;
    TonemapPipeline(TonemapPipeline&&) = delete;
    TonemapPipeline& operator=(TonemapPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
