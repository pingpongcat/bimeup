#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace bimeup::renderer {

class Device;
class RayTracingPipeline;

/// Stage 9.4.a — RT sun-shadow visibility pass.
///
/// Writes a per-pixel `R8_UNORM` visibility image (1 = lit, 0 = occluded)
/// by tracing one shadow ray toward the sun from the world-space position
/// reconstructed at each fragment from the depth buffer. Runs additively
/// alongside the classical `ShadowMap` + PCF path — the Stage-9 hybrid
/// composite picks this image when an RT render mode is active;
/// Rasterised mode keeps sampling the shadow map and this pass never
/// fires.
///
/// Strict no-op on non-RT devices: `Build` returns false, the visibility
/// image is never allocated, and `Dispatch` is a safe no-op. Mirrors the
/// Stage-9 rule that classical rendering stays live on GPUs without RT.
///
/// Shaders (payload is a transmission accumulator — `1` = lit, `0` = blocked):
/// - `shadow.rgen` — seeds `payload = 1`, traces with `TerminateOnFirstHit`.
/// - `shadow.rchit` — accepted opaque hit writes `payload = 0`.
/// - `shadow.rahit` — glass any-hit multiplies the pass-through factor
///   and `ignoreIntersectionEXT`s so sun light continues through windows.
/// - `shadow.rmiss` — no-op; accumulated transmission IS the result.
///
/// Descriptor set 0 layout:
///   binding 0 — acceleration structure (TLAS)
///   binding 1 — depth combined-image-sampler (caller provides view +
///               sampler; expected to be in SHADER_READ_ONLY_OPTIMAL)
///   binding 2 — visibility storage image (R8_UNORM, this pass owns it)
class RtShadowPass {
public:
    static constexpr VkFormat kVisibilityFormat = VK_FORMAT_R8_UNORM;

    /// Push-constant block packed to 96 B (≤ the 128-byte Vulkan
    /// guarantee): one inverse view-projection for depth-to-world
    /// reconstruction, sun direction, image extent + 2×4-byte padding to
    /// keep the block 16-byte-aligned.
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::vec4 sunDirWorld;  // xyz = travel direction (DirectionalLight convention)
        glm::uvec2 extent;
        uint32_t pad0 = 0;
        uint32_t pad1 = 0;
    };

    explicit RtShadowPass(const Device& device);
    ~RtShadowPass();

    RtShadowPass(const RtShadowPass&) = delete;
    RtShadowPass& operator=(const RtShadowPass&) = delete;
    RtShadowPass(RtShadowPass&&) = delete;
    RtShadowPass& operator=(RtShadowPass&&) = delete;

    /// Build (or rebuild) for the given viewport extent. Allocates the
    /// visibility image + view + sampler, the descriptor pool + set, and
    /// the underlying `RayTracingPipeline`. `shaderDir` supplies the
    /// compiled SPIR-V directory (same value the rest of the renderer
    /// reads from `BIMEUP_SHADER_DIR`). Returns `false` on non-RT devices,
    /// on missing / invalid SPIR-V, or on pipeline build failure.
    bool Build(uint32_t width, uint32_t height, const std::string& shaderDir);

    /// Update all per-frame descriptor sets with the given TLAS and depth
    /// resources. Call this once when resources change (e.g., after
    /// building the pass or rebuilding the TLAS), not every frame.
    void UpdateAllDescriptors(VkAccelerationStructureKHR tlas,
                              VkImageView depthView, VkSampler depthSampler);

    /// Record dispatch commands for one frame. Transitions the visibility
    /// image to `GENERAL`, binds the pipeline + descriptor set, pushes the
    /// invViewProj + sunDir + extent block, issues `vkCmdTraceRaysKHR` at
    /// `extent`, then transitions visibility back to
    /// `SHADER_READ_ONLY_OPTIMAL` so downstream composite can sample it.
    /// No-op when `IsValid() == false`.
    ///
    /// The caller must have called `UpdateAllDescriptors()` at least once
    /// after building the pass. The caller holds the depth-read barrier:
    /// depth must already be in `SHADER_READ_ONLY_OPTIMAL` when this is
    /// recorded. `sunDirWorld` follows the `DirectionalLight::direction`
    /// convention — it is the direction sunlight TRAVELS through the
    /// scene (points away from the sun). The raygen inverts it to shoot
    /// rays toward the light source.
    ///
    /// `frameIndex` selects which per-frame descriptor set to bind
    /// (must be < MAX_FRAMES_IN_FLIGHT from RenderLoop).
    void Dispatch(VkCommandBuffer cmd, uint32_t frameIndex,
                  const glm::mat4& view, const glm::mat4& proj,
                  const glm::vec3& sunDirWorld);

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] VkImage GetVisibilityImage() const { return m_image; }
    [[nodiscard]] VkImageView GetVisibilityImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler GetVisibilitySampler() const { return m_sampler; }
    [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_dsLayout; }
    [[nodiscard]] uint32_t GetWidth() const { return m_width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_height; }

private:
    void LoadDispatch();
    void Reset();
    void CreateVisibilityImage(uint32_t width, uint32_t height);
    void CreateDescriptor();
    void UpdateDescriptor(uint32_t frameIndex, VkAccelerationStructureKHR tlas,
                          VkImageView depthView, VkSampler depthSampler);

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    const Device& m_device;

    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkImageLayout m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    VkDescriptorSetLayout m_dsLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_dsPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSets[MAX_FRAMES_IN_FLIGHT] = {};

    std::unique_ptr<RayTracingPipeline> m_pipeline;

    PFN_vkCmdTraceRaysKHR m_pfnTraceRays = nullptr;
};

}  // namespace bimeup::renderer
