#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SsaoBlurPipeline.h>
#include <renderer/VulkanContext.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::SsaoBlurPipeline;
using bimeup::renderer::VulkanContext;

class SsaoBlurPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());

        // Descriptor set expected by ssao_blur.comp:
        //   binding 0: AO input (combined-image-sampler, R8)
        //   binding 1: linear depth pyramid (combined-image-sampler) — edge gate
        //   binding 2: AO output (storage image, R8)
        // Caller binds one descriptor set per pass (H then V) pointing at
        // different AO images so the blur can ping-pong.
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_compute = std::make_unique<Shader>(*m_device, ShaderStage::Compute,
                                             shaderDir + "/ssao_blur.comp.spv");
    }

    void TearDown() override {
        m_pipeline.reset();
        m_compute.reset();
        m_layout.reset();
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<DescriptorSetLayout> m_layout;
    std::unique_ptr<Shader> m_compute;
    std::unique_ptr<SsaoBlurPipeline> m_pipeline;
};

TEST_F(SsaoBlurPipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/ssao_blur.comp.spv"));
}

TEST_F(SsaoBlurPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SsaoBlurPipeline>(
        *m_device, *m_compute, m_layout->GetLayout());
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SsaoBlurPipelineTest, DestructorCleansUp) {
    {
        SsaoBlurPipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch a leaked pipeline/layout.
}

TEST(SsaoBlurPipelinePushConstants, SizeIsThreeScalars) {
    // direction (ivec2, 8 bytes) + edgeSharpness (float, 4 bytes) = 12 bytes.
    // Two calls per frame (H then V) differ only by the direction push, so
    // keeping the block tiny keeps the command-buffer tape cheap.
    EXPECT_EQ(sizeof(SsaoBlurPipeline::PushConstants),
              (2U * sizeof(std::int32_t)) + sizeof(float));
}
