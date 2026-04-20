#define VMA_IMPLEMENTATION
#include <renderer/Device.h>
#include <tools/Log.h>

#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace bimeup::renderer {

Device::Device(VkInstance instance) {
    PickPhysicalDevice(instance, VK_NULL_HANDLE);
    ProbeLineRasterization();
    CreateLogicalDevice(false);
    CreateAllocator(instance);
}

Device::Device(VkInstance instance, VkSurfaceKHR surface) {
    PickPhysicalDevice(instance, surface);
    ProbeLineRasterization();
    CreateLogicalDevice(true);
    CreateAllocator(instance);
}

Device::~Device() {
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
    }
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

VkQueue Device::GetPresentQueue() const {
    return m_presentQueue;
}

uint32_t Device::GetPresentQueueFamily() const {
    return m_presentQueueFamily;
}

VmaAllocator Device::GetAllocator() const {
    return m_allocator;
}

void Device::CreateAllocator(VkInstance instance) {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;

    VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

void Device::PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    auto typeName = [](VkPhysicalDeviceType t) {
        switch (t) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "cpu";
            default:                                     return "other";
        }
    };

    long long bestScore = -1;
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        uint32_t graphicsFamily = 0;
        const bool hasGraphics = FindGraphicsQueueFamily(device, graphicsFamily);
        bool hasPresent = true;
        if (hasGraphics && surface != VK_NULL_HANDLE) {
            uint32_t presentFamily = 0;
            hasPresent = FindPresentQueueFamily(device, surface, presentFamily);
        }

        const long long score =
            (hasGraphics && hasPresent) ? RateDevice(device) : -1;
        if (bimeup::tools::Log::GetLogger()) {
            LOG_INFO("Vulkan device: {} ({}) score={}{}", props.deviceName,
                     typeName(props.deviceType), score,
                     (!hasGraphics ? "  [no graphics queue]"
                      : !hasPresent ? "  [no present queue]"
                                    : ""));
        }

        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = device;
            m_graphicsQueueFamily = graphicsFamily;
            if (surface != VK_NULL_HANDLE) {
                FindPresentQueueFamily(device, surface, m_presentQueueFamily);
                m_hasPresentQueue = true;
            }
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable GPU found (need graphics queue)");
    }

    if (bimeup::tools::Log::GetLogger()) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        LOG_INFO("Selected GPU: {} ({})", props.deviceName,
                 typeName(props.deviceType));
    }
}

void Device::CreateLogicalDevice(bool enableSwapchain) {
    float queuePriority = 1.0F;

    std::set<uint32_t> uniqueFamilies = {m_graphicsQueueFamily};
    if (m_hasPresentQueue) {
        uniqueFamilies.insert(m_presentQueueFamily);
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    // Required because MRT-targeted pipelines (transparent, section-fill,
    // disk-marker) use different blend states per attachment — colour
    // attachment[0] blends, secondary normal/stencil attachments don't —
    // and the spec requires this feature for non-uniform blend state.
    deviceFeatures.independentBlend = VK_TRUE;
    // RP.17.7 — enable wideLines so the edge overlay can draw at >1 px for
    // better readability; guarded by the physical-device probe.
    if (m_hasWideLines) {
        deviceFeatures.wideLines = VK_TRUE;
    }

    std::vector<const char*> enabledExtensions;
    if (enableSwapchain) {
        enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (m_hasSmoothLines) {
        enabledExtensions.push_back(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);
    }

    // Chain `smoothLines` into the feature struct so the EdgeOverlayPipeline
    // can ask for `RECTANGULAR_SMOOTH_EXT` line rasterization. Only chained
    // when the device supports it; `m_hasSmoothLines` was negotiated in
    // `ProbeLineRasterization` against the physical device's capabilities.
    VkPhysicalDeviceLineRasterizationFeaturesEXT lineFeatures{};
    lineFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
    lineFeatures.smoothLines = m_hasSmoothLines ? VK_TRUE : VK_FALSE;
    lineFeatures.rectangularLines = m_hasSmoothLines ? VK_TRUE : VK_FALSE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames =
        enabledExtensions.empty() ? nullptr : enabledExtensions.data();
    if (m_hasSmoothLines) {
        createInfo.pNext = &lineFeatures;
    }

    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan logical device");
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    if (m_hasPresentQueue) {
        vkGetDeviceQueue(m_device, m_presentQueueFamily, 0, &m_presentQueue);
    }
}

int Device::RateDevice(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    // Strong preference for discrete GPUs on hybrid systems (Optimus/PRIME):
    // dedicated VRAM + full throughput beats integrated in every realistic
    // BIM-viewer workload. Among multiple discretes, tiebreak by local VRAM.
    int score = 0;
    switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score = 100000; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score = 1000;   break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    score = 100;    break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            score = 10;     break;
        default:                                     score = 1;      break;
    }

    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(device, &mem);
    VkDeviceSize localHeapBytes = 0;
    for (uint32_t i = 0; i < mem.memoryHeapCount; ++i) {
        if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            localHeapBytes += mem.memoryHeaps[i].size;
        }
    }
    score += static_cast<int>(localHeapBytes >> 30);  // +1 per GiB of VRAM

    return score;
}

void Device::ProbeLineRasterization() {
    // Probe the core `wideLines` feature up-front — independent of the
    // line-rasterization extension below, but queried here for locality.
    VkPhysicalDeviceFeatures baseFeatures{};
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &baseFeatures);
    m_hasWideLines = (baseFeatures.wideLines == VK_TRUE);

    // Query device extensions for `VK_EXT_line_rasterization`; if present,
    // chain the feature query to see if `smoothLines` is supported. Both
    // must be true to enable coverage-based line AA on the edge overlay.
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> extProps(extCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, extProps.data());

    bool extSupported = false;
    for (const auto& ext : extProps) {
        if (std::string_view(ext.extensionName) == VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME) {
            extSupported = true;
            break;
        }
    }
    if (!extSupported) {
        if (bimeup::tools::Log::GetLogger()) {
            LOG_INFO("Edge overlay AA: VK_EXT_line_rasterization not supported "
                     "— falling back to aliased 1-px lines");
        }
        return;
    }

    VkPhysicalDeviceLineRasterizationFeaturesEXT lineFeatures{};
    lineFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &lineFeatures;
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);

    m_hasSmoothLines =
        (lineFeatures.smoothLines == VK_TRUE) && (lineFeatures.rectangularLines == VK_TRUE);
    if (bimeup::tools::Log::GetLogger()) {
        LOG_INFO("Edge overlay AA: {}",
                 m_hasSmoothLines ? "VK_EXT_line_rasterization smoothLines enabled"
                                  : "smoothLines feature unavailable — aliased lines");
    }
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

bool Device::FindPresentQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface,
                                    uint32_t& index) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            index = i;
            return true;
        }
    }
    return false;
}

}  // namespace bimeup::renderer
