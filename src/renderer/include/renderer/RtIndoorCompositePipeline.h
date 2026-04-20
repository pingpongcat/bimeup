#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace bimeup::renderer {

class Device;
class Shader;

/// Stage 9.8.c.2 — compute pipeline for the Hybrid-RT indoor-fill
/// composite. Runs after the Stage-9.8.b.2 sun composite and before
/// `BuildDepthPyramid`, reconstructing world-space position from depth
/// and adding the overhead-fill contribution
/// (`fillColor * NdotL * rtIndoorVisibility`) additively into the per-swap
/// HDR image. Paired with Stage 9.8.c.1's `useRtIndoorPath = 1` push in
/// `basic.frag` — when Hybrid RT is active the main pass leaves the fill
/// lambert out and this pass re-applies it with RT wall-occlusion from
/// `RtIndoorPass`.
///
/// Descriptor set 0 (caller-owned — 9.8.c.3 allocates the per-swap set):
///   binding 0: depth G-buffer                (combined-image-sampler)
///   binding 1: oct-packed view-space normal  (combined-image-sampler)
///   binding 2: RT indoor visibility          (combined-image-sampler, R8_UNORM)
///   binding 3: LightingUBO                   (uniform buffer)
///   binding 4: HDR image                     (storage image, additive RW)
///
/// Dispatch tiles are 8×8 sized to the full-res HDR extent.
class RtIndoorCompositePipeline {
public:
    /// Push-constant block packed to 144 B. Same shape as
    /// `RtSunCompositePipeline::PushConstants` — `invViewProj` inverts the
    /// current frame's projection * view for depth → world reconstruction,
    /// `invView` lifts the view-space normal G-buffer sample back to world
    /// space so the Lambert dot product runs in the same space as the
    /// lighting UBO's world-space fill direction, `extent` is the HDR
    /// image size in pixels.
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::mat4 invView;
        glm::uvec2 extent;
        uint32_t pad0 = 0;
        uint32_t pad1 = 0;
    };

    RtIndoorCompositePipeline(const Device& device,
                              const Shader& computeShader,
                              VkDescriptorSetLayout descriptorSetLayout);
    ~RtIndoorCompositePipeline();

    RtIndoorCompositePipeline(const RtIndoorCompositePipeline&) = delete;
    RtIndoorCompositePipeline& operator=(const RtIndoorCompositePipeline&) = delete;
    RtIndoorCompositePipeline(RtIndoorCompositePipeline&&) = delete;
    RtIndoorCompositePipeline& operator=(RtIndoorCompositePipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
