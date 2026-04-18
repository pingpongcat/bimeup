#include <gtest/gtest.h>

#include <renderer/DepthLinearizePipeline.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <string>

using bimeup::renderer::DepthLinearizePipeline;
using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

class DepthLinearizePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());

        // binding 0: non-linear depth as combined-image-sampler (the depth
        // attachment post main-pass, layout SHADER_READ_ONLY_OPTIMAL).
        // binding 1: linear-depth output as storage image.
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_compute = std::make_unique<Shader>(*m_device, ShaderStage::Compute,
                                             shaderDir + "/depth_linearize.comp.spv");
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
    std::unique_ptr<DepthLinearizePipeline> m_pipeline;
};

TEST_F(DepthLinearizePipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/depth_linearize.comp.spv"));
}

TEST_F(DepthLinearizePipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<DepthLinearizePipeline>(
        *m_device, *m_compute, m_layout->GetLayout());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(DepthLinearizePipelineTest, DestructorCleansUp) {
    {
        DepthLinearizePipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout.
}

TEST(DepthLinearizePipelinePushConstants, SizeIsTwoFloats) {
    // Near + far are the only inputs; the shader reads them via a single
    // push-constant block — a bigger POD would eat into the 128-byte budget
    // without cause.
    EXPECT_EQ(sizeof(DepthLinearizePipeline::PushConstants), 2U * sizeof(float));
}
