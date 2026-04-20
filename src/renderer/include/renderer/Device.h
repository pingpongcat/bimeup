#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace bimeup::renderer {

class Device {
public:
    explicit Device(VkInstance instance);
    Device(VkInstance instance, VkSurfaceKHR surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const;
    [[nodiscard]] VkDevice GetDevice() const;
    [[nodiscard]] VkQueue GetGraphicsQueue() const;
    [[nodiscard]] uint32_t GetGraphicsQueueFamily() const;
    [[nodiscard]] VkQueue GetPresentQueue() const;
    [[nodiscard]] uint32_t GetPresentQueueFamily() const;
    [[nodiscard]] VmaAllocator GetAllocator() const;

    /// RP.17.7 — true when the device exposes `VK_EXT_line_rasterization` with
    /// the `smoothLines` feature. Pipelines that want coverage-based line AA
    /// (e.g. the edge overlay) should gate their `smoothLines` pipeline flag on
    /// this. When false, line-rasterization falls back to the default aliased
    /// Bresenham path silently.
    [[nodiscard]] bool HasSmoothLines() const { return m_hasSmoothLines; }

    /// RP.17.7 — true when `VkPhysicalDeviceFeatures.wideLines` is exposed.
    /// Required to pick a `VkPipelineRasterizationStateCreateInfo.lineWidth`
    /// greater than 1.0. Falls back silently to 1-px lines otherwise.
    [[nodiscard]] bool HasWideLines() const { return m_hasWideLines; }

private:
    void CreateAllocator(VkInstance instance);
    void PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void CreateLogicalDevice(bool enableSwapchain);
    void ProbeLineRasterization();
    static int RateDevice(VkPhysicalDevice device);
    static bool FindGraphicsQueueFamily(VkPhysicalDevice device, uint32_t& index);
    static bool FindPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface,
                                       uint32_t& index);

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
    bool m_hasPresentQueue = false;
    bool m_hasSmoothLines = false;
    bool m_hasWideLines = false;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
