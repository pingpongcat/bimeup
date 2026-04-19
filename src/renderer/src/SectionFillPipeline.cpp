#include <renderer/SectionFillPipeline.h>

#include <renderer/Pipeline.h>

#include <cstddef>

namespace bimeup::renderer {

namespace {

struct SectionVertexLayout {
    float position[3];
    float color[4];
};

}  // namespace

SectionFillPipeline::SectionFillPipeline(const Device& device,
                                         const Shader& vertexShader,
                                         const Shader& fragmentShader,
                                         VkRenderPass renderPass,
                                         VkDescriptorSetLayout cameraLayout,
                                         uint32_t colorAttachmentCount,
                                         bool disableSecondaryColorWrites) {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(SectionVertexLayout);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription posAttr{};
    posAttr.binding = 0;
    posAttr.location = 0;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttr.offset = offsetof(SectionVertexLayout, position);

    VkVertexInputAttributeDescription colorAttr{};
    colorAttr.binding = 0;
    colorAttr.location = 1;
    colorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorAttr.offset = offsetof(SectionVertexLayout, color);

    PipelineConfig config{};
    config.renderPass = renderPass;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.depthTestEnable = true;
    config.depthWriteEnable = false;
    config.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    config.colorAttachmentCount = colorAttachmentCount;
    config.disableSecondaryColorWrites = disableSecondaryColorWrites;
    config.vertexBindings = {binding};
    config.vertexAttributes = {posAttr, colorAttr};
    config.descriptorSetLayouts = {cameraLayout};

    m_pipeline = std::make_unique<Pipeline>(device, vertexShader, fragmentShader, config);
}

SectionFillPipeline::~SectionFillPipeline() = default;

void SectionFillPipeline::Bind(VkCommandBuffer cmd) const {
    m_pipeline->Bind(cmd);
}

VkPipeline SectionFillPipeline::GetPipeline() const {
    return m_pipeline->GetPipeline();
}

VkPipelineLayout SectionFillPipeline::GetLayout() const {
    return m_pipeline->GetLayout();
}

}  // namespace bimeup::renderer
