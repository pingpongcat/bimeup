#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

namespace bimeup::renderer {

class Device;

/// Stage 9.3 — Settings for a ray-tracing pipeline build.
///
/// SBT layout baked from these paths is fixed at one raygen / one miss /
/// one hit group; `anyHitPath` is optional (empty → hit group is
/// closest-hit only). The pipeline layout is built from the caller's
/// descriptor-set layout and an optional push-constant range spanning the
/// raygen + hit stages. Additional hit-group variants (shadow any-hit,
/// transmission any-hit, etc.) will be added as Stage 9 progresses; the
/// SBT-region accessors keep the `vkCmdTraceRaysKHR` call shape stable.
struct RayTracingPipelineSettings {
    std::string raygenPath;
    std::string missPath;
    std::string closestHitPath;
    std::string anyHitPath;  // optional — empty = no any-hit shader
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    uint32_t pushConstantSize = 0;
    VkShaderStageFlags pushConstantStages =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    uint32_t maxRayRecursionDepth = 1;
};

/// Stage 9.3 — Ray-tracing pipeline + Shader Binding Table.
///
/// Owns one raygen / one miss / one hit group (closest-hit + optional
/// any-hit), a pipeline layout, and a device-local SBT buffer. `Build`
/// rebuilds from scratch; calling it again with different settings tears
/// down the old pipeline and SBT. No-op when `device.HasRayTracing() ==
/// false` — `Build` returns false, no Vulkan resources are allocated, and
/// the SBT regions all report `deviceAddress == 0`, `size == 0`. Mirrors
/// the Stage-9 rule that the classical raster path stays live when RT is
/// unavailable.
///
/// Built lazily by the render-mode switch (RenderLoop wires this in a
/// later 9.x task): a user on a non-RT GPU will never see `Build` fire.
class RayTracingPipeline {
public:
    explicit RayTracingPipeline(const Device& device);
    ~RayTracingPipeline();

    RayTracingPipeline(const RayTracingPipeline&) = delete;
    RayTracingPipeline& operator=(const RayTracingPipeline&) = delete;
    RayTracingPipeline(RayTracingPipeline&&) = delete;
    RayTracingPipeline& operator=(RayTracingPipeline&&) = delete;

    /// Build (or rebuild) the pipeline + SBT. Returns true on success,
    /// false when RT is unavailable, a required shader path is empty, or
    /// a SPIR-V file can't be read.
    bool Build(const RayTracingPipelineSettings& settings);

    [[nodiscard]] bool IsValid() const { return m_pipeline != VK_NULL_HANDLE; }
    [[nodiscard]] VkPipeline GetPipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

    /// SBT regions, ready for `vkCmdTraceRaysKHR`. All zero when !IsValid().
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& GetRaygenRegion() const { return m_raygenRegion; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& GetMissRegion() const { return m_missRegion; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& GetHitRegion() const { return m_hitRegion; }
    [[nodiscard]] const VkStridedDeviceAddressRegionKHR& GetCallableRegion() const { return m_callableRegion; }

private:
    struct RawBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceAddress address = 0;
    };

    void LoadDispatch();
    void Reset();
    static std::vector<uint32_t> ReadSpirv(const std::string& path);
    VkShaderModule CreateModule(const std::vector<uint32_t>& spirv);
    RawBuffer CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags memProps, const void* data);
    void DestroyRawBuffer(RawBuffer& buf);
    [[nodiscard]] uint32_t FindMemoryType(uint32_t typeFilter,
                                          VkMemoryPropertyFlags properties) const;

    const Device& m_device;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    RawBuffer m_sbt;

    VkStridedDeviceAddressRegionKHR m_raygenRegion{};
    VkStridedDeviceAddressRegionKHR m_missRegion{};
    VkStridedDeviceAddressRegionKHR m_hitRegion{};
    VkStridedDeviceAddressRegionKHR m_callableRegion{};

    // Dispatch pointers — null when RT unavailable.
    PFN_vkCreateRayTracingPipelinesKHR m_pfnCreatePipelines = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR m_pfnGetGroupHandles = nullptr;
    PFN_vkGetBufferDeviceAddressKHR m_pfnGetBufferAddress = nullptr;
};

}  // namespace bimeup::renderer
