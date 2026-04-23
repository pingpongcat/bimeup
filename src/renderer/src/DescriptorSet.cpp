#include <renderer/DescriptorSet.h>
#include <renderer/Buffer.h>
#include <renderer/Device.h>

#include <stdexcept>

namespace bimeup::renderer {

// --- DescriptorSetLayout ---

DescriptorSetLayout::DescriptorSetLayout(const Device& device,
                                         const std::vector<LayoutBinding>& bindings)
    : m_device(device.GetDevice()) {
    std::vector<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());

    for (const auto& b : bindings) {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = b.binding;
        vkBinding.descriptorType = b.type;
        vkBinding.descriptorCount = 1;
        vkBinding.stageFlags = b.stageFlags;
        vkBinding.pImmutableSamplers = nullptr;
        vkBindings.push_back(vkBinding);
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    createInfo.pBindings = vkBindings.data();

    VkResult result = vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_layout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

DescriptorSetLayout::~DescriptorSetLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    }
}

VkDescriptorSetLayout DescriptorSetLayout::GetLayout() const {
    return m_layout;
}

// --- DescriptorPool ---

DescriptorPool::DescriptorPool(const Device& device, uint32_t maxSets,
                               const std::vector<PoolSize>& poolSizes)
    : m_device(device.GetDevice()) {
    std::vector<VkDescriptorPoolSize> vkPoolSizes;
    vkPoolSizes.reserve(poolSizes.size());

    for (const auto& ps : poolSizes) {
        VkDescriptorPoolSize vkSize{};
        vkSize.type = ps.type;
        vkSize.descriptorCount = ps.count;
        vkPoolSizes.push_back(vkSize);
    }

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.poolSizeCount = static_cast<uint32_t>(vkPoolSizes.size());
    createInfo.pPoolSizes = vkPoolSizes.data();
    createInfo.maxSets = maxSets;

    VkResult result = vkCreateDescriptorPool(m_device, &createInfo, nullptr, &m_pool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

DescriptorPool::~DescriptorPool() {
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    }
}

VkDescriptorPool DescriptorPool::GetPool() const {
    return m_pool;
}

// --- DescriptorSet ---

DescriptorSet::DescriptorSet(const Device& device, const DescriptorPool& pool,
                             const DescriptorSetLayout& layout)
    : m_device(device.GetDevice()) {
    VkDescriptorSetLayout setLayout = layout.GetLayout();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool.GetPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout;

    VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, &m_set);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }
}

VkDescriptorSet DescriptorSet::GetSet() const {
    return m_set;
}

void DescriptorSet::UpdateBuffer(uint32_t binding, const Buffer& buffer) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer.GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = buffer.GetSize();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void DescriptorSet::UpdateImage(uint32_t binding, VkImageView imageView, VkSampler sampler,
                                VkImageLayout imageLayout) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = imageLayout;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void DescriptorSet::Bind(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout,
                         uint32_t setIndex) const {
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, setIndex, 1,
                            &m_set, 0, nullptr);
}

}  // namespace bimeup::renderer
