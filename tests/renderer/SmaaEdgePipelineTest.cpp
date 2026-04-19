#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SmaaEdgePipeline.h>
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
using bimeup::renderer::SmaaEdgePipeline;
using bimeup::renderer::VulkanContext;

namespace {

// The edge pass writes the RG8 edges texture — real RP.11c wiring will use
// VK_FORMAT_R8G8_UNORM. For pipeline creation the key constraint is a single
// 2-channel colour attachment; LOAD_OP_DONT_CARE matches how the real pass
// owns the full target.
VkRenderPass CreateRg8ColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R8G8_UNORM;
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

class SmaaEdgePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateRg8ColorOnlyRenderPass(m_device->GetDevice());

        // smaa_edge.frag expects at set 0:
        //   binding 0: input LDR colour (COMBINED_IMAGE_SAMPLER, sampler2D).
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_vert = std::make_unique<Shader>(*m_device, ShaderStage::Vertex,
                                          shaderDir + "/smaa.vert.spv");
        m_frag = std::make_unique<Shader>(*m_device, ShaderStage::Fragment,
                                          shaderDir + "/smaa_edge.frag.spv");
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
    std::unique_ptr<SmaaEdgePipeline> m_pipeline;
};

TEST_F(SmaaEdgePipelineTest, SmaaShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa_edge.frag.spv"));
}

TEST_F(SmaaEdgePipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SmaaEdgePipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SmaaEdgePipelineTest, ConstructsWithMsaa4x) {
    // SMAA 1x always runs on a single-sampled LDR input in practice — but the
    // rasterisation-sample parameter is kept in the contract for shape parity
    // with Fxaa/Bloom/Outline, and so a future direct-MSAA LDR path isn't
    // gated on a pipeline-API change.
    m_pipeline = std::make_unique<SmaaEdgePipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(SmaaEdgePipelineTest, DestructorCleansUp) {
    {
        SmaaEdgePipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                  m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
}

TEST(SmaaEdgePipelinePushConstants, SizeIsSixteenBytes) {
    // vec2 rcpFrame (8) + float threshold (4) + float localContrastFactor (4)
    // = 16 bytes. Well under the 128-byte Vulkan push-constant minimum.
    EXPECT_EQ(sizeof(SmaaEdgePipeline::PushConstants), 16U);
}

TEST(SmaaEdgePipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    SmaaEdgePipeline::PushConstants pc{};
    const auto base = reinterpret_cast<const std::byte*>(&pc);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.rcpFrame) - base, 0);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.threshold) - base, 8);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.localContrastFactor) - base, 12);
}
