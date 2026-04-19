#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/SectionFillPipeline.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::SectionFillPipeline;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

VkRenderPass CreateColorDepthRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_B8G8R8A8_SRGB;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth{};
    depth.format = VK_FORMAT_D32_SFLOAT;
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription attachments[2] = {color, depth};

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    VkRenderPass rp = VK_NULL_HANDLE;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &rp) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create test render pass");
    }
    return rp;
}

}  // namespace

class SectionFillPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        s_renderPass = CreateColorDepthRenderPass(s_device->GetDevice());
        s_cameraLayout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}});

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/section_fill.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/section_fill.frag.spv");
    }

    static void TearDownTestSuite() {
        s_vert.reset();
        s_frag.reset();
        s_cameraLayout.reset();
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
        m_cameraLayout = s_cameraLayout.get();
        m_vert = s_vert.get();
        m_frag = s_frag.get();
    }
    void TearDown() override { m_pipeline.reset(); }

    Device* m_device = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    DescriptorSetLayout* m_cameraLayout = nullptr;
    Shader* m_vert = nullptr;
    Shader* m_frag = nullptr;
    std::unique_ptr<SectionFillPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static VkRenderPass s_renderPass;
    static std::unique_ptr<DescriptorSetLayout> s_cameraLayout;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> SectionFillPipelineTest::s_context;
std::unique_ptr<Device> SectionFillPipelineTest::s_device;
VkRenderPass SectionFillPipelineTest::s_renderPass = VK_NULL_HANDLE;
std::unique_ptr<DescriptorSetLayout> SectionFillPipelineTest::s_cameraLayout;
std::unique_ptr<Shader> SectionFillPipelineTest::s_vert;
std::unique_ptr<Shader> SectionFillPipelineTest::s_frag;

TEST_F(SectionFillPipelineTest, SectionFillShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/section_fill.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/section_fill.frag.spv"));
}

TEST_F(SectionFillPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SectionFillPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_cameraLayout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SectionFillPipelineTest, ConstructsWithMsaa4x) {
    m_pipeline = std::make_unique<SectionFillPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_cameraLayout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(SectionFillPipelineTest, DestructorCleansUp) {
    {
        SectionFillPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                     m_cameraLayout->GetLayout(),
                                     VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout
}
