#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/EdgeOverlayPipeline.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::EdgeOverlayPipeline;
using bimeup::renderer::LayoutBinding;
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

class EdgeOverlayPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        s_renderPass = CreateColorDepthRenderPass(s_device->GetDevice());
        // RP.17.8.a — the overlay fragment now samples the clip-planes UBO at
        // binding 3 to discard fragments behind any active axis plane, so the
        // descriptor set layout handed to the pipeline must declare that binding
        // alongside the camera UBO at binding 0.
        s_dsLayout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
                {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT}});

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/edge_overlay.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/edge_overlay.frag.spv");
    }

    static void TearDownTestSuite() {
        s_vert.reset();
        s_frag.reset();
        s_dsLayout.reset();
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
        m_dsLayout = s_dsLayout.get();
        m_vert = s_vert.get();
        m_frag = s_frag.get();
    }
    void TearDown() override { m_pipeline.reset(); }

    Device* m_device = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    DescriptorSetLayout* m_dsLayout = nullptr;
    Shader* m_vert = nullptr;
    Shader* m_frag = nullptr;
    std::unique_ptr<EdgeOverlayPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static VkRenderPass s_renderPass;
    static std::unique_ptr<DescriptorSetLayout> s_dsLayout;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> EdgeOverlayPipelineTest::s_context;
std::unique_ptr<Device> EdgeOverlayPipelineTest::s_device;
VkRenderPass EdgeOverlayPipelineTest::s_renderPass = VK_NULL_HANDLE;
std::unique_ptr<DescriptorSetLayout> EdgeOverlayPipelineTest::s_dsLayout;
std::unique_ptr<Shader> EdgeOverlayPipelineTest::s_vert;
std::unique_ptr<Shader> EdgeOverlayPipelineTest::s_frag;

TEST_F(EdgeOverlayPipelineTest, EdgeOverlayShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/edge_overlay.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/edge_overlay.frag.spv"));
}

TEST_F(EdgeOverlayPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<EdgeOverlayPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_dsLayout->GetLayout());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(EdgeOverlayPipelineTest, ConstructsForMrtMainPass) {
    // MRT main pass has 3 colour attachments (HDR + normal G-buffer +
    // transparency stencil); the overlay only writes attachment 0, so
    // `disableSecondaryColorWrites = true` keeps the normal/stencil buffers
    // intact when this pipeline is bound inside the main pass.
    m_pipeline = std::make_unique<EdgeOverlayPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_dsLayout->GetLayout(),
        /*colorAttachmentCount=*/1,
        /*disableSecondaryColorWrites=*/true);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(EdgeOverlayPipelineTest, ConstructsWithClipPlaneLayout) {
    // RP.17.8.a — asserts the pipeline accepts a descriptor set layout that
    // declares binding 3 (ClipPlanesUBO) in addition to the camera UBO at
    // binding 0. The overlay fragment shader samples that UBO to discard
    // fragments behind any active axis clip plane; a layout missing binding 3
    // would fail SPIR-V reflection during pipeline creation.
    DescriptorSetLayout layoutWithClipPlanes(
        *m_device,
        std::vector<LayoutBinding>{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
            {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT}});

    m_pipeline = std::make_unique<EdgeOverlayPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        layoutWithClipPlanes.GetLayout());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(EdgeOverlayPipelineTest, ConstructsWithSmoothLinesWhenSupported) {
    if (!m_device->HasSmoothLines()) {
        GTEST_SKIP() << "VK_EXT_line_rasterization / smoothLines not supported "
                        "by this device — falling back to aliased lines.";
    }
    m_pipeline = std::make_unique<EdgeOverlayPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_dsLayout->GetLayout(),
        /*colorAttachmentCount=*/1,
        /*disableSecondaryColorWrites=*/false,
        /*smoothLines=*/true);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(EdgeOverlayPipelineTest, DestructorCleansUp) {
    {
        EdgeOverlayPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                     m_dsLayout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout
}
