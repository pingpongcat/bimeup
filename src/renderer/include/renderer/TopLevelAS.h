#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace bimeup::renderer {

class Device;

/// Stage 9.2 — One instance per scene node, referencing a pre-built BLAS
/// (Stage 9.1.b) with a world-space transform. The `transform` is stored
/// as a `glm::mat4` on the CPU side and transposed-trimmed into the 3×4
/// row-major Vulkan form at build time.
struct TlasInstance {
    glm::mat4 transform{1.0F};
    VkDeviceAddress blasAddress = 0;
    uint32_t customIndex = 0;
    uint32_t mask = 0xFF;
};

/// Stage 9.2 — Top-level ray-tracing acceleration structure.
///
/// Built from `TlasInstance`s; each instance wraps a BLAS device address
/// from `AccelerationStructure::GetDeviceAddress`. `Build` rebuilds from
/// scratch — scene edits (node add/remove/transform-change) call `Build`
/// again with the new instance list. Incremental TLAS update is out of
/// scope for Stage 9.
///
/// No-op when `device.HasRayTracing() == false`: `Build` returns false
/// and no Vulkan resources are allocated. Mirrors the Stage-9 rule that
/// RT modes are opt-in — the classical raster path must stay live.
class TopLevelAS {
public:
    explicit TopLevelAS(const Device& device);
    ~TopLevelAS();

    TopLevelAS(const TopLevelAS&) = delete;
    TopLevelAS& operator=(const TopLevelAS&) = delete;
    TopLevelAS(TopLevelAS&&) = delete;
    TopLevelAS& operator=(TopLevelAS&&) = delete;

    /// Build (or rebuild) from the given instance list. Returns true on
    /// success. Returns false (and leaves the TLAS empty) when the device
    /// lacks RT support, when `instances` is empty, or when a rebuild
    /// with an empty list is requested (the TLAS resets to the pre-build
    /// state in that case).
    bool Build(const std::vector<TlasInstance>& instances);

    [[nodiscard]] bool IsValid() const { return m_handle != VK_NULL_HANDLE; }
    [[nodiscard]] VkAccelerationStructureKHR GetHandle() const { return m_handle; }
    [[nodiscard]] VkDeviceAddress GetDeviceAddress() const { return m_address; }
    [[nodiscard]] std::size_t InstanceCount() const { return m_instanceCount; }

private:
    struct RawBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceAddress address = 0;
    };

    void LoadDispatch();
    void Reset();
    RawBuffer CreateRawBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags memProps, const void* data);
    void DestroyRawBuffer(RawBuffer& buf);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    const Device& m_device;
    VkCommandPool m_pool = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_handle = VK_NULL_HANDLE;
    VkDeviceAddress m_address = 0;
    RawBuffer m_storage;
    std::size_t m_instanceCount = 0;

    // Dispatch pointers — null when RT unavailable.
    PFN_vkGetBufferDeviceAddressKHR m_pfnGetBufferAddress = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR m_pfnGetBuildSizes = nullptr;
    PFN_vkCreateAccelerationStructureKHR m_pfnCreateAS = nullptr;
    PFN_vkDestroyAccelerationStructureKHR m_pfnDestroyAS = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR m_pfnCmdBuild = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR m_pfnGetASAddress = nullptr;
};

}  // namespace bimeup::renderer
