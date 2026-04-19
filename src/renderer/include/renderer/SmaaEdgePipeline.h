#pragma once

#include <memory>

#include <glm/vec2.hpp>

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Pipeline;
class Shader;

/// Graphics pipeline for the SMAA 1x luma edge-detection pass (RP.11b.2).
/// Fullscreen triangle, no depth test, no blend — writes a 2-channel edges
/// texture (RG8 in practice) whose red/green channels hold the x/y edge bits
/// consumed by the weights pass (RP.11b.3).
///
/// `smaa_edge.frag` reads a single LDR colour input (set 0, binding 0) and
/// mirrors the CPU predicate `renderer::SmaaDetectEdgeLuma` (RP.11a):
/// Rec.709 luma + absolute threshold + local-contrast adaptation. The CPU
/// helper's unit tests pin the classification boundary before this shader
/// builds, so the GPU path can't drift silently.
class SmaaEdgePipeline {
public:
    // Layout matches the `push_constant` block in `smaa_edge.frag`. Vulkan
    // push-constant rules: vec2 is 8-aligned, scalars are 4-aligned — the
    // three fields pack naturally into 16 bytes with no padding.
    struct PushConstants {
        glm::vec2 rcpFrame;          // (1/width, 1/height), frame-space texel size
        float threshold;             // absolute edge gate, default 0.1
        float localContrastFactor;   // suppresses weak edges near stronger ones, default 2.0
    };

    SmaaEdgePipeline(const Device& device,
                     const Shader& vertexShader,
                     const Shader& fragmentShader,
                     VkRenderPass renderPass,
                     VkDescriptorSetLayout inputLayout,
                     VkSampleCountFlagBits samples);
    ~SmaaEdgePipeline();

    SmaaEdgePipeline(const SmaaEdgePipeline&) = delete;
    SmaaEdgePipeline& operator=(const SmaaEdgePipeline&) = delete;
    SmaaEdgePipeline(SmaaEdgePipeline&&) = delete;
    SmaaEdgePipeline& operator=(SmaaEdgePipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const;
    [[nodiscard]] VkPipelineLayout GetLayout() const;

private:
    std::unique_ptr<Pipeline> m_pipeline;
};

}  // namespace bimeup::renderer
