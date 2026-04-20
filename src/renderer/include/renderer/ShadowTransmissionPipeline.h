#pragma once

#include <memory>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// RP.18.2 — graphics pipeline that writes tinted sun attenuation for transparent
/// geometry into the shadow render pass's R16G16B16A16_SFLOAT transmission
/// attachment. Bound in the same render pass as the opaque shadow-depth
/// pipeline (see `ShadowMap::GetRenderPass`), after opaque depth is written:
///
///  - Vertex input: position-only from the shared mesh vertex buffer.
///  - Depth: test LESS against the opaque depth attachment; depth-write OFF
///    (a wall behind glass must still fully block the sun).
///  - Blend: VK_BLEND_OP_MIN with ONE/ONE factors, so overlapping glass panes
///    compose as the darkest/most-tinted tap (classical coloured shadow map
///    approximation of multiplicative attenuation).
///  - Culling: NONE — IFC glass triangulation is commonly two-sided, and a
///    well-formed single-sided pane would write the same tint from either
///    direction under min-blend.
///  - Push range: `mat4 lightSpaceModel` at offset 0 (vertex), then
///    `vec4 glassTint` at offset 64 (fragment). CPU is expected to pre-scale
///    the tint as `surfaceColor.rgb * (1 - effectiveAlpha)` before pushing.
class ShadowTransmissionPipeline {
public:
    ShadowTransmissionPipeline(const Device& device,
                               const Shader& vertexShader,
                               const Shader& fragmentShader,
                               VkRenderPass shadowRenderPass);
    ~ShadowTransmissionPipeline();

    ShadowTransmissionPipeline(const ShadowTransmissionPipeline&) = delete;
    ShadowTransmissionPipeline& operator=(const ShadowTransmissionPipeline&) = delete;
    ShadowTransmissionPipeline(ShadowTransmissionPipeline&&) = delete;
    ShadowTransmissionPipeline& operator=(ShadowTransmissionPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

    /// True iff the pipeline was built with VK_BLEND_OP_MIN on the colour
    /// attachment. Fixed to true today; exposed as a getter so tests pin the
    /// contract without having to reach into VkPipeline internals.
    [[nodiscard]] bool UsesMinBlend() const { return true; }

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
