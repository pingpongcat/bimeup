#include <renderer/EdgeOverlayPipeline.h>

#include <renderer/MeshBuffer.h>
#include <renderer/Pipeline.h>

#include <cstddef>
#include <cstdint>

namespace bimeup::renderer {

EdgeOverlayPipeline::EdgeOverlayPipeline(const Device& device,
                                         const Shader& vertexShader,
                                         const Shader& fragmentShader,
                                         VkRenderPass renderPass,
                                         VkDescriptorSetLayout cameraLayout,
                                         uint32_t colorAttachmentCount,
                                         bool disableSecondaryColorWrites) {
    // Vertex buffer is shared with the basic/opaque pipeline — same stride, same
    // layout — but only the position attribute is consumed here.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription posAttr{};
    posAttr.binding = 0;
    posAttr.location = 0;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttr.offset = offsetof(Vertex, position);

    VkPushConstantRange modelRange{};
    modelRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    modelRange.offset = 0;
    modelRange.size = 64;  // mat4

    VkPushConstantRange colorRange{};
    colorRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    colorRange.offset = 64;
    colorRange.size = 16;  // vec4

    PipelineConfig config{};
    config.renderPass = renderPass;
    config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.depthTestEnable = true;
    config.depthWriteEnable = false;
    config.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    config.depthBiasEnable = true;
    config.depthBiasConstantFactor = -1.0F;
    config.depthBiasSlopeFactor = -1.0F;
    config.alphaBlendEnable = true;
    config.colorAttachmentCount = colorAttachmentCount;
    config.disableSecondaryColorWrites = disableSecondaryColorWrites;
    config.vertexBindings = {binding};
    config.vertexAttributes = {posAttr};
    config.descriptorSetLayouts = {cameraLayout};
    config.pushConstantRanges = {modelRange, colorRange};

    m_pipeline = std::make_unique<Pipeline>(device, vertexShader, fragmentShader, config);
}

EdgeOverlayPipeline::~EdgeOverlayPipeline() = default;

void EdgeOverlayPipeline::Bind(VkCommandBuffer cmd) const {
    m_pipeline->Bind(cmd);
}

VkPipeline EdgeOverlayPipeline::GetPipeline() const {
    return m_pipeline->GetPipeline();
}

VkPipelineLayout EdgeOverlayPipeline::GetLayout() const {
    return m_pipeline->GetLayout();
}

}  // namespace bimeup::renderer
