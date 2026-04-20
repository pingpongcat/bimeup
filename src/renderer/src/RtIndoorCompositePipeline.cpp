#include <renderer/RtIndoorCompositePipeline.h>

#include <renderer/Device.h>
#include <renderer/Shader.h>

#include <stdexcept>

namespace bimeup::renderer {

RtIndoorCompositePipeline::RtIndoorCompositePipeline(const Device& device,
                                                     const Shader& computeShader,
                                                     VkDescriptorSetLayout descriptorSetLayout)
    : m_device(device.GetDevice()) {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RtIndoorCompositePipeline layout");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = computeShader.GetStageInfo();
    pipelineInfo.layout = m_layout;

    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                 &m_pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to create RtIndoorCompositePipeline");
    }
}

RtIndoorCompositePipeline::~RtIndoorCompositePipeline() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    }
}

void RtIndoorCompositePipeline::Bind(VkCommandBuffer cmd) const {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
}

}  // namespace bimeup::renderer
