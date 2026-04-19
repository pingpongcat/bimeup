#pragma once

#include <memory>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the screen-space selection/hover outline pass
/// (RP.6b). Runs after tonemap in the present render pass: fullscreen
/// triangle, alpha-blended over the LDR swapchain image, no depth test.
///
/// `outline.frag` samples a 3×3 patch of the stencil-id attachment (set 0,
/// binding 0 — R8_UINT sampled as `usampler2D`) and applies the
/// `EdgeFromStencil` rule — hover (2) beats selected (1) when both appear
/// in the window. Where the stencil is uniform and the centre lies inside
/// a selected element, a Sobel-on-linear-depth fallback (set 0, binding 1 —
/// mip 0 of the RP.4 depth pyramid) promotes within-selection silhouette
/// creases to the outline.
///
/// Push constants carry the panel-tweakable knobs (selected/hover colours,
/// screen-space tap thickness, depth-edge magnitude cutoff) so the whole
/// block fits in 48 bytes — well under the 128-byte Vulkan guarantee and no
/// UBO plumbing needed.
class OutlinePipeline {
public:
    // Layout matches the `push_constant` block in `outline.frag`. The colour
    // members use `alignas(16)` so the C++ struct satisfies std140 layout
    // rules for push-constant blocks (vec4 → 16-byte aligned); the vec2 +
    // two trailing floats pack into the final 16-byte unit.
    struct PushConstants {
        alignas(16) glm::vec4 selectedColor;
        alignas(16) glm::vec4 hoverColor;
        alignas(8)  glm::vec2 texelSize;  // (1/width, 1/height) in UV
        float thickness;                  // screen-space tap offset in pixels
        float depthEdgeThreshold;         // Sobel(linear depth) cutoff in metres
    };

    OutlinePipeline(const Device& device,
                    const Shader& vertexShader,
                    const Shader& fragmentShader,
                    VkRenderPass renderPass,
                    VkDescriptorSetLayout inputLayout);
    ~OutlinePipeline();

    OutlinePipeline(const OutlinePipeline&) = delete;
    OutlinePipeline& operator=(const OutlinePipeline&) = delete;
    OutlinePipeline(OutlinePipeline&&) = delete;
    OutlinePipeline& operator=(OutlinePipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
