#include <renderer/Device.h>
#include <renderer/TopLevelAS.h>
#include <tools/Log.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace bimeup::renderer {

namespace {

// glm is column-major; Vulkan's `VkTransformMatrixKHR` is a 3×4 row-major
// float matrix (rotation+translation, no projection row). Transpose and
// trim the last row on its way into the instance buffer.
VkTransformMatrixKHR ToVkTransform(const glm::mat4& m) {
    VkTransformMatrixKHR out{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            out.matrix[row][col] = m[col][row];
        }
    }
    return out;
}

}  // namespace

TopLevelAS::TopLevelAS(const Device& device) : m_device(device) {
    if (!m_device.HasRayTracing()) {
        return;
    }
    LoadDispatch();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_device.GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device.GetDevice(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("TopLevelAS: failed to create command pool");
    }
}

TopLevelAS::~TopLevelAS() {
    Reset();
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device.GetDevice(), m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

void TopLevelAS::LoadDispatch() {
    VkDevice dev = m_device.GetDevice();
    auto load = [dev](const char* name) -> PFN_vkVoidFunction {
        PFN_vkVoidFunction p = vkGetDeviceProcAddr(dev, name);
        if (p == nullptr) {
            throw std::runtime_error(std::string("TopLevelAS: vkGetDeviceProcAddr failed for ") + name);
        }
        return p;
    };
    m_pfnGetBufferAddress =
        reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(load("vkGetBufferDeviceAddressKHR"));
    m_pfnGetBuildSizes =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(load("vkGetAccelerationStructureBuildSizesKHR"));
    m_pfnCreateAS =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(load("vkCreateAccelerationStructureKHR"));
    m_pfnDestroyAS =
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(load("vkDestroyAccelerationStructureKHR"));
    m_pfnCmdBuild =
        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(load("vkCmdBuildAccelerationStructuresKHR"));
    m_pfnGetASAddress =
        reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(load("vkGetAccelerationStructureDeviceAddressKHR"));
}

void TopLevelAS::Reset() {
    if (m_handle != VK_NULL_HANDLE && m_pfnDestroyAS != nullptr) {
        m_pfnDestroyAS(m_device.GetDevice(), m_handle, nullptr);
    }
    m_handle = VK_NULL_HANDLE;
    m_address = 0;
    DestroyRawBuffer(m_storage);
    m_instanceCount = 0;
}

bool TopLevelAS::Build(const std::vector<TlasInstance>& instances) {
    if (!m_device.HasRayTracing()) {
        return false;
    }
    // Rebuild semantics: tear down any previous TLAS first — even when the
    // new list is empty (the scene was cleared), leaving `IsValid()==false`.
    Reset();
    if (instances.empty()) {
        return false;
    }

    VkDevice dev = m_device.GetDevice();

    // Pack CPU-side instance array. `VkAccelerationStructureInstanceKHR` is
    // 64 bytes: a 3×4 row-major transform, packed customIndex+mask, packed
    // SBT-offset+flags, then the referenced BLAS's device address.
    std::vector<VkAccelerationStructureInstanceKHR> gpuInstances;
    gpuInstances.reserve(instances.size());
    for (const auto& src : instances) {
        VkAccelerationStructureInstanceKHR inst{};
        inst.transform = ToVkTransform(src.transform);
        inst.instanceCustomIndex = src.customIndex & 0x00FFFFFFu;
        inst.mask = src.mask;
        inst.instanceShaderBindingTableRecordOffset = 0;
        // Stage 9.6.a — cull-disable stays on unconditionally (BIM meshes
        // don't carry a consistent winding); OR in caller-supplied flags so
        // a glass instance can request `FORCE_NO_OPAQUE_BIT` to opt into
        // any-hit-based sun-light transmission.
        inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR | src.flags;
        inst.accelerationStructureReference = src.blasAddress;
        gpuInstances.push_back(inst);
    }

    const VkDeviceSize instBufSize =
        gpuInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    RawBuffer instBuf = CreateRawBuffer(
        instBufSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        gpuInstances.data());

    VkAccelerationStructureGeometryInstancesDataKHR instData{};
    instData.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instData.arrayOfPointers = VK_FALSE;
    instData.data.deviceAddress = instBuf.address;

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances = instData;

    const uint32_t primitiveCount = static_cast<uint32_t>(gpuInstances.size());

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    m_pfnGetBuildSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                       &buildInfo, &primitiveCount, &sizes);

    m_storage = CreateRawBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = m_storage.buffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    if (m_pfnCreateAS(dev, &createInfo, nullptr, &m_handle) != VK_SUCCESS) {
        DestroyRawBuffer(instBuf);
        DestroyRawBuffer(m_storage);
        m_handle = VK_NULL_HANDLE;
        throw std::runtime_error("TopLevelAS: vkCreateAccelerationStructureKHR failed");
    }

    RawBuffer scratch = CreateRawBuffer(
        sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, nullptr);

    buildInfo.dstAccelerationStructure = m_handle;
    buildInfo.scratchData.deviceAddress = scratch.address;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primitiveCount;
    range.primitiveOffset = 0;
    range.firstVertex = 0;
    range.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = m_pool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(dev, &cmdAlloc, &cmd) != VK_SUCCESS) {
        m_pfnDestroyAS(dev, m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
        DestroyRawBuffer(instBuf);
        DestroyRawBuffer(scratch);
        DestroyRawBuffer(m_storage);
        throw std::runtime_error("TopLevelAS: vkAllocateCommandBuffers failed");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    m_pfnCmdBuild(cmd, 1, &buildInfo, &pRange);
    vkEndCommandBuffer(cmd);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(dev, &fenceInfo, nullptr, &fence);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submit, fence);
    vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(dev, fence, nullptr);
    vkFreeCommandBuffers(dev, m_pool, 1, &cmd);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = m_handle;
    m_address = m_pfnGetASAddress(dev, &addrInfo);

    DestroyRawBuffer(instBuf);
    DestroyRawBuffer(scratch);

    m_instanceCount = instances.size();
    return true;
}

TopLevelAS::RawBuffer TopLevelAS::CreateRawBuffer(
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
        throw std::runtime_error("TopLevelAS: vkCreateBuffer failed");
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
        throw std::runtime_error("TopLevelAS: vkAllocateMemory failed");
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

void TopLevelAS::DestroyRawBuffer(RawBuffer& buf) {
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

uint32_t TopLevelAS::FindMemoryType(uint32_t typeFilter,
                                    VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_device.GetPhysicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("TopLevelAS: no suitable memory type");
}

}  // namespace bimeup::renderer
