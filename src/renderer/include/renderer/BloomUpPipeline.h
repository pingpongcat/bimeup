#pragma once

#include <memory>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the bloom dual-filter tent upsample pass (RP.10b).
/// Fullscreen triangle, no depth test, no blend — writes the tent-upsampled
/// contribution into the target (double-resolution) mip attachment. The
/// actual composite with the higher mip is an RP.10c wiring concern (hw
/// additive blend via an extended PipelineConfig, or a separate composite
/// shader reading both mips).
///
/// `bloom_up.frag` samples a single source colour input (set 0, binding 0)
/// and runs Marius Bjørge's 8-tap tent — 4 cardinals weight 1, 4 diagonals
/// weight 2, /12. No centre tap: composite would double-count it.
///
/// CPU mirror `renderer::BloomUpsample` in `renderer/BloomMath.h` (RP.10a)
/// pins the weights.
class BloomUpPipeline {
public:
    // Layout matches the `push_constant` block in `bloom_up.frag`. 12 bytes
    // total — Vulkan push constants accept any 4-aligned size; no padding
    // needed. `intensity` is 1.0 for intermediate upsamples and the
    // user-picked `bloomIntensity` panel value for the final composite.
    struct PushConstants {
        glm::vec2 rcpSrcSize;   // (1/srcWidth, 1/srcHeight)
        float intensity;        // composite-scale multiplier
    };

    BloomUpPipeline(const Device& device,
                    const Shader& vertexShader,
                    const Shader& fragmentShader,
                    VkRenderPass renderPass,
                    VkDescriptorSetLayout inputLayout,
                    VkSampleCountFlagBits samples);
    ~BloomUpPipeline();

    BloomUpPipeline(const BloomUpPipeline&) = delete;
    BloomUpPipeline& operator=(const BloomUpPipeline&) = delete;
    BloomUpPipeline(BloomUpPipeline&&) = delete;
    BloomUpPipeline& operator=(BloomUpPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
