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

/// Stage 9.7.a — RT indoor-fill visibility pass.
///
/// Writes a per-pixel `R8_UNORM` visibility image (1 = lit by the
/// overhead fill, 0 = occluded by a wall or ceiling) by tracing one
/// shadow ray toward the overhead fill's inverted travel direction from
/// each depth-reconstructed fragment. Only runs when the scene's
/// `indoorLightsEnabled` is on (the caller gates the dispatch) — raster
/// mode keeps the cheap directional fill from `Lighting.cpp`
/// (RP.16.5), the RT hybrid composite (9.8) will route to this image
/// when `HybridRt` is selected *and* indoor lights are enabled.
///
/// Strict no-op on non-RT devices: `Build` returns false, the
/// visibility image is never allocated, and `Dispatch` is a safe no-op.
/// Mirrors the Stage-9 rule that classical rendering stays live on GPUs
/// without RT.
///
/// Shaders (binary sun-style shadow ray — transmission-through-glass is
/// not modelled for indoor fill; walls are hard occluders):
/// - `rt_indoor.rgen` — seeds `payload = 0`, reconstructs world pos
///   from depth, shoots a ray along `-fillDirWorld` with
///   `TerminateOnFirstHit | SkipClosestHitShader`, `payload = 1` comes
///   only from the miss shader.
/// - `rt_indoor.rmiss` — sets payload to 1 when the ray escapes.
/// - `rt_probe.rchit` — re-used Stage-9.3 stub (skipped by ray flag).
///
/// Descriptor set 0 layout (identical to `RtShadowPass` /
/// `RtAoPass`, so future passes can share a layout):
///   binding 0 — acceleration structure (TLAS)
///   binding 1 — depth combined-image-sampler
///   binding 2 — visibility storage image (R8_UNORM, this pass owns it)
class RtIndoorPass {
public:
    static constexpr VkFormat kVisibilityFormat = VK_FORMAT_R8_UNORM;

    /// Push-constant block packed to 96 B (≤ the 128-byte Vulkan
    /// guarantee). Shape matches `RtShadowPass::PushConstants` exactly
    /// — the indoor fill is geometrically a sun-style directional
    /// light, just with a different travel direction and a
    /// building-scale max-distance in the raygen.
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::vec4 fillDirWorld;  // xyz = travel direction (light→scene)
        glm::uvec2 extent;
        uint32_t pad0 = 0;
        uint32_t pad1 = 0;
    };

    explicit RtIndoorPass(const Device& device);
    ~RtIndoorPass();

    RtIndoorPass(const RtIndoorPass&) = delete;
    RtIndoorPass& operator=(const RtIndoorPass&) = delete;
    RtIndoorPass(RtIndoorPass&&) = delete;
    RtIndoorPass& operator=(RtIndoorPass&&) = delete;

    /// Build (or rebuild) for the given viewport extent. Allocates the
    /// visibility image + view + sampler, the descriptor pool + set,
    /// and the underlying `RayTracingPipeline`. Returns `false` on
    /// non-RT devices, on missing / invalid SPIR-V, or on pipeline
    /// build failure.
    bool Build(uint32_t width, uint32_t height, const std::string& shaderDir);

    /// Update all per-frame descriptor sets with the given TLAS and depth
    /// resources. Call this once when resources change (e.g., after
    /// building the pass or rebuilding the TLAS), not every frame.
    void UpdateAllDescriptors(VkAccelerationStructureKHR tlas,
                              VkImageView depthView, VkSampler depthSampler);

    /// Record dispatch commands for one frame. Transitions the
    /// visibility image to `GENERAL`, binds pipeline + descriptor set,
    /// pushes the constants and issues `vkCmdTraceRaysKHR` at
    /// `extent`, then transitions visibility back to
    /// `SHADER_READ_ONLY_OPTIMAL` so the composite pass can sample it.
    /// No-op when `IsValid() == false`.
    ///
    /// The caller must have called `UpdateAllDescriptors()` at least once
    /// after building the pass. The caller holds the depth-read barrier:
    /// depth must already be in `SHADER_READ_ONLY_OPTIMAL` when this is
    /// recorded. `fillDirWorld` follows the `DirectionalLight::direction`
    /// convention — it is the direction the fill light travels through
    /// the scene (points *away* from the virtual overhead lamp). The
    /// raygen inverts it to shoot shadow rays toward the light.
    ///
    /// `frameIndex` selects which per-frame descriptor set to bind
    /// (must be < MAX_FRAMES_IN_FLIGHT from RenderLoop).
    void Dispatch(VkCommandBuffer cmd, uint32_t frameIndex,
                  const glm::mat4& view, const glm::mat4& proj,
                  const glm::vec3& fillDirWorld);

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
