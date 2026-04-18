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
