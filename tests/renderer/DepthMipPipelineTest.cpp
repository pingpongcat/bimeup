#include <gtest/gtest.h>

#include <renderer/DepthMipPipeline.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <string>

using bimeup::renderer::DepthMipPipeline;
using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

class DepthMipPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());

        // binding 0: source mip (linear depth) as combined-image-sampler.
        // binding 1: dest mip (linear depth) as r32f storage image.
        // One layout reused across all mip-level dispatches; the caller binds
        // a per-level descriptor set between dispatches.
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_compute = std::make_unique<Shader>(*m_device, ShaderStage::Compute,
                                             shaderDir + "/depth_mip.comp.spv");
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
    std::unique_ptr<DepthMipPipeline> m_pipeline;
};

TEST_F(DepthMipPipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/depth_mip.comp.spv"));
}

TEST_F(DepthMipPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<DepthMipPipeline>(
        *m_device, *m_compute, m_layout->GetLayout());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(DepthMipPipelineTest, DestructorCleansUp) {
    {
        DepthMipPipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout.
}
