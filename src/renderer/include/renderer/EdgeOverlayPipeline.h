#pragma once

#include <memory>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// RP.17.3 — graphics pipeline for the feature-edge overlay. Consumes the same
/// vertex buffer as the basic/opaque mesh pipeline (pos + normal + colour, 40-B
/// stride) but with a line-list index buffer and position-only vertex input.
/// Topology LINE_LIST, polygon mode FILL (we *are* lines), depth test LESS_OR_EQUAL
/// with depth write off, polygon-offset bias (-1 constant, -1 slope) so lines
/// win the z-fight with their owning surface, alpha-blend on for the opacity
/// knob. Push range matches opaquePipeline: mat4 model at offset 0 (vertex),
/// vec4 edgeColor at offset 64 (fragment).
class EdgeOverlayPipeline {
public:
    EdgeOverlayPipeline(const Device& device,
                        const Shader& vertexShader,
                        const Shader& fragmentShader,
                        VkRenderPass renderPass,
                        VkDescriptorSetLayout cameraLayout,
                        uint32_t colorAttachmentCount = 1,
                        bool disableSecondaryColorWrites = false,
                        bool smoothLines = false,
                        float lineWidth = 1.0F);
    ~EdgeOverlayPipeline();

    EdgeOverlayPipeline(const EdgeOverlayPipeline&) = delete;
    EdgeOverlayPipeline& operator=(const EdgeOverlayPipeline&) = delete;
    EdgeOverlayPipeline(EdgeOverlayPipeline&&) = delete;
    EdgeOverlayPipeline& operator=(EdgeOverlayPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
