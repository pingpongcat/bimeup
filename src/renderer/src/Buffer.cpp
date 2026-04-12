#include <renderer/Buffer.h>
#include <renderer/Device.h>

#include <cstring>
#include <stdexcept>

namespace bimeup::renderer {

namespace {

VkBufferUsageFlags GetUsageFlags(BufferType type) {
    switch (type) {
        case BufferType::Vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case BufferType::Index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case BufferType::Uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    return 0;
}

VmaMemoryUsage GetMemoryUsage(BufferType type) {
    switch (type) {
        case BufferType::Vertex:
        case BufferType::Index:
            return VMA_MEMORY_USAGE_AUTO;
        case BufferType::Uniform:
            return VMA_MEMORY_USAGE_AUTO;
    }
    return VMA_MEMORY_USAGE_AUTO;
}

VmaAllocationCreateFlags GetAllocFlags(BufferType type) {
    switch (type) {
        case BufferType::Vertex:
        case BufferType::Index:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        case BufferType::Uniform:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                   VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    return 0;
}

}  // namespace

Buffer::Buffer(const Device& device, BufferType type, VkDeviceSize size, const void* data)
    : m_allocator(device.GetAllocator()), m_size(size), m_type(type) {

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = GetUsageFlags(type);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = GetMemoryUsage(type);
    allocInfo.flags = GetAllocFlags(type);

    VkResult result = vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo,
                                       &m_buffer, &m_allocation, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    if (data != nullptr) {
        void* mapped = Map();
        std::memcpy(mapped, data, static_cast<size_t>(size));
        Unmap();
    }
}

Buffer::~Buffer() {
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
}

VkBuffer Buffer::GetBuffer() const {
    return m_buffer;
}

VkDeviceSize Buffer::GetSize() const {
    return m_size;
}

BufferType Buffer::GetType() const {
    return m_type;
}

void* Buffer::Map() {
    void* mapped = nullptr;
    VkResult result = vmaMapMemory(m_allocator, m_allocation, &mapped);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to map buffer memory");
    }
    return mapped;
}

void Buffer::Unmap() {
    vmaUnmapMemory(m_allocator, m_allocation);
}

}  // namespace bimeup::renderer
