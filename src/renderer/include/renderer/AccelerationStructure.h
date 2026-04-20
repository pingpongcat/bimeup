#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <unordered_map>

namespace bimeup::renderer {

class Device;
struct MeshData;

/// Stage 9.1.b — BLAS-per-mesh ray-tracing acceleration structures.
///
/// When `device.HasRayTracing() == false` the builder is a strict no-op:
/// `BuildBlas` returns `InvalidHandle` without touching the GPU and no
/// Vulkan resources are allocated. This mirrors the Stage-9 rule that the
/// classical raster path stays live and untouched when RT is unavailable.
///
/// On RT-capable devices `BuildBlas` builds a single bottom-level AS from
/// the mesh's triangle soup, submits the build on the graphics queue and
/// waits for completion synchronously (TLAS assembly in 9.2 will reuse the
/// cached handle + device address).
///
/// The `VK_KHR_acceleration_structure` entry points are loaded lazily via
/// `vkGetDeviceProcAddr` — the Vulkan loader does not provide them
/// statically and forcing the static link would break on non-RT drivers.
class AccelerationStructure {
public:
    using BlasHandle = uint32_t;
    static constexpr BlasHandle InvalidHandle = 0;

    explicit AccelerationStructure(const Device& device);
    ~AccelerationStructure();

    AccelerationStructure(const AccelerationStructure&) = delete;
    AccelerationStructure& operator=(const AccelerationStructure&) = delete;
    AccelerationStructure(AccelerationStructure&&) = delete;
    AccelerationStructure& operator=(AccelerationStructure&&) = delete;

    /// Build a BLAS from a triangle-indexed `MeshData`. Returns
    /// `InvalidHandle` when RT is unavailable or `data` is empty.
    BlasHandle BuildBlas(const MeshData& data);

    [[nodiscard]] bool IsValid(BlasHandle handle) const;
    [[nodiscard]] VkAccelerationStructureKHR GetHandle(BlasHandle handle) const;
    [[nodiscard]] VkDeviceAddress GetDeviceAddress(BlasHandle handle) const;
    [[nodiscard]] std::size_t BlasCount() const;

private:
    struct RawBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceAddress address = 0;
    };

    struct Blas {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        VkDeviceAddress address = 0;
        RawBuffer storage;
    };

    void LoadDispatch();
    RawBuffer CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags memProps, const void* data);
    void DestroyRawBuffer(RawBuffer& buf);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    const Device& m_device;
    VkCommandPool m_pool = VK_NULL_HANDLE;
    BlasHandle m_nextHandle = 1;
    std::unordered_map<BlasHandle, Blas> m_blasMap;

    // Dispatch pointers — null when RT unavailable.
    PFN_vkGetBufferDeviceAddressKHR m_pfnGetBufferAddress = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR m_pfnGetBuildSizes = nullptr;
    PFN_vkCreateAccelerationStructureKHR m_pfnCreateAS = nullptr;
    PFN_vkDestroyAccelerationStructureKHR m_pfnDestroyAS = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR m_pfnCmdBuild = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR m_pfnGetASAddress = nullptr;
};

}  // namespace bimeup::renderer
