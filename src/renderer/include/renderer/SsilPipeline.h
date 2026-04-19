#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device;
class Shader;

/// Compute pipeline for RP.7b — screen-space indirect lighting. Samples a
/// hemisphere kernel in view space, reprojects each tap into the **previous**
/// frame's HDR target via the reprojection matrix from
/// `renderer::ComputeReprojectionMatrix` (RP.7a), and accumulates a
/// normal-rejected one-bounce indirect colour into a half-res RGBA16F target.
/// Companion pass to `SsaoPipeline`: same Chapman-style hemisphere sampling,
/// but colour-carrying and temporally-reprojected. Port of the Intel/Godot
/// `ssil.glsl` reference.
///
/// Pipeline shape mirrors `SsaoPipeline`: owns its own `VkPipelineLayout` +
/// `VkPipeline` directly (the graphics-only `renderer::Pipeline` doesn't
/// cover compute); deleted copy/move; `Bind(cmd)` at
/// `VK_PIPELINE_BIND_POINT_COMPUTE`; layout cleanup on ctor failure.
///
/// Expected descriptor set (caller-owned, bound at set 0):
///   binding 0: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — linear depth
///              pyramid (RP.4, full chain, sampled via `textureLod`).
///   binding 1: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — oct-packed
///              view-space normal G-buffer (R16G16_SNORM from RP.3).
///   binding 2: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — previous-frame
///              HDR target (`RenderLoop::HDR_FORMAT`, RGBA16F). Cleared to 0
///              on first frame / MSAA toggle so the initial contribution is
///              "no bounce" rather than garbage.
///   binding 3: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER — `SsilUbo`:
///                 mat4  proj;            // view-pos → UV (current frame)
///                 mat4  invProj;         // UV + linDepth → view-pos (curr)
///                 mat4  reprojection;    // prevViewProj * currInvViewProj
///                 vec4  kernel[64];      // .xyz = +z-hemisphere sample
///   binding 4: VK_DESCRIPTOR_TYPE_STORAGE_IMAGE — half-res RGBA16F SSIL
///              output (`rgba16f` write-only).
///   binding 5: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER — outline stencil
///              G-buffer (R8_UINT, sampled as `usampler2D`, RP.12c.2
///              transparency gate). Bit 4 on a sampled tap means "translucent
///              surface" and skips that tap's contribution. NEAREST sampler:
///              integer formats don't support linear filtering.
///
/// Kernel count is a compile-time constant (64) mirrored on both sides to
/// keep the UBO layout fixed; `renderer::GenerateHemisphereKernel(64, seed)`
/// from RP.5a supplies the CPU source. (SSAO retired its hemisphere kernel
/// with the RP.12e XeGTAO swap — SSIL is now the sole consumer.)
///
/// Dispatch tiles are 8×8 sized to the **half-res** output:
/// `ceil(halfSize.xy / 8)`.
class SsilPipeline {
public:
    // Compile-time kernel size — mirrors the `SSIL_KERNEL_SIZE` #define in
    // `ssil_main.comp`. Don't change one without the other.
    static constexpr unsigned int kKernelSize = 64;

    struct PushConstants {
        float radius;           // view-space sample radius (metres)
        float intensity;        // scale on the accumulated indirect colour
        float normalRejection;  // exponent for dot(nCurr, nSampled) lobe
        float frameSeed;        // per-frame perturbation for the IGN rotation
        float maxLuminance;     // RP.12c: per-channel post-accumulation cap
    };

    SsilPipeline(const Device& device,
                 const Shader& computeShader,
                 VkDescriptorSetLayout descriptorSetLayout);
    ~SsilPipeline();

    SsilPipeline(const SsilPipeline&) = delete;
    SsilPipeline& operator=(const SsilPipeline&) = delete;
    SsilPipeline(SsilPipeline&&) = delete;
    SsilPipeline& operator=(SsilPipeline&&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
