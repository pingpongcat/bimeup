#include <renderer/Pipeline.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>

#include <array>
#include <stdexcept>

namespace bimeup::renderer {

Pipeline::Pipeline(const Device& device, const Shader& vertexShader,
                   const Shader& fragmentShader, const PipelineConfig& config)
    : m_device(device.GetDevice()) {
    if (config.renderPass == VK_NULL_HANDLE) {
        throw std::runtime_error("Pipeline requires a valid render pass");
    }
    CreateLayout(config);
    CreatePipeline(vertexShader, fragmentShader, config);
}

Pipeline::~Pipeline() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    }
}

VkPipeline Pipeline::GetPipeline() const {
    return m_pipeline;
}

VkPipelineLayout Pipeline::GetLayout() const {
    return m_layout;
}

void Pipeline::Bind(VkCommandBuffer cmd) const {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

void Pipeline::CreateLayout(const PipelineConfig& config) {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(config.descriptorSetLayouts.size());
    layoutInfo.pSetLayouts = config.descriptorSetLayouts.empty()
                                 ? nullptr
                                 : config.descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(config.pushConstantRanges.size());
    layoutInfo.pPushConstantRanges = config.pushConstantRanges.empty()
                                        ? nullptr
                                        : config.pushConstantRanges.data();

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }
}

void Pipeline::CreatePipeline(const Shader& vertexShader, const Shader& fragmentShader,
                               const PipelineConfig& config) {
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vertexShader.GetStageInfo(),
        fragmentShader.GetStageInfo(),
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount =
        static_cast<uint32_t>(config.vertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions =
        config.vertexBindings.empty() ? nullptr : config.vertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(config.vertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions =
        config.vertexAttributes.empty() ? nullptr : config.vertexAttributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = config.polygonMode;
    rasterizer.lineWidth = 1.0F;
    rasterizer.cullMode = config.cullMode;
    rasterizer.frontFace = config.frontFace;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = config.renderPass;
    pipelineInfo.subpass = config.subpass;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline");
    }
}

}  // namespace bimeup::renderer
