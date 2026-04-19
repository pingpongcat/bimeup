#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SsilBlurPipeline.h>
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
using bimeup::renderer::SsilBlurPipeline;
using bimeup::renderer::VulkanContext;

class SsilBlurPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());

        // Descriptor set expected by ssil_blur.comp:
        //   binding 0: SSIL input (combined-image-sampler, RGBA16F)
        //   binding 1: linear depth pyramid (combined-image-sampler) — edge gate
        //   binding 2: normal G-buffer (combined-image-sampler) — normal gate
        //   binding 3: SSIL output (storage image, RGBA16F)
        // Caller binds one descriptor set per pass (H then V) pointing at
        // different SSIL images so the blur can ping-pong.
        s_layout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_compute = std::make_unique<Shader>(*s_device, ShaderStage::Compute,
                                             shaderDir + "/ssil_blur.comp.spv");
    }

    static void TearDownTestSuite() {
        s_compute.reset();
        s_layout.reset();
        s_device.reset();
        s_context.reset();
    }

    void SetUp() override {
        m_device = s_device.get();
        m_layout = s_layout.get();
        m_compute = s_compute.get();
    }
    void TearDown() override { m_pipeline.reset(); }

    Device* m_device = nullptr;
    DescriptorSetLayout* m_layout = nullptr;
    Shader* m_compute = nullptr;
    std::unique_ptr<SsilBlurPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static std::unique_ptr<DescriptorSetLayout> s_layout;
    static std::unique_ptr<Shader> s_compute;
};

std::unique_ptr<VulkanContext> SsilBlurPipelineTest::s_context;
std::unique_ptr<Device> SsilBlurPipelineTest::s_device;
std::unique_ptr<DescriptorSetLayout> SsilBlurPipelineTest::s_layout;
std::unique_ptr<Shader> SsilBlurPipelineTest::s_compute;

TEST_F(SsilBlurPipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/ssil_blur.comp.spv"));
}

TEST_F(SsilBlurPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SsilBlurPipeline>(
        *m_device, *m_compute, m_layout->GetLayout());
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SsilBlurPipelineTest, DestructorCleansUp) {
    {
        SsilBlurPipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch a leaked pipeline/layout.
}

TEST(SsilBlurPipelinePushConstants, SizeIsFourScalars) {
    // direction (ivec2, 8 bytes) + edgeSharpness (float, 4) + normalSharpness
    // (float, 4) = 16 bytes. Two calls per frame (H then V) differ only by
    // the direction push, so keeping the block tiny keeps the command-buffer
    // tape cheap. The depth + normal gates together preserve silhouette AND
    // crease boundaries that RP.7b's per-tap normal rejection already picked
    // up but an isotropic blur would smear.
    EXPECT_EQ(sizeof(SsilBlurPipeline::PushConstants),
              (2U * sizeof(std::int32_t)) + (2U * sizeof(float)));
}
