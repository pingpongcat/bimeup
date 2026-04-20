#pragma once

#include <memory>

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
    /// RP.11 / RP.13b / 9.8.a knobs fed into `tonemap.frag` via push
    /// constants. `exposure` is a pre-ACES multiplier on the AO-modulated
    /// HDR; defaults to 1.0 so unit tests / bare constructors behave
    /// identically, while the panel seeds a scene-appropriate value
    /// (~0.6). `useRtAo` selects the AO source — 0.0 samples the XeGTAO
    /// half-res AO image bound at binding 1 (bit-compatible raster path),
    /// 1.0 samples the RT AO image bound at binding 2 (Stage 9.8.a hybrid
    /// composite). RP.13b retired fog.
    struct PushConstants {
        float exposure{1.0F};
        float useRtAo{0.0F};
    };

    TonemapPipeline(const Device& device,
                    const Shader& vertexShader,
                    const Shader& fragmentShader,
                    VkRenderPass renderPass,
                    VkDescriptorSetLayout hdrSamplerLayout);
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
