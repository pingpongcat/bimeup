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
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        s_renderPass = CreateRg8ColorOnlyRenderPass(s_device->GetDevice());

        // smaa_edge.frag expects at set 0:
        //   binding 0: input LDR colour (COMBINED_IMAGE_SAMPLER, sampler2D).
        s_layout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/smaa.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/smaa_edge.frag.spv");
    }

    static void TearDownTestSuite() {
        s_vert.reset();
        s_frag.reset();
        s_layout.reset();
        if (s_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(s_device->GetDevice(), s_renderPass, nullptr);
            s_renderPass = VK_NULL_HANDLE;
        }
        s_device.reset();
        s_context.reset();
    }

    void SetUp() override {
        m_device = s_device.get();
        m_renderPass = s_renderPass;
        m_layout = s_layout.get();
        m_vert = s_vert.get();
        m_frag = s_frag.get();
    }
    void TearDown() override { m_pipeline.reset(); }

    Device* m_device = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    DescriptorSetLayout* m_layout = nullptr;
    Shader* m_vert = nullptr;
    Shader* m_frag = nullptr;
    std::unique_ptr<SmaaEdgePipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static VkRenderPass s_renderPass;
    static std::unique_ptr<DescriptorSetLayout> s_layout;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> SmaaEdgePipelineTest::s_context;
std::unique_ptr<Device> SmaaEdgePipelineTest::s_device;
VkRenderPass SmaaEdgePipelineTest::s_renderPass = VK_NULL_HANDLE;
std::unique_ptr<DescriptorSetLayout> SmaaEdgePipelineTest::s_layout;
std::unique_ptr<Shader> SmaaEdgePipelineTest::s_vert;
std::unique_ptr<Shader> SmaaEdgePipelineTest::s_frag;

TEST_F(SmaaEdgePipelineTest, SmaaShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa_edge.frag.spv"));
}

TEST_F(SmaaEdgePipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SmaaEdgePipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SmaaEdgePipelineTest, DestructorCleansUp) {
    {
        SmaaEdgePipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                  m_layout->GetLayout());
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
