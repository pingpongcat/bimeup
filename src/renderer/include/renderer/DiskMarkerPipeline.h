#pragma once

#include <memory>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for drawing a flat disk marker (pre-triangulated
/// `DiskVertex`: vec3 position + vec4 color). Depth test LEQUAL with depth
/// write disabled so the marker composites over the slab it lies on without
/// occluding geometry behind it; CULL_NONE because the disk is viewed from
/// both sides; alpha blending ON so per-vertex alpha produces a soft edge.
/// Uses a single camera UBO descriptor set, matching SectionFillPipeline.
class DiskMarkerPipeline {
public:
    DiskMarkerPipeline(const Device& device,
                       const Shader& vertexShader,
                       const Shader& fragmentShader,
                       VkRenderPass renderPass,
                       VkDescriptorSetLayout cameraLayout,
                       uint32_t colorAttachmentCount = 1,
                       bool disableSecondaryColorWrites = false);
    ~DiskMarkerPipeline();

    DiskMarkerPipeline(const DiskMarkerPipeline&) = delete;
    DiskMarkerPipeline& operator=(const DiskMarkerPipeline&) = delete;
    DiskMarkerPipeline(DiskMarkerPipeline&&) = delete;
    DiskMarkerPipeline& operator=(DiskMarkerPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
