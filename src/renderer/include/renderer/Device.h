#pragma once

#include <vulkan/vulkan.h>

namespace bimeup::renderer {

class Device {
public:
    explicit Device(VkInstance instance);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const;
    [[nodiscard]] VkDevice GetDevice() const;
    [[nodiscard]] VkQueue GetGraphicsQueue() const;
    [[nodiscard]] uint32_t GetGraphicsQueueFamily() const;

private:
    void PickPhysicalDevice(VkInstance instance);
    void CreateLogicalDevice();
    static int RateDevice(VkPhysicalDevice device);
    static bool FindGraphicsQueueFamily(VkPhysicalDevice device, uint32_t& index);

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
};

}  // namespace bimeup::renderer
