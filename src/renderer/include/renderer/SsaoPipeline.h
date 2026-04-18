#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.5b — classic hemisphere-kernel SSAO (Chapman 2013 /
/// LearnOpenGL), writing a half-res R8 AO term from the RP.4 linear depth
/// pyramid and the RP.3 view-space normal G-buffer. Follows the shape of
/// `DepthLinearizePipeline` / `DepthMipPipeline`: owns its own
/// `VkPipelineLayout` + `VkPipeline` directly (graphics-only
/// `renderer::Pipeline` doesn't cover compute).
///
/// Expected descriptor set (caller-owned, bound at set 0):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — linear depth
///              pyramid (full chain, sampled via `textureLod`).
///   binding 1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — oct-packed
///              view-space normal G-buffer (R16G16_SNORM from RP.3).
///   binding 2: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER — `SsaoUbo`:
///                 mat4  proj;         // view-pos → UV
///                 mat4  invProj;      // UV + linDepth → view-pos
///                 vec4  kernel[64];   // .xyz = offset, .w = 0
///   binding 3: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — R8 half-res AO output.
///
/// Kernel count is a compile-time constant (64) mirrored on both sides to
/// keep the UBO layout fixed; `renderer::GenerateHemisphereKernel(64, seed)`
/// from RP.5a supplies the CPU source.
///
/// Dispatch tiles are 8×8 sized to the **half-res** AO output:
/// `ceil(halfSize.xy / 8)`.
class SsaoPipeline {
public:
    // Compile-time kernel size — mirrors the `SSAO_KERNEL_SIZE` #define in
    // `ssao_main.comp`. Don't change one without the other.
    static constexpr unsigned int kKernelSize = 64;

    struct PushConstants {
        float radius;       // view-space sample radius (metres)
        float bias;         // depth-compare epsilon, fights self-occlusion
        float intensity;    // 0 = no darkening, 1 = reference, >1 = punchier
        float shadowPower;  // exponent applied to the final AO term
    };

    SsaoPipeline(const Device& device,
                 const Shader& computeShader,
                 VkDescriptorSetLayout descriptorSetLayout);
    ~SsaoPipeline();

    SsaoPipeline(const SsaoPipeline&) = delete;
    SsaoPipeline& operator=(const SsaoPipeline&) = delete;
    SsaoPipeline(SsaoPipeline&&) = delete;
    SsaoPipeline& operator=(SsaoPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
