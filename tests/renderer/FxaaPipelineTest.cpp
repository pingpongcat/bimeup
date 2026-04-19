#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/FxaaPipeline.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::FxaaPipeline;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

// FXAA runs in the present pass, writing the final anti-aliased LDR image to
// the swapchain — single sRGB color attachment, no depth, load-op LOAD so it
// composes over whatever tonemap + outline wrote. Mirrors the RenderLoop's
// real present pass shape (RP.8c will flip the composite order to tonemap →
// outline → FXAA → swapchain); this test render pass exists only to satisfy
// the pipeline-creation contract.
VkRenderPass CreateColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_B8G8R8A8_SRGB;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

class FxaaPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateColorOnlyRenderPass(m_device->GetDevice());

        // fxaa.frag expects at set 0:
        //   binding 0: input LDR colour (COMBINED_IMAGE_SAMPLER, sampler2D)
        // Single binding — FXAA is a pure colour-to-colour filter, no depth
        // or normal inputs (unlike the outline pass at RP.6b).
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_vert = std::make_unique<Shader>(*m_device, ShaderStage::Vertex,
                                          shaderDir + "/fxaa.vert.spv");
        m_frag = std::make_unique<Shader>(*m_device, ShaderStage::Fragment,
                                          shaderDir + "/fxaa.frag.spv");
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
    std::unique_ptr<FxaaPipeline> m_pipeline;
};

TEST_F(FxaaPipelineTest, FxaaShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/fxaa.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/fxaa.frag.spv"));
}

TEST_F(FxaaPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<FxaaPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(FxaaPipelineTest, ConstructsWithMsaa4x) {
    // Swapchain is single-sampled in practice (the MRT render pass resolves
    // MSAA before tonemap lands in the present pass), but keeping the
    // rasterisation-sample count parametrised future-proofs the pipeline for
    // a direct-MSAA present path and mirrors OutlinePipeline's shape.
    m_pipeline = std::make_unique<FxaaPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(FxaaPipelineTest, DestructorCleansUp) {
    {
        FxaaPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                              m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout.
}

TEST(FxaaPipelinePushConstants, SizeIsTwentyFourBytes) {
    // vec2 rcpFrame (8) + float subpixel (4) + float edgeThreshold (4) +
    // float edgeThresholdMin (4) + int quality (4) = 24 bytes. Panel knobs
    // live in push constants (under the 128-byte Vulkan guarantee); no UBO
    // needed. `quality` is an int32 so the shader can branch LOW (0) / HIGH
    // (1) without a pipeline rebuild — cleaner than a specialization constant
    // for this two-preset case since the Pipeline class doesn't wire
    // specialization info through today.
    EXPECT_EQ(sizeof(FxaaPipeline::PushConstants), 24U);
}

TEST(FxaaPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    // Vulkan push_constant block layout rules: scalars 4-aligned, vec2
    // 8-aligned. Pins the exact byte offsets so a field reorder that still
    // totals 24 bytes (e.g. swapping subpixel and rcpFrame) gets caught here,
    // not at validation time or as a visual regression.
    FxaaPipeline::PushConstants pc{};
    const auto base = reinterpret_cast<const std::byte*>(&pc);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.rcpFrame) - base, 0);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.subpixel) - base, 8);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.edgeThreshold) - base, 12);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.edgeThresholdMin) - base, 16);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.quality) - base, 20);
}
