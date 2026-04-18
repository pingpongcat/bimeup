#include <renderer/DiskMarkerPipeline.h>

#include <renderer/Pipeline.h>

#include <cstddef>

namespace bimeup::renderer {

namespace {

struct DiskVertexLayout {
    float position[3];
    float color[4];
};

}  // namespace

DiskMarkerPipeline::DiskMarkerPipeline(const Device& device,
                                       const Shader& vertexShader,
                                       const Shader& fragmentShader,
                                       VkRenderPass renderPass,
                                       VkDescriptorSetLayout cameraLayout,
                                       VkSampleCountFlagBits samples) {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(DiskVertexLayout);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription posAttr{};
    posAttr.binding = 0;
    posAttr.location = 0;
    posAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
    posAttr.offset = offsetof(DiskVertexLayout, position);

    VkVertexInputAttributeDescription colorAttr{};
    colorAttr.binding = 0;
    colorAttr.location = 1;
    colorAttr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    colorAttr.offset = offsetof(DiskVertexLayout, color);

    PipelineConfig config{};
    config.renderPass = renderPass;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // Draw on top of everything so the PoV hover marker stays visible even
    // when the cursor floats through other geometry (walls, transparent
    // panes, ghosted non-slab elements). Depth write stays off so the disk
    // doesn't shadow-occlude later passes.
    config.depthTestEnable = true;
    config.depthWriteEnable = false;
    config.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    config.alphaBlendEnable = true;
    config.rasterizationSamples = samples != 0 ? samples : VK_SAMPLE_COUNT_1_BIT;
    config.colorAttachmentCount = 1;
    config.vertexBindings = {binding};
    config.vertexAttributes = {posAttr, colorAttr};
    config.descriptorSetLayouts = {cameraLayout};

    m_pipeline = std::make_unique<Pipeline>(device, vertexShader, fragmentShader, config);
}

DiskMarkerPipeline::~DiskMarkerPipeline() = default;

void DiskMarkerPipeline::Bind(VkCommandBuffer cmd) const {
    m_pipeline->Bind(cmd);
}

VkPipeline DiskMarkerPipeline::GetPipeline() const {
    return m_pipeline->GetPipeline();
}

VkPipelineLayout DiskMarkerPipeline::GetLayout() const {
    return m_pipeline->GetLayout();
}

}  // namespace bimeup::renderer
