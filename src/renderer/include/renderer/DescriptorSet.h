#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace bimeup::renderer {

class Buffer;
class Device;

struct LayoutBinding {
    uint32_t binding;
    VkDescriptorType type;
    VkShaderStageFlags stageFlags;
};

struct PoolSize {
    VkDescriptorType type;
    uint32_t count;
};

class DescriptorSetLayout {
public:
    DescriptorSetLayout(const Device& device, const std::vector<LayoutBinding>& bindings);
    ~DescriptorSetLayout();

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout(DescriptorSetLayout&&) = delete;
    DescriptorSetLayout& operator=(DescriptorSetLayout&&) = delete;

    [[nodiscard]] VkDescriptorSetLayout GetLayout() const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
};

class DescriptorPool {
public:
    DescriptorPool(const Device& device, uint32_t maxSets, const std::vector<PoolSize>& poolSizes);
    ~DescriptorPool();

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(DescriptorPool&&) = delete;
    DescriptorPool& operator=(DescriptorPool&&) = delete;

    [[nodiscard]] VkDescriptorPool GetPool() const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
};

class DescriptorSet {
public:
    DescriptorSet(const Device& device, const DescriptorPool& pool,
                  const DescriptorSetLayout& layout);

    // Descriptor sets are freed when the pool is destroyed — no explicit cleanup needed
    ~DescriptorSet() = default;

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;
    DescriptorSet(DescriptorSet&&) = delete;
    DescriptorSet& operator=(DescriptorSet&&) = delete;

    [[nodiscard]] VkDescriptorSet GetSet() const;

    void UpdateBuffer(uint32_t binding, const Buffer& buffer);
    void Bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t setIndex = 0) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
