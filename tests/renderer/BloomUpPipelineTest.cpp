#include <gtest/gtest.h>

#include <renderer/BloomUpPipeline.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bimeup::renderer::BloomUpPipeline;
using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

VkRenderPass CreateHdrColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
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

class BloomUpPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateHdrColorOnlyRenderPass(m_device->GetDevice());

        // bloom_up.frag expects at set 0:
        //   binding 0: smaller source mip (COMBINED_IMAGE_SAMPLER).
        // The tent-upsampled contribution is the shader's single output; the
        // actual composite with the higher mip is an RP.10c wiring concern —
        // either via hw additive blend or a separate composite shader.
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_vert = std::make_unique<Shader>(*m_device, ShaderStage::Vertex,
                                          shaderDir + "/bloom.vert.spv");
        m_frag = std::make_unique<Shader>(*m_device, ShaderStage::Fragment,
                                          shaderDir + "/bloom_up.frag.spv");
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
    std::unique_ptr<BloomUpPipeline> m_pipeline;
};

TEST_F(BloomUpPipelineTest, BloomUpFragCompiledToSpirv) {
    // bloom.vert is also checked in BloomDownPipelineTest — here we pin the
    // separate up-frag build output so a glob miss doesn't silently disable
    // the upsample half of the pyramid.
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/bloom_up.frag.spv"));
}

TEST_F(BloomUpPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<BloomUpPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(BloomUpPipelineTest, ConstructsWithMsaa4x) {
    m_pipeline = std::make_unique<BloomUpPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(BloomUpPipelineTest, DestructorCleansUp) {
    {
        BloomUpPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                 m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
}

TEST(BloomUpPipelinePushConstants, SizeIsTwelveBytes) {
    // vec2 rcpSrcSize (8) + float intensity (4) = 12 bytes. No padding
    // needed — Vulkan push constants accept any 4-aligned size. Intensity
    // is 1.0 for intermediate upsamples in the pyramid and the user-picked
    // `bloomIntensity` panel value only for the final composite.
    EXPECT_EQ(sizeof(BloomUpPipeline::PushConstants), 12U);
}

TEST(BloomUpPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    // Pins rcpSrcSize @ 0, intensity @ 8. A swap would still total 12 bytes
    // but would silently scale the sample offsets by `intensity` and apply
    // `rcpSrcSize.x` as a brightness multiplier — visually catastrophic.
    BloomUpPipeline::PushConstants pc{};
    const auto base = reinterpret_cast<const std::byte*>(&pc);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.rcpSrcSize) - base, 0);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.intensity) - base, 8);
}
