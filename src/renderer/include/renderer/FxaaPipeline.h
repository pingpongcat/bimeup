#pragma once

#include <memory>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the FXAA post-process pass (RP.8b). Runs after
/// tonemap + outline in the present render pass: fullscreen triangle, no
/// depth test, no blend — writes the final anti-aliased LDR pixel directly
/// to the swapchain image.
///
/// `fxaa.frag` samples a single LDR colour input (set 0, binding 0) and
/// runs an FXAA 3.11-style edge-aware blur. Quality preset (LOW vs HIGH)
/// is driven by the `quality` push-constant int rather than a specialization
/// constant — the `renderer::Pipeline` class doesn't thread specialization
/// info through today, and runtime branching on a single uniform value
/// avoids the pipeline rebuild a specialization-constant flip would require.
///
/// CPU mirrors of the edge-detection helpers live in `renderer/FxaaMath.h`
/// (RP.8a) — `FxaaLuminance`, `FxaaLocalContrast`, `FxaaIsEdge` — so the
/// shader's early-exit predicate is pinned by unit tests before this pipeline
/// builds.
class FxaaPipeline {
public:
    // Layout matches the `push_constant` block in `fxaa.frag`. Vulkan push
    // constant rules: vec2 is 8-aligned, scalars are 4-aligned — all fields
    // pack naturally into 24 bytes with no padding.
    struct PushConstants {
        glm::vec2 rcpFrame;       // (1/width, 1/height) — frame-space texel size
        float subpixel;           // 0..1, amount of sub-pixel AA applied
        float edgeThreshold;      // relative edge gate, default 0.166 (FXAA 3.11)
        float edgeThresholdMin;   // absolute edge gate floor, default 0.0833
        int32_t quality;          // 0 = LOW (edge-only), 1 = HIGH (edge + subpixel)
    };

    FxaaPipeline(const Device& device,
                 const Shader& vertexShader,
                 const Shader& fragmentShader,
                 VkRenderPass renderPass,
                 VkDescriptorSetLayout inputLayout,
                 VkSampleCountFlagBits samples);
    ~FxaaPipeline();

    FxaaPipeline(const FxaaPipeline&) = delete;
    FxaaPipeline& operator=(const FxaaPipeline&) = delete;
    FxaaPipeline(FxaaPipeline&&) = delete;
    FxaaPipeline& operator=(FxaaPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
