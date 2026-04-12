#include <renderer/Device.h>
#include <tools/Log.h>

#include <stdexcept>
#include <vector>

namespace bimeup::renderer {

Device::Device(VkInstance instance) {
    PickPhysicalDevice(instance);
    CreateLogicalDevice();
}

Device::~Device() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
}

VkPhysicalDevice Device::GetPhysicalDevice() const {
    return m_physicalDevice;
}

VkDevice Device::GetDevice() const {
    return m_device;
}

VkQueue Device::GetGraphicsQueue() const {
    return m_graphicsQueue;
}

uint32_t Device::GetGraphicsQueueFamily() const {
    return m_graphicsQueueFamily;
}

void Device::PickPhysicalDevice(VkInstance instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    int bestScore = -1;
    for (const auto& device : devices) {
        uint32_t graphicsFamily = 0;
        if (!FindGraphicsQueueFamily(device, graphicsFamily)) {
            continue;
        }

        int score = RateDevice(device);
        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = device;
            m_graphicsQueueFamily = graphicsFamily;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable GPU found (need graphics queue)");
    }

    if (bimeup::tools::Log::GetLogger()) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        LOG_INFO("Selected GPU: {}", props.deviceName);
    }
}

void Device::CreateLogicalDevice() {
    float queuePriority = 1.0F;

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 0;
    createInfo.enabledLayerCount = 0;

    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan logical device");
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
}

int Device::RateDevice(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 100;
    }

    return score;
}

bool Device::FindGraphicsQueueFamily(VkPhysicalDevice device, uint32_t& index) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, families.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            index = i;
            return true;
        }
    }
    return false;
}

}  // namespace bimeup::renderer
