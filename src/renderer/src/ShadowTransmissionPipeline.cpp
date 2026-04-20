#include <renderer/ShadowTransmissionPipeline.h>

#include <renderer/MeshBuffer.h>
#include <renderer/Pipeline.h>

#include <cstddef>

namespace bimeup::renderer {

ShadowTransmissionPipeline::ShadowTransmissionPipeline(const Device& device,
                                                       const Shader& vertexShader,
                                                       const Shader& fragmentShader,
                                                       VkRenderPass shadowRenderPass) {
    // Shared mesh vertex buffer (pos + normal + colour, 40-B stride); only
    // position is consumed — the transmission attachment is written from the
    // push-constant tint, not from per-vertex colour.
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription posAttr{};
    posAttr.binding = 0;
    posAttr.location = 0;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttr.offset = offsetof(Vertex, position);

    VkPushConstantRange lightSpaceRange{};
    lightSpaceRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    lightSpaceRange.offset = 0;
    lightSpaceRange.size = 64;  // mat4 lightSpaceMatrix * model

    VkPushConstantRange tintRange{};
    tintRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    tintRange.offset = 64;
    tintRange.size = 16;  // vec4 glassTint

    PipelineConfig config{};
    config.renderPass = shadowRenderPass;
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    // NONE — IFC glass is commonly two-sided, and a single-sided pane would
    // write the same tint from either face under min-blend anyway.
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // Depth-test LESS against the opaque depth attachment the depth pipeline
    // already filled earlier in the same render pass; depth-write OFF so a
    // wall behind the glass still fully blocks the sun.
    config.depthTestEnable = true;
    config.depthWriteEnable = false;
    config.depthCompareOp = VK_COMPARE_OP_LESS;
    config.minBlend = true;
    config.colorAttachmentCount = 1;
    config.vertexBindings = {binding};
    config.vertexAttributes = {posAttr};
    config.pushConstantRanges = {lightSpaceRange, tintRange};

    m_pipeline = std::make_unique<Pipeline>(device, vertexShader, fragmentShader, config);
}

ShadowTransmissionPipeline::~ShadowTransmissionPipeline() = default;

void ShadowTransmissionPipeline::Bind(VkCommandBuffer cmd) const {
    m_pipeline->Bind(cmd);
}

VkPipeline ShadowTransmissionPipeline::GetPipeline() const {
    return m_pipeline->GetPipeline();
}

VkPipelineLayout ShadowTransmissionPipeline::GetLayout() const {
    return m_pipeline->GetLayout();
}

}  // namespace bimeup::renderer
