#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SmaaBlendPipeline.h>
#include <renderer/VulkanContext.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::SmaaBlendPipeline;
using bimeup::renderer::VulkanContext;

namespace {

// Blend pass writes the final AA'd LDR pixel to an RGBA8 target — same
// intermediate shape as today's FXAA output (RP.11c will drop this into
// the same slot).
VkRenderPass CreateRgba8ColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &color;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &rp) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create test render pass");
    }
    return rp;
}

}  // namespace

class SmaaBlendPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateRgba8ColorOnlyRenderPass(m_device->GetDevice());

        // smaa_blend.frag expects at set 0:
        //   binding 0: input LDR colour (COMBINED_IMAGE_SAMPLER)
        //   binding 1: blend weights    (COMBINED_IMAGE_SAMPLER)
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_vert = std::make_unique<Shader>(*m_device, ShaderStage::Vertex,
                                          shaderDir + "/smaa.vert.spv");
        m_frag = std::make_unique<Shader>(*m_device, ShaderStage::Fragment,
                                          shaderDir + "/smaa_blend.frag.spv");
    }

    void TearDown() override {
        m_pipeline.reset();
        m_vert.reset();
        m_frag.reset();
        m_layout.reset();
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->GetDevice(), m_renderPass, nullptr);
        }
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::unique_ptr<DescriptorSetLayout> m_layout;
    std::unique_ptr<Shader> m_vert;
    std::unique_ptr<Shader> m_frag;
    std::unique_ptr<SmaaBlendPipeline> m_pipeline;
};

TEST_F(SmaaBlendPipelineTest, SmaaShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa_blend.frag.spv"));
}

TEST_F(SmaaBlendPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SmaaBlendPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SmaaBlendPipelineTest, ConstructsWithMsaa4x) {
    m_pipeline = std::make_unique<SmaaBlendPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(SmaaBlendPipelineTest, DestructorCleansUp) {
    {
        SmaaBlendPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                   m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
}

TEST(SmaaBlendPipelinePushConstants, SizeIsTwelveBytes) {
    // vec2 rcpFrame (8) + float enabled (4) = 12 bytes. The `enabled` field
    // gates the shader's passthrough branch so the RP.11c panel toggle can
    // short-circuit without relying on a stale weights texture.
    EXPECT_EQ(sizeof(SmaaBlendPipeline::PushConstants), 12U);
}

TEST(SmaaBlendPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    EXPECT_EQ(offsetof(SmaaBlendPipeline::PushConstants, rcpFrame), 0U);
    EXPECT_EQ(offsetof(SmaaBlendPipeline::PushConstants, enabled), 8U);
}
