#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <cstddef>

namespace bimeup::renderer {

class Device;

enum class BufferType {
    Vertex,
    Index,
    Uniform
};

class Buffer {
public:
    Buffer(const Device& device, BufferType type, VkDeviceSize size, const void* data);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator=(Buffer&&) = delete;

    [[nodiscard]] VkBuffer GetBuffer() const;
    [[nodiscard]] VkDeviceSize GetSize() const;
    [[nodiscard]] BufferType GetType() const;

    void* Map();
    void Unmap();

private:
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    BufferType m_type;
};

}  // namespace bimeup::renderer
