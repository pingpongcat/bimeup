#include <renderer/AccelerationStructure.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <tools/Log.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace bimeup::renderer {

AccelerationStructure::AccelerationStructure(const Device& device) : m_device(device) {
    if (!m_device.HasRayTracing()) {
        return;
    }
    LoadDispatch();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_device.GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device.GetDevice(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS) {
        throw std::runtime_error("AccelerationStructure: failed to create command pool");
    }
}

AccelerationStructure::~AccelerationStructure() {
    VkDevice dev = m_device.GetDevice();
    for (auto& [handle, blas] : m_blasMap) {
        if (blas.handle != VK_NULL_HANDLE && m_pfnDestroyAS != nullptr) {
            m_pfnDestroyAS(dev, blas.handle, nullptr);
        }
        DestroyRawBuffer(blas.storage);
    }
    m_blasMap.clear();
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(dev, m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

void AccelerationStructure::LoadDispatch() {
    VkDevice dev = m_device.GetDevice();
    auto load = [dev](const char* name) -> PFN_vkVoidFunction {
        PFN_vkVoidFunction p = vkGetDeviceProcAddr(dev, name);
        if (p == nullptr) {
            throw std::runtime_error(std::string("AccelerationStructure: vkGetDeviceProcAddr failed for ") + name);
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

AccelerationStructure::BlasHandle AccelerationStructure::BuildBlas(const MeshData& data) {
    if (!m_device.HasRayTracing()) {
        return InvalidHandle;
    }
    if (data.vertices.empty() || data.indices.empty() || (data.indices.size() % 3) != 0) {
        return InvalidHandle;
    }

    VkDevice dev = m_device.GetDevice();

    // Upload vertex + index data to host-visible buffers; BLAS build reads them
    // via device address. We point at `position` (first `vec3` of `Vertex`) and
    // set stride = `sizeof(Vertex)` — the triangle geometry ignores the rest.
    const VkDeviceSize vbSize = data.vertices.size() * sizeof(Vertex);
    const VkDeviceSize ibSize = data.indices.size() * sizeof(uint32_t);

    RawBuffer vbuf = CreateRawBuffer(
        vbSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.vertices.data());

    RawBuffer ibuf = CreateRawBuffer(
        ibSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        data.indices.data());

    VkAccelerationStructureGeometryTrianglesDataKHR tri{};
    tri.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    tri.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    tri.vertexData.deviceAddress = vbuf.address;
    tri.vertexStride = sizeof(Vertex);
    tri.maxVertex = static_cast<uint32_t>(data.vertices.size() - 1);
    tri.indexType = VK_INDEX_TYPE_UINT32;
    tri.indexData.deviceAddress = ibuf.address;
    tri.transformData.deviceAddress = 0;

    VkAccelerationStructureGeometryKHR geom{};
    geom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.geometry.triangles = tri;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    const uint32_t primitiveCount = static_cast<uint32_t>(data.indices.size() / 3);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes{};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    m_pfnGetBuildSizes(dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                       &buildInfo, &primitiveCount, &sizes);

    RawBuffer storage = CreateRawBuffer(
        sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        nullptr);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = storage.buffer;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkAccelerationStructureKHR asHandle = VK_NULL_HANDLE;
    if (m_pfnCreateAS(dev, &createInfo, nullptr, &asHandle) != VK_SUCCESS) {
        DestroyRawBuffer(vbuf);
        DestroyRawBuffer(ibuf);
        DestroyRawBuffer(storage);
        throw std::runtime_error("AccelerationStructure: vkCreateAccelerationStructureKHR failed");
    }

    RawBuffer scratch = CreateRawBuffer(
        sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        nullptr);

    buildInfo.dstAccelerationStructure = asHandle;
    buildInfo.scratchData.deviceAddress = scratch.address;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primitiveCount;
    range.primitiveOffset = 0;
    range.firstVertex = 0;
    range.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    // Immediate submit — BLAS builds are one-shot at scene-upload time so a
    // synchronous fence wait is fine; TLAS rebuild will move to async in 9.2.
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = m_pool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(dev, &cmdAlloc, &cmd) != VK_SUCCESS) {
        m_pfnDestroyAS(dev, asHandle, nullptr);
        DestroyRawBuffer(vbuf);
        DestroyRawBuffer(ibuf);
        DestroyRawBuffer(scratch);
        DestroyRawBuffer(storage);
        throw std::runtime_error("AccelerationStructure: vkAllocateCommandBuffers failed");
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
    addrInfo.accelerationStructure = asHandle;
    const VkDeviceAddress blasAddress = m_pfnGetASAddress(dev, &addrInfo);

    DestroyRawBuffer(vbuf);
    DestroyRawBuffer(ibuf);
    DestroyRawBuffer(scratch);

    const BlasHandle h = m_nextHandle++;
    m_blasMap[h] = Blas{asHandle, blasAddress, storage};
    return h;
}

bool AccelerationStructure::IsValid(BlasHandle handle) const {
    return m_blasMap.find(handle) != m_blasMap.end();
}

VkAccelerationStructureKHR AccelerationStructure::GetHandle(BlasHandle handle) const {
    auto it = m_blasMap.find(handle);
    return it == m_blasMap.end() ? VK_NULL_HANDLE : it->second.handle;
}

VkDeviceAddress AccelerationStructure::GetDeviceAddress(BlasHandle handle) const {
    auto it = m_blasMap.find(handle);
    return it == m_blasMap.end() ? 0 : it->second.address;
}

std::size_t AccelerationStructure::BlasCount() const {
    return m_blasMap.size();
}

AccelerationStructure::RawBuffer AccelerationStructure::CreateRawBuffer(
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memProps, const void* data) {
    // Raw `vkCreateBuffer` + `vkAllocateMemory` — VMA on this codebase is
    // configured without `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`
    // (api version pinned to 1.0) and bumping it is out of scope for 9.1.b.
    // The AS storage + scratch + staging buffers all need BDA-compatible
    // allocations, so we go through the Vulkan API directly here.
    VkDevice dev = m_device.GetDevice();
    RawBuffer out{};

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bufInfo, nullptr, &out.buffer) != VK_SUCCESS) {
        throw std::runtime_error("AccelerationStructure: vkCreateBuffer failed");
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
        throw std::runtime_error("AccelerationStructure: vkAllocateMemory failed");
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

void AccelerationStructure::DestroyRawBuffer(RawBuffer& buf) {
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

uint32_t AccelerationStructure::FindMemoryType(uint32_t typeFilter,
                                               VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_device.GetPhysicalDevice(), &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0 &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("AccelerationStructure: no suitable memory type");
}

}  // namespace bimeup::renderer
