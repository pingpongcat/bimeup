#include <renderer/TonemapPipeline.h>

#include <renderer/Pipeline.h>

namespace bimeup::renderer {

TonemapPipeline::TonemapPipeline(const Device& device,
                                 const Shader& vertexShader,
                                 const Shader& fragmentShader,
                                 VkRenderPass renderPass,
                                 VkDescriptorSetLayout hdrSamplerLayout,
                                 VkSampleCountFlagBits samples) {
    PipelineConfig config{};
    config.renderPass = renderPass;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.depthTestEnable = false;
    config.depthWriteEnable = false;
    config.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    config.alphaBlendEnable = false;
    config.rasterizationSamples = samples != 0 ? samples : VK_SAMPLE_COUNT_1_BIT;
    config.colorAttachmentCount = 1;
    config.descriptorSetLayouts = {hdrSamplerLayout};

    VkPushConstantRange fogRange{};
    fogRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    fogRange.offset = 0;
    fogRange.size = sizeof(PushConstants);
    config.pushConstantRanges = {fogRange};

    m_pipeline = std::make_unique<Pipeline>(device, vertexShader, fragmentShader, config);
}

TonemapPipeline::~TonemapPipeline() = default;

void TonemapPipeline::Bind(VkCommandBuffer cmd) const {
    m_pipeline->Bind(cmd);
}

VkPipeline TonemapPipeline::GetPipeline() const {
    return m_pipeline->GetPipeline();
}

VkPipelineLayout TonemapPipeline::GetLayout() const {
    return m_pipeline->GetLayout();
}

}  // namespace bimeup::renderer
