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

/// Stage 9.5.a — RT ambient-occlusion visibility pass.
///
/// Writes a per-pixel `R8_UNORM` AO image (1 = fully lit, 0 = fully
/// occluded) by casting a single cosine-weighted-hemisphere ray per pixel
/// with a short max distance (default 1 m, architectural scale). Runs
/// additively alongside the classical XeGTAO compute chain — the Stage-9
/// hybrid composite picks this image when an RT render mode is active;
/// Rasterised mode keeps sampling the XeGTAO output and this pass never
/// fires.
///
/// Strict no-op on non-RT devices: `Build` returns false, the AO image is
/// never allocated, and `Dispatch` is a safe no-op. Mirrors the Stage-9
/// rule that classical rendering stays live on GPUs without RT.
///
/// Shaders:
/// - `rtao.rgen` — reconstructs world-space position + normal from the
///   depth buffer (normal via dFdx/dFdy cross-product of neighbour
///   positions) and traces one cosine-hemisphere ray. Payload stays at
///   the cleared 0 on a hit (occluded) and is set to 1 by the miss
///   shader. The Stage-9.3 `rt_probe.rchit` stub is re-used as the
///   skipped closest-hit so AO rays follow the opaque-any-hit contract.
/// - `rtao.rmiss` — sets payload to 1 when the ray escapes.
///
/// Descriptor set 0 layout (identical to `RtShadowPass`, so a future
/// combined hybrid pass can share a single layout if useful):
///   binding 0 — acceleration structure (TLAS)
///   binding 1 — depth combined-image-sampler
///   binding 2 — AO storage image (R8_UNORM, this pass owns it)
class RtAoPass {
public:
    static constexpr VkFormat kAoFormat = VK_FORMAT_R8_UNORM;

    /// Push-constant block packed to 144 B. Depth-to-world reconstruction
    /// needs both the inverse view-projection (for world position) and
    /// the inverse view (for the view-space-normal → world-space basis
    /// used when orienting the hemisphere). `frameIndex` seeds the hash
    /// RNG so a future 9.5.c temporal pass can average samples; for now
    /// it's consumed as-is and produces a single-sample AO per frame.
    struct PushConstants {
        glm::mat4 invViewProj;
        glm::mat4 invView;
        glm::uvec2 extent;
        float radius = 1.0F;
        uint32_t frameIndex = 0U;
    };

    explicit RtAoPass(const Device& device);
    ~RtAoPass();

    RtAoPass(const RtAoPass&) = delete;
    RtAoPass& operator=(const RtAoPass&) = delete;
    RtAoPass(RtAoPass&&) = delete;
    RtAoPass& operator=(RtAoPass&&) = delete;

    /// Build (or rebuild) for the given viewport extent. Allocates the AO
    /// image + view + sampler, the descriptor pool + set, and the
    /// underlying `RayTracingPipeline`. Returns `false` on non-RT
    /// devices, on missing / invalid SPIR-V, or on pipeline build
    /// failure.
    bool Build(uint32_t width, uint32_t height, const std::string& shaderDir);

    /// Update all per-frame descriptor sets with the given TLAS and depth
    /// resources. Call this once when resources change (e.g., after
    /// building the pass or rebuilding the TLAS), not every frame.
    void UpdateAllDescriptors(VkAccelerationStructureKHR tlas,
                              VkImageView depthView, VkSampler depthSampler);

    /// Record dispatch commands for one frame. Transitions the AO image
    /// to `GENERAL`, binds pipeline + descriptor set, pushes the
    /// constants and issues `vkCmdTraceRaysKHR` at `extent`, then
    /// transitions AO back to `SHADER_READ_ONLY_OPTIMAL` so the composite
    /// pass can sample it. No-op when `IsValid() == false`.
    ///
    /// The caller must have called `UpdateAllDescriptors()` at least once
    /// after building the pass. The caller holds the depth-read barrier:
    /// depth must already be in `SHADER_READ_ONLY_OPTIMAL` when this is
    /// recorded. `radius` is the world-space ray max distance (metres);
    /// `frameIndexRng` drives the per-pixel RNG seed.
    ///
    /// `frameIndex` selects which per-frame descriptor set to bind
    /// (must be < MAX_FRAMES_IN_FLIGHT from RenderLoop).
    void Dispatch(VkCommandBuffer cmd, uint32_t frameIndex,
                  const glm::mat4& view, const glm::mat4& proj,
                  float radius, uint32_t frameIndexRng);

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] VkImage GetAoImage() const { return m_image; }
    [[nodiscard]] VkImageView GetAoImageView() const { return m_imageView; }
    [[nodiscard]] VkSampler GetAoSampler() const { return m_sampler; }
    [[nodiscard]] VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_dsLayout; }
    [[nodiscard]] uint32_t GetWidth() const { return m_width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_height; }

private:
    void LoadDispatch();
    void Reset();
    void CreateAoImage(uint32_t width, uint32_t height);
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
