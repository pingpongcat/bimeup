#define VMA_IMPLEMENTATION
#include <renderer/Device.h>
#include <tools/Log.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace bimeup::renderer {

Device::Device(VkInstance instance) {
    PickPhysicalDevice(instance, VK_NULL_HANDLE, std::nullopt);
    ProbeLineRasterization();
    ProbeRayTracing();
    CreateLogicalDevice(false);
    CreateAllocator(instance);
}

Device::Device(VkInstance instance, VkSurfaceKHR surface) {
    PickPhysicalDevice(instance, surface, std::nullopt);
    ProbeLineRasterization();
    ProbeRayTracing();
    CreateLogicalDevice(true);
    CreateAllocator(instance);
}

Device::Device(VkInstance instance, VkSurfaceKHR surface,
               std::optional<std::uint32_t> deviceIndexOverride) {
    PickPhysicalDevice(instance, surface, deviceIndexOverride);
    ProbeLineRasterization();
    ProbeRayTracing();
    CreateLogicalDevice(surface != VK_NULL_HANDLE);
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

void Device::PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
                                std::optional<std::uint32_t> deviceIndexOverride) {
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
    std::uint32_t bestIndex = 0;
    for (std::uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDevice device = devices[i];
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
            LOG_INFO("Vulkan device #{}: {} ({}) score={}{}", i, props.deviceName,
                     typeName(props.deviceType), score,
                     (!hasGraphics ? "  [no graphics queue]"
                      : !hasPresent ? "  [no present queue]"
                                    : ""));
        }

        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    std::uint32_t chosenIndex = bestIndex;
    if (deviceIndexOverride.has_value()) {
        const std::uint32_t wanted = *deviceIndexOverride;
        if (wanted >= deviceCount) {
            throw std::runtime_error(
                "--device-id " + std::to_string(wanted) +
                " is out of range (0.." + std::to_string(deviceCount - 1) + ")");
        }
        VkPhysicalDevice device = devices[wanted];
        uint32_t graphicsFamily = 0;
        if (!FindGraphicsQueueFamily(device, graphicsFamily)) {
            throw std::runtime_error(
                "--device-id " + std::to_string(wanted) +
                " picks a device with no graphics queue");
        }
        if (surface != VK_NULL_HANDLE) {
            uint32_t presentFamily = 0;
            if (!FindPresentQueueFamily(device, surface, presentFamily)) {
                throw std::runtime_error(
                    "--device-id " + std::to_string(wanted) +
                    " picks a device that cannot present to the window surface");
            }
        }
        chosenIndex = wanted;
    } else if (bestScore < 0) {
        throw std::runtime_error("No suitable GPU found (need graphics queue)");
    }

    m_physicalDevice = devices[chosenIndex];
    FindGraphicsQueueFamily(m_physicalDevice, m_graphicsQueueFamily);
    if (surface != VK_NULL_HANDLE) {
        FindPresentQueueFamily(m_physicalDevice, surface, m_presentQueueFamily);
        m_hasPresentQueue = true;
    }

    if (bimeup::tools::Log::GetLogger()) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        LOG_INFO("Selected GPU #{}: {} ({}){}", chosenIndex, props.deviceName,
                 typeName(props.deviceType),
                 deviceIndexOverride.has_value() ? "  [--device-id override]" : "");
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

    // Collect every enabled extension first so the vector never reallocates
    // after `ppEnabledExtensionNames` captures `.data()`. Growing the vector
    // after that pointer is read dangles it and segfaults `vk_string_validate`.
    std::vector<const char*> enabledExtensions;
    if (enableSwapchain) {
        enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (m_hasSmoothLines) {
        enabledExtensions.push_back(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);
    }
    if (m_hasRayTracing) {
        // `VK_KHR_acceleration_structure` pulls in `deferred_host_operations`,
        // `bufferDeviceAddress` (1.2 core) and `descriptorIndexing` (1.2 core)
        // as hard prerequisites. BDA and descriptor_indexing flow through
        // `VkPhysicalDeviceVulkan12Features` below — NVIDIA rejects the
        // promoted extension strings on a 1.2+ instance.
        enabledExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        enabledExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        enabledExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
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

    // Stage 9.1.a — opt-in RT feature chain. Populated only when
    // `ProbeRayTracing` succeeded. `VkPhysicalDeviceVulkan12Features` carries
    // the promoted `bufferDeviceAddress` + `descriptorIndexing` meta-features;
    // `accelerationStructure` + `rayTracingPipeline` come from their KHR
    // feature structs.
    VkPhysicalDeviceVulkan12Features v12Features{};
    v12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    v12Features.bufferDeviceAddress = VK_TRUE;
    v12Features.descriptorIndexing = VK_TRUE;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.rayTracingPipeline = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames =
        enabledExtensions.empty() ? nullptr : enabledExtensions.data();

    // Build the pNext chain head-first. Multiple feature groups may be live
    // simultaneously (smooth-lines + RT are independent capabilities on some
    // GPUs), so we walk a `head` pointer rather than hard-wiring one branch.
    void* featureChainHead = nullptr;
    auto prependFeature = [&](auto& feature) {
        feature.pNext = featureChainHead;
        featureChainHead = &feature;
    };
    if (m_hasSmoothLines) {
        prependFeature(lineFeatures);
    }
    if (m_hasRayTracing) {
        prependFeature(v12Features);
        prependFeature(asFeatures);
        prependFeature(rtFeatures);
    }
    createInfo.pNext = featureChainHead;

    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS && m_hasRayTracing) {
        // Stage 9.1.a — NVIDIA's driver advertises every RT feature but some
        // versions (observed on 595.44 in headless setups) still reject the
        // logical-device create with `VK_ERROR_INITIALIZATION_FAILED`. The
        // probe is optimistic by design; when the device truly can't start
        // with RT on, we quietly drop it and retry without, so the raster
        // path keeps working. `HasRayTracing()` then reflects reality.
        if (bimeup::tools::Log::GetLogger()) {
            LOG_WARN("Ray tracing: vkCreateDevice rejected RT extensions "
                     "(VkResult={}); retrying without RT", static_cast<int>(result));
        }
        m_hasRayTracing = false;
        enabledExtensions.erase(
            std::remove_if(enabledExtensions.begin(), enabledExtensions.end(),
                [](const char* name) {
                    std::string_view n(name);
                    return n == VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ||
                           n == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME ||
                           n == VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
                }),
            enabledExtensions.end());
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames =
            enabledExtensions.empty() ? nullptr : enabledExtensions.data();
        featureChainHead = nullptr;
        if (m_hasSmoothLines) {
            prependFeature(lineFeatures);
        }
        createInfo.pNext = featureChainHead;
        result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    }
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

void Device::ProbeRayTracing() {
    // Stage 9.1.a — probe for the Stage-9 ray-tracing render modes. The probe
    // is strictly a runtime query: when it reports false the logical device
    // is built without RT extensions and the classical raster path runs as
    // today. We require:
    //   - `VK_KHR_acceleration_structure` + `VK_KHR_ray_tracing_pipeline` +
    //     `VK_KHR_deferred_host_operations` as extension strings (the first
    //     two are the actual RT feature carriers; the third is an AS
    //     prerequisite).
    //   - `VkPhysicalDeviceVulkan12Features.bufferDeviceAddress` +
    //     `.descriptorIndexing` as features. These were promoted to core in
    //     1.2 and the NVIDIA driver rejects them as extension strings on a
    //     1.2+ instance — the core feature struct is the supported path.
    //   - `accelerationStructure` and `rayTracingPipeline` feature bits.
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> extProps(extCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, extProps.data());

    auto hasExt = [&](std::string_view name) {
        for (const auto& ext : extProps) {
            if (std::string_view(ext.extensionName) == name) return true;
        }
        return false;
    };

    const bool extsPresent =
        hasExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
        hasExt(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) &&
        hasExt(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

    if (!extsPresent) {
        if (bimeup::tools::Log::GetLogger()) {
            LOG_INFO("Ray tracing: one or more required extensions missing — "
                     "RT render modes will be disabled");
        }
        return;
    }

    VkPhysicalDeviceVulkan12Features v12Features{};
    v12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.pNext = &v12Features;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.pNext = &asFeatures;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &rtFeatures;
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);

    m_hasRayTracing =
        (asFeatures.accelerationStructure == VK_TRUE) &&
        (rtFeatures.rayTracingPipeline == VK_TRUE) &&
        (v12Features.bufferDeviceAddress == VK_TRUE);

    if (bimeup::tools::Log::GetLogger()) {
        LOG_INFO("Ray tracing: {}",
                 m_hasRayTracing ? "VK_KHR_acceleration_structure + "
                                   "ray_tracing_pipeline enabled"
                                 : "required features unavailable — "
                                   "RT render modes will be disabled");
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
