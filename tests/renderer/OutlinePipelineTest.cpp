#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/OutlinePipeline.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <glm/vec4.hpp>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::OutlinePipeline;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

// Outline draws into the present pass (tonemap output), which is a 1-sample
// color-only swapchain target. Mirroring the RenderLoop's real present pass
// here so the pipeline-build test exercises the same shape it will at runtime.
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

class OutlinePipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        s_renderPass = CreateColorOnlyRenderPass(s_device->GetDevice());

        // outline.frag expects at set 0:
        //   binding 0: stencil id (usampler2D, COMBINED_IMAGE_SAMPLER)
        //   binding 1: linear depth (sampler2D, COMBINED_IMAGE_SAMPLER)
        // usampler/sampler distinction is a SPIR-V decoration only — the
        // Vulkan descriptor type is COMBINED_IMAGE_SAMPLER for both.
        s_layout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/outline.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/outline.frag.spv");
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
    std::unique_ptr<OutlinePipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static VkRenderPass s_renderPass;
    static std::unique_ptr<DescriptorSetLayout> s_layout;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> OutlinePipelineTest::s_context;
std::unique_ptr<Device> OutlinePipelineTest::s_device;
VkRenderPass OutlinePipelineTest::s_renderPass = VK_NULL_HANDLE;
std::unique_ptr<DescriptorSetLayout> OutlinePipelineTest::s_layout;
std::unique_ptr<Shader> OutlinePipelineTest::s_vert;
std::unique_ptr<Shader> OutlinePipelineTest::s_frag;

TEST_F(OutlinePipelineTest, OutlineShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/outline.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/outline.frag.spv"));
}

TEST_F(OutlinePipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<OutlinePipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(OutlinePipelineTest, ConstructsWithMsaa4x) {
    m_pipeline = std::make_unique<OutlinePipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(OutlinePipelineTest, DestructorCleansUp) {
    {
        OutlinePipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                 m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout.
}

TEST(OutlinePipelinePushConstants, SizeIsFortyEightBytes) {
    // vec4 selectedColor + vec4 hoverColor + vec2 texelSize + float thickness +
    // float depthEdgeThreshold. Panel-tweakable knobs live in push constants
    // (under the 128-byte Vulkan guarantee), no UBO needed. Mirrors the
    // `push_constant` block in outline.frag.
    EXPECT_EQ(sizeof(OutlinePipeline::PushConstants), 48U);
}

TEST(OutlinePipelinePushConstants, ColorOffsetsAreAlignedForStd140) {
    // Vulkan push_constant blocks follow std140-like rules: vec4s align to 16.
    // The C++ struct uses `alignas(16)` on both colour members to match the
    // GLSL layout so memcpy into `vkCmdPushConstants` stays in sync.
    OutlinePipeline::PushConstants pc{};
    const auto base = reinterpret_cast<const std::byte*>(&pc);
    const auto selOff = reinterpret_cast<const std::byte*>(&pc.selectedColor) - base;
    const auto hovOff = reinterpret_cast<const std::byte*>(&pc.hoverColor) - base;
    EXPECT_EQ(selOff, 0);
    EXPECT_EQ(hovOff, 16);
}
