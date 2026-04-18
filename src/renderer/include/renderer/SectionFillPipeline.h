#pragma once

#include <memory>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for drawing pre-triangulated section-cap geometry
/// (`scene::SectionVertex`: vec3 position + vec4 color). Depth test LEQUAL
/// with depth write disabled so caps composite over the scene without
/// self-occluding; CULL_NONE because triangulation may produce either
/// winding; no stencil; only a single camera UBO descriptor set.
class SectionFillPipeline {
public:
    SectionFillPipeline(const Device& device,
                        const Shader& vertexShader,
                        const Shader& fragmentShader,
                        VkRenderPass renderPass,
                        VkDescriptorSetLayout cameraLayout,
                        VkSampleCountFlagBits samples,
                        uint32_t colorAttachmentCount = 1,
                        bool disableSecondaryColorWrites = false);
    ~SectionFillPipeline();

    SectionFillPipeline(const SectionFillPipeline&) = delete;
    SectionFillPipeline& operator=(const SectionFillPipeline&) = delete;
    SectionFillPipeline(SectionFillPipeline&&) = delete;
    SectionFillPipeline& operator=(SectionFillPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
