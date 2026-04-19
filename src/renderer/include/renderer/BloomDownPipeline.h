#pragma once

#include <memory>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the bloom dual-filter downsample pass (RP.10b).
/// Fullscreen triangle, no depth test, no blend — writes the half-resolution
/// filtered colour directly into the target bloom mip attachment.
///
/// `bloom_down.frag` samples a single source colour input (set 0, binding 0)
/// and runs Marius Bjørge's 5-tap downsample (centre weight 4, 4 diagonals
/// weight 1, /8). The first downsample in the pyramid — HDR scene → mip 0 —
/// sets `applyPrefilter = 1` to additionally run a Jorge Jimenez / COD
/// soft-knee threshold per sample; subsequent mip-to-mip passes pass 0.
///
/// CPU mirrors of the weighting and prefilter helpers live in
/// `renderer/BloomMath.h` (RP.10a) — `BloomPrefilter`, `BloomDownsample` —
/// so the shader's weights and knee formulation are pinned by unit tests
/// before this pipeline builds.
class BloomDownPipeline {
public:
    // Layout matches the `push_constant` block in `bloom_down.frag`. Vulkan
    // push-constant rules: vec2 is 8-aligned, scalars are 4-aligned — all
    // fields pack naturally into 20 bytes with no padding. `applyPrefilter`
    // is an int32 so the shader can branch between the "HDR → mip 0" and
    // "mip_n → mip_{n+1}" cases without a pipeline rebuild.
    struct PushConstants {
        glm::vec2 rcpSrcSize;      // (1/srcWidth, 1/srcHeight)
        float threshold;           // prefilter cutoff (max-channel luma)
        float knee;                // half-width of the soft-knee region
        int32_t applyPrefilter;    // 0 = mip→mip, 1 = scene→mip 0 with knee
    };

    BloomDownPipeline(const Device& device,
                      const Shader& vertexShader,
                      const Shader& fragmentShader,
                      VkRenderPass renderPass,
                      VkDescriptorSetLayout inputLayout,
                      VkSampleCountFlagBits samples);
    ~BloomDownPipeline();

    BloomDownPipeline(const BloomDownPipeline&) = delete;
    BloomDownPipeline& operator=(const BloomDownPipeline&) = delete;
    BloomDownPipeline(BloomDownPipeline&&) = delete;
    BloomDownPipeline& operator=(BloomDownPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
