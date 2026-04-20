#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstdint>

namespace bimeup::renderer {

class Device;
class Shader;

/// Stage 9.8.b.2 — compute pipeline for the Hybrid-RT sun composite. Runs
/// after the RT shadow + AO dispatches and before `BuildDepthPyramid`,
/// reconstructing world-space position from depth and adding the sun
/// contribution (`keyColor * NdotL * transmittedTint * rtVisibility`)
/// additively into the per-swap HDR image. Paired with Stage 9.8.b.1's
/// `useRtSunPath = 1` push in `basic.frag` — when Hybrid RT is active the
/// main pass leaves the sun term out and this pass re-applies it with the
/// raytraced visibility + RP.18 glass tint.
///
/// Descriptor set 0 (caller-owned — 9.8.b.3 allocates the per-swap set):
///   binding 0: depth G-buffer                (combined-image-sampler)
///   binding 1: oct-packed view-space normal  (combined-image-sampler)
///   binding 2: RT shadow visibility          (combined-image-sampler)
///   binding 3: shadow transmission map       (combined-image-sampler)
///   binding 4: LightingUBO                   (uniform buffer)
///   binding 5: HDR image                     (storage image, additive RW)
///
/// Dispatch tiles are 8×8 sized to the full-res HDR extent.
class RtSunCompositePipeline {
public:
    /// Push-constant block packed to 144 B. `invViewProj` inverts the
    /// current frame's projection * view for depth → world reconstruction.
    /// `invView` lifts the view-space normal G-buffer sample back to
    /// world-space so the Lambert dot product runs in the same space as
    /// the lighting UBO's world-space sun direction and the world-space
    /// fragment position feeding the light-space matrix for the RP.18
    /// transmission sample. `extent` is the HDR image size in pixels.
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::mat4 invView;
        glm::uvec2 extent;
        uint32_t pad0 = 0;
        uint32_t pad1 = 0;
    };

    RtSunCompositePipeline(const Device& device,
                           const Shader& computeShader,
                           VkDescriptorSetLayout descriptorSetLayout);
    ~RtSunCompositePipeline();

    RtSunCompositePipeline(const RtSunCompositePipeline&) = delete;
    RtSunCompositePipeline& operator=(const RtSunCompositePipeline&) = delete;
    RtSunCompositePipeline(RtSunCompositePipeline&&) = delete;
    RtSunCompositePipeline& operator=(RtSunCompositePipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
