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
    void UpdateImage(uint32_t binding, VkImageView imageView, VkSampler sampler,
                     VkImageLayout imageLayout);
    /// Stage 9.Q.2 — bind a top-level acceleration structure to the given
    /// slot. The descriptor type at `binding` must be
    /// `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR`. Caller must ensure
    /// `tlas != VK_NULL_HANDLE`; SceneUploader::WriteTlasToDescriptor wraps
    /// this with the null-TLAS no-op for the raster-mode path.
    void UpdateAccelerationStructure(uint32_t binding, VkAccelerationStructureKHR tlas);
    void Bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t setIndex = 0) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkDescriptorSet m_set = VK_NULL_HANDLE;
};

}  // namespace bimeup::renderer
