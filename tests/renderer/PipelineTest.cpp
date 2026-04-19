#include <gtest/gtest.h>
#include <renderer/Pipeline.h>
#include <renderer/Shader.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Device;
using bimeup::renderer::Pipeline;
using bimeup::renderer::PipelineConfig;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

// Minimal valid SPIR-V vertex shader (void main entry point)
std::vector<uint32_t> MakeMinimalVertexSpirv() {
    return {
        0x07230203, 0x00010000, 0x00000000, 0x00000005, 0x00000000,
        0x00020011, 0x00000001,
        0x0003000E, 0x00000000, 0x00000001,
        0x0005000F, 0x00000000, 0x00000001, 0x6E69616D, 0x00000000,
        0x00020013, 0x00000002,
        0x00030021, 0x00000003, 0x00000002,
        0x00050036, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        0x000200F8, 0x00000004,
        0x000100FD,
        0x00010038,
    };
}

// Minimal valid SPIR-V fragment shader (void main, OriginUpperLeft)
std::vector<uint32_t> MakeMinimalFragmentSpirv() {
    return {
        0x07230203, 0x00010000, 0x00000000, 0x00000005, 0x00000000,
        0x00020011, 0x00000001,
        0x0003000E, 0x00000000, 0x00000001,
        0x0005000F, 0x00000004, 0x00000001, 0x6E69616D, 0x00000000,
        0x00030010, 0x00000001, 0x00000007,
        0x00020013, 0x00000002,
        0x00030021, 0x00000003, 0x00000002,
        0x00050036, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        0x000200F8, 0x00000004,
        0x000100FD,
        0x00010038,
    };
}

VkRenderPass CreateMrtRenderPass(VkDevice device, VkFormat colorFormat, VkFormat normalFormat) {
    std::array<VkAttachmentDescription, 2> attachments{};
    for (size_t i = 0; i < attachments.size(); ++i) {
        attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    attachments[0].format = colorFormat;
    attachments[1].format = normalFormat;

    std::array<VkAttachmentReference, 2> colorRefs{};
    colorRefs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult result = vkCreateRenderPass(device, &info, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create MRT test render pass");
    }
    return renderPass;
}

VkRenderPass CreateSimpleRenderPass(VkDevice device, VkFormat format) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create test render pass");
    }
    return renderPass;
}

}  // namespace

class PipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateSimpleRenderPass(m_device->GetDevice(), VK_FORMAT_B8G8R8A8_SRGB);
    }

    void TearDown() override {
        m_pipeline.reset();
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->GetDevice(), m_renderPass, nullptr);
        }
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::unique_ptr<Pipeline> m_pipeline;
};

TEST_F(PipelineTest, CreateWithVertexAndFragmentShader) {
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = m_renderPass;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(PipelineTest, CreateWithVertexInput) {
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 3;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr{};
    attr.binding = 0;
    attr.location = 0;
    attr.format = VK_FORMAT_R32G32B32_SFLOAT;
    attr.offset = 0;

    PipelineConfig config{};
    config.renderPass = m_renderPass;
    config.vertexBindings = {binding};
    config.vertexAttributes = {attr};

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(PipelineTest, CreateWithWireframeMode) {
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = m_renderPass;
    config.polygonMode = VK_POLYGON_MODE_LINE;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(PipelineTest, NullRenderPassThrows) {
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = VK_NULL_HANDLE;

    EXPECT_THROW(
        Pipeline(*m_device, vertShader, fragShader, config),
        std::runtime_error);
}

TEST_F(PipelineTest, CreateWithAlphaBlendEnabled) {
    // Transparent pass: alpha blending on, depth test on, depth write off.
    // Validation layers would flag any misconfigured blend state.
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = m_renderPass;
    config.alphaBlendEnable = true;
    config.depthTestEnable = true;
    config.depthWriteEnable = false;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(PipelineTest, CreateWithTwoColorAttachmentsAllWriting) {
    // MRT pass (colour + normal G-buffer). Pipeline must build a blend-state array
    // sized to colorAttachmentCount — a single VkPipelineColorBlendAttachmentState
    // pointed at by pAttachments is a spec violation when attachmentCount > 1.
    VkRenderPass mrt = CreateMrtRenderPass(m_device->GetDevice(),
                                           VK_FORMAT_R16G16B16A16_SFLOAT,
                                           VK_FORMAT_R16G16_SNORM);

    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = mrt;
    config.colorAttachmentCount = 2;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);

    m_pipeline.reset();
    vkDestroyRenderPass(m_device->GetDevice(), mrt, nullptr);
}

TEST_F(PipelineTest, CreateWithTwoColorAttachmentsSecondWriteMasked) {
    // Overlay-style MRT pipeline: writes only attachment 0 (colour), leaves
    // attachment 1 (normal) untouched via zero writemask.
    VkRenderPass mrt = CreateMrtRenderPass(m_device->GetDevice(),
                                           VK_FORMAT_R16G16B16A16_SFLOAT,
                                           VK_FORMAT_R16G16_SNORM);

    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = mrt;
    config.colorAttachmentCount = 2;
    config.disableSecondaryColorWrites = true;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);

    m_pipeline.reset();
    vkDestroyRenderPass(m_device->GetDevice(), mrt, nullptr);
}

TEST_F(PipelineTest, CreateMrtWithAlphaBlendOnPrimaryOnly) {
    // Transparent-style MRT pipeline: alpha-over on attachment 0, attachment 1
    // write-masked so blend state on the secondary attachment is inert.
    VkRenderPass mrt = CreateMrtRenderPass(m_device->GetDevice(),
                                           VK_FORMAT_R16G16B16A16_SFLOAT,
                                           VK_FORMAT_R16G16_SNORM);

    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = mrt;
    config.colorAttachmentCount = 2;
    config.alphaBlendEnable = true;
    config.depthTestEnable = true;
    config.depthWriteEnable = false;
    config.disableSecondaryColorWrites = true;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);

    m_pipeline.reset();
    vkDestroyRenderPass(m_device->GetDevice(), mrt, nullptr);
}

TEST_F(PipelineTest, CreateWithAdditiveBlendEnabled) {
    // RP.10c — bloom upsample composites additively onto the higher mip. The
    // additiveBlend flag must flip the blend state to srcColor * ONE +
    // dstColor * ONE = ADD, so the pipeline builds without validation errors.
    // Mutually exclusive with alphaBlendEnable — asserting builds with the
    // two flags not both set is the unit-level expectation.
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = m_renderPass;
    config.additiveBlend = true;

    m_pipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, config);
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(PipelineTest, DestructorCleansUp) {
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig config{};
    config.renderPass = m_renderPass;

    {
        Pipeline pipeline(*m_device, vertShader, fragShader, config);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout
}
