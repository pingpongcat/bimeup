#include <renderer/OutlinePipeline.h>

#include <renderer/Pipeline.h>

namespace bimeup::renderer {

OutlinePipeline::OutlinePipeline(const Device& device,
                                 const Shader& vertexShader,
                                 const Shader& fragmentShader,
                                 VkRenderPass renderPass,
                                 VkDescriptorSetLayout inputLayout,
                                 VkSampleCountFlagBits samples) {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    PipelineConfig config{};
    config.renderPass = renderPass;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.depthTestEnable = false;
    config.depthWriteEnable = false;
    config.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    // Alpha-blend over the already-tonemapped swapchain image. The fragment
    // shader `discard`s non-edge pixels so this mostly costs nothing.
    config.alphaBlendEnable = true;
    config.rasterizationSamples = samples != 0 ? samples : VK_SAMPLE_COUNT_1_BIT;
    config.colorAttachmentCount = 1;
    config.descriptorSetLayouts = {inputLayout};
    config.pushConstantRanges = {pushRange};

    m_pipeline = std::make_unique<Pipeline>(device, vertexShader, fragmentShader, config);
}

OutlinePipeline::~OutlinePipeline() = default;

void OutlinePipeline::Bind(VkCommandBuffer cmd) const {
    m_pipeline->Bind(cmd);
}

VkPipeline OutlinePipeline::GetPipeline() const {
    return m_pipeline->GetPipeline();
}

VkPipelineLayout OutlinePipeline::GetLayout() const {
    return m_pipeline->GetLayout();
}

}  // namespace bimeup::renderer
