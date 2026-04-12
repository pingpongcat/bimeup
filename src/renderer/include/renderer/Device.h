#pragma once

#include <vulkan/vulkan.h>

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

private:
    void PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    void CreateLogicalDevice(bool enableSwapchain);
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
};

}  // namespace bimeup::renderer
