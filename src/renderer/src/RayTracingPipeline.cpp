#include <renderer/Device.h>
#include <renderer/RayTracingPipeline.h>

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace bimeup::renderer {

namespace {

constexpr uint32_t kShaderUnused = VK_SHADER_UNUSED_KHR;

// Align `value` up to `alignment` (power-of-two). Used for SBT stride /
// base-address alignment requirements from the RT pipeline properties.
uint32_t AlignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

VkPhysicalDeviceRayTracingPipelinePropertiesKHR QueryRtProps(const Device& device) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(device.GetPhysicalDevice(), &props2);
    return rtProps;
}

}  // namespace

RayTracingPipeline::RayTracingPipeline(const Device& device) : m_device(device) {
    if (!m_device.HasRayTracing()) {
        return;
    }
    LoadDispatch();
}

RayTracingPipeline::~RayTracingPipeline() {
    Reset();
}

void RayTracingPipeline::LoadDispatch() {
    VkDevice dev = m_device.GetDevice();
    auto load = [dev](const char* name) -> PFN_vkVoidFunction {
        PFN_vkVoidFunction p = vkGetDeviceProcAddr(dev, name);
        if (p == nullptr) {
            throw std::runtime_error(std::string("RayTracingPipeline: vkGetDeviceProcAddr failed for ") + name);
        }
        return p;
    };
    m_pfnCreatePipelines =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(load("vkCreateRayTracingPipelinesKHR"));
    m_pfnGetGroupHandles =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(load("vkGetRayTracingShaderGroupHandlesKHR"));
    // `bufferDeviceAddress` was promoted to core in Vulkan 1.2, so on a 1.3
    // instance we load the core function (no KHR suffix). The typedef is
    // compatible since the signature is identical.
    m_pfnGetBufferAddress =
        reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(load("vkGetBufferDeviceAddress"));
}

void RayTracingPipeline::Reset() {
    VkDevice dev = m_device.GetDevice();
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(dev, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(dev, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
    DestroyRawBuffer(m_sbt);
    m_raygenRegion = {};
    m_missRegion = {};
    m_hitRegion = {};
    m_callableRegion = {};
}

bool RayTracingPipeline::Build(const RayTracingPipelineSettings& settings) {
    if (!m_device.HasRayTracing()) {
        return false;
    }
    if (settings.raygenPath.empty() || settings.missPath.empty() ||
        settings.closestHitPath.empty()) {
        return false;
    }

    // Tear down any previous pipeline/SBT first — rebuild from scratch.
    Reset();

    VkDevice dev = m_device.GetDevice();

    // --- Shader modules -----------------------------------------------------
    // Stage indices below pack as: [raygen, miss, closesthit, (anyhit?)]
    // so the group descriptors can reference them by index. On failure the
    // already-created modules are destroyed before returning false.
    std::vector<VkShaderModule> modules;
    auto cleanupModules = [&]() {
        for (auto m : modules) {
            if (m != VK_NULL_HANDLE) {
                vkDestroyShaderModule(dev, m, nullptr);
            }
        }
    };

    std::vector<uint32_t> rgSpirv;
    std::vector<uint32_t> msSpirv;
    std::vector<uint32_t> chSpirv;
    std::vector<uint32_t> ahSpirv;
    try {
        rgSpirv = ReadSpirv(settings.raygenPath);
        msSpirv = ReadSpirv(settings.missPath);
        chSpirv = ReadSpirv(settings.closestHitPath);
        if (!settings.anyHitPath.empty()) {
            ahSpirv = ReadSpirv(settings.anyHitPath);
        }
    } catch (const std::runtime_error&) {
        return false;
    }

    VkShaderModule rgMod = CreateModule(rgSpirv);
    modules.push_back(rgMod);
    VkShaderModule msMod = CreateModule(msSpirv);
    modules.push_back(msMod);
    VkShaderModule chMod = CreateModule(chSpirv);
    modules.push_back(chMod);
    VkShaderModule ahMod = VK_NULL_HANDLE;
    if (!ahSpirv.empty()) {
        ahMod = CreateModule(ahSpirv);
        modules.push_back(ahMod);
    }

    // --- Shader stages ------------------------------------------------------
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    auto pushStage = [&](VkShaderStageFlagBits stage, VkShaderModule mod) {
        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = stage;
        info.module = mod;
        info.pName = "main";
        stages.push_back(info);
    };
    const uint32_t rgIdx = static_cast<uint32_t>(stages.size());
    pushStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, rgMod);
    const uint32_t msIdx = static_cast<uint32_t>(stages.size());
    pushStage(VK_SHADER_STAGE_MISS_BIT_KHR, msMod);
    const uint32_t chIdx = static_cast<uint32_t>(stages.size());
    pushStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chMod);
    const uint32_t ahIdx = ahMod != VK_NULL_HANDLE
                               ? static_cast<uint32_t>(stages.size())
                               : kShaderUnused;
    if (ahMod != VK_NULL_HANDLE) {
        pushStage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, ahMod);
    }

    // --- Shader groups ------------------------------------------------------
    // Group 0 = raygen general, group 1 = miss general, group 2 = triangles
    // hit group (closest-hit + optional any-hit).
    std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};
    for (auto& g : groups) {
        g.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        g.generalShader = kShaderUnused;
        g.closestHitShader = kShaderUnused;
        g.anyHitShader = kShaderUnused;
        g.intersectionShader = kShaderUnused;
    }
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = rgIdx;
    groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[1].generalShader = msIdx;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].closestHitShader = chIdx;
    groups[2].anyHitShader = ahIdx;

    // --- Pipeline layout ----------------------------------------------------
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (settings.descriptorSetLayout != VK_NULL_HANDLE) {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &settings.descriptorSetLayout;
    }
    VkPushConstantRange pcRange{};
    if (settings.pushConstantSize > 0) {
        pcRange.stageFlags = settings.pushConstantStages;
        pcRange.offset = 0;
        pcRange.size = settings.pushConstantSize;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pcRange;
    }
    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        cleanupModules();
        m_layout = VK_NULL_HANDLE;
        return false;
    }

    // --- Pipeline -----------------------------------------------------------
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = settings.maxRayRecursionDepth;
    pipelineInfo.layout = m_layout;

    VkResult res = m_pfnCreatePipelines(dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
                                        &pipelineInfo, nullptr, &m_pipeline);
    cleanupModules();  // modules can be destroyed once the pipeline is created
    if (res != VK_SUCCESS) {
        vkDestroyPipelineLayout(dev, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
        m_pipeline = VK_NULL_HANDLE;
        return false;
    }

    // --- Shader Binding Table -----------------------------------------------
    // Layout: one region per group kind (raygen / miss / hit), each region
    // starts on `shaderGroupBaseAlignment` and contains a single
    // handle-sized record. Simpler than a tightly-packed SBT and enough for
    // the 9.x passes we need.
    const auto rtProps = QueryRtProps(m_device);
    const uint32_t handleSize = rtProps.shaderGroupHandleSize;
    const uint32_t handleAlign = rtProps.shaderGroupHandleAlignment;
    const uint32_t baseAlign = rtProps.shaderGroupBaseAlignment;
    const uint32_t alignedHandleSize = AlignUp(handleSize, handleAlign);
    const uint32_t regionStride = AlignUp(alignedHandleSize, baseAlign);

    const uint32_t groupCount = static_cast<uint32_t>(groups.size());
    std::vector<uint8_t> handleBlob(static_cast<size_t>(groupCount) * handleSize);
    if (m_pfnGetGroupHandles(dev, m_pipeline, 0, groupCount,
                             handleBlob.size(), handleBlob.data()) != VK_SUCCESS) {
        vkDestroyPipeline(dev, m_pipeline, nullptr);
        vkDestroyPipelineLayout(dev, m_layout, nullptr);
        m_pipeline = VK_NULL_HANDLE;
        m_layout = VK_NULL_HANDLE;
        return false;
    }

    // Stage each group handle into its aligned slot in a host-visible buffer.
    const uint32_t rgOffset = 0;
    const uint32_t msOffset = regionStride;
    const uint32_t htOffset = regionStride * 2U;
    const VkDeviceSize sbtSize = static_cast<VkDeviceSize>(regionStride) * 3U;

    std::vector<uint8_t> sbtStaging(static_cast<size_t>(sbtSize), 0);
    std::memcpy(sbtStaging.data() + rgOffset,
                handleBlob.data() + (static_cast<size_t>(0) * handleSize), handleSize);
    std::memcpy(sbtStaging.data() + msOffset,
                handleBlob.data() + (static_cast<size_t>(1) * handleSize), handleSize);
    std::memcpy(sbtStaging.data() + htOffset,
                handleBlob.data() + (static_cast<size_t>(2) * handleSize), handleSize);

    m_sbt = CreateRawBuffer(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtStaging.data());

    m_raygenRegion.deviceAddress = m_sbt.address + rgOffset;
    m_raygenRegion.stride = regionStride;  // spec: raygen size == stride
    m_raygenRegion.size = regionStride;

    m_missRegion.deviceAddress = m_sbt.address + msOffset;
    m_missRegion.stride = alignedHandleSize;
    m_missRegion.size = alignedHandleSize;

    m_hitRegion.deviceAddress = m_sbt.address + htOffset;
    m_hitRegion.stride = alignedHandleSize;
    m_hitRegion.size = alignedHandleSize;

    m_callableRegion = {};  // unused for stage 9

    return true;
}

std::vector<uint32_t> RayTracingPipeline::ReadSpirv(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("RayTracingPipeline: failed to open SPIR-V: " + path);
    }
    auto fileSize = file.tellg();
    if (fileSize <= 0 || fileSize % sizeof(uint32_t) != 0) {
        throw std::runtime_error("RayTracingPipeline: invalid SPIR-V size: " + path);
    }
    std::vector<uint32_t> spirv(static_cast<size_t>(fileSize) / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv.data()), fileSize);
    return spirv;
}

VkShaderModule RayTracingPipeline::CreateModule(const std::vector<uint32_t>& spirv) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size() * sizeof(uint32_t);
    info.pCode = spirv.data();
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device.GetDevice(), &info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("RayTracingPipeline: vkCreateShaderModule failed");
    }
    return module;
}

RayTracingPipeline::RawBuffer RayTracingPipeline::CreateRawBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memProps, const void* data) {
    VkDevice dev = m_device.GetDevice();
    RawBuffer out{};

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bufInfo, nullptr, &out.buffer) != VK_SUCCESS) {
        throw std::runtime_error("RayTracingPipeline: vkCreateBuffer failed");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(dev, out.buffer, &memReq);

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, memProps);
    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
        allocInfo.pNext = &flagsInfo;
    }

    if (vkAllocateMemory(dev, &allocInfo, nullptr, &out.memory) != VK_SUCCESS) {
        vkDestroyBuffer(dev, out.buffer, nullptr);
        out.buffer = VK_NULL_HANDLE;
        throw std::runtime_error("RayTracingPipeline: vkAllocateMemory failed");
    }
    vkBindBufferMemory(dev, out.buffer, out.memory, 0);

    if (data != nullptr && (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
        void* mapped = nullptr;
        vkMapMemory(dev, out.memory, 0, size, 0, &mapped);
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(dev, out.memory);
    }

    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
        VkBufferDeviceAddressInfo addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = out.buffer;
        out.address = m_pfnGetBufferAddress(dev, &addrInfo);
    }

    return out;
}

void RayTracingPipeline::DestroyRawBuffer(RawBuffer& buf) {
    VkDevice dev = m_device.GetDevice();
    if (buf.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, buf.buffer, nullptr);
        buf.buffer = VK_NULL_HANDLE;
    }
    if (buf.memory != VK_NULL_HANDLE) {
        vkFreeMemory(dev, buf.memory, nullptr);
        buf.memory = VK_NULL_HANDLE;
    }
    buf.address = 0;
}

uint32_t RayTracingPipeline::FindMemoryType(uint32_t typeFilter,
                                            VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_device.GetPhysicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("RayTracingPipeline: no suitable memory type");
}

}  // namespace bimeup::renderer
