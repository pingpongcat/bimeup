#include <gtest/gtest.h>
#include <renderer/RenderMode.h>
#include <renderer/Pipeline.h>
#include <renderer/Shader.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Device;
using bimeup::renderer::Pipeline;
using bimeup::renderer::PipelineConfig;
using bimeup::renderer::RenderMode;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;
using bimeup::renderer::GetPolygonMode;

TEST(RenderModeTest, ShadedMapsToFill) {
    EXPECT_EQ(GetPolygonMode(RenderMode::Shaded), VK_POLYGON_MODE_FILL);
}

TEST(RenderModeTest, WireframeMapsToLine) {
    EXPECT_EQ(GetPolygonMode(RenderMode::Wireframe), VK_POLYGON_MODE_LINE);
}

namespace {

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

class RenderModePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateSimpleRenderPass(m_device->GetDevice(), VK_FORMAT_B8G8R8A8_SRGB);
    }

    void TearDown() override {
        m_shadedPipeline.reset();
        m_wireframePipeline.reset();
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->GetDevice(), m_renderPass, nullptr);
        }
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::unique_ptr<Pipeline> m_shadedPipeline;
    std::unique_ptr<Pipeline> m_wireframePipeline;
};

TEST_F(RenderModePipelineTest, BothModesCreateValidPipelines) {
    Shader vertShader(*m_device, ShaderStage::Vertex, MakeMinimalVertexSpirv());
    Shader fragShader(*m_device, ShaderStage::Fragment, MakeMinimalFragmentSpirv());

    PipelineConfig shadedConfig{};
    shadedConfig.renderPass = m_renderPass;
    shadedConfig.polygonMode = bimeup::renderer::GetPolygonMode(RenderMode::Shaded);

    PipelineConfig wireframeConfig{};
    wireframeConfig.renderPass = m_renderPass;
    wireframeConfig.polygonMode = bimeup::renderer::GetPolygonMode(RenderMode::Wireframe);

    m_shadedPipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, shadedConfig);
    m_wireframePipeline = std::make_unique<Pipeline>(*m_device, vertShader, fragShader, wireframeConfig);

    EXPECT_NE(m_shadedPipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_wireframePipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_shadedPipeline->GetPipeline(), m_wireframePipeline->GetPipeline());
}
