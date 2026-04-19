#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SsaoPipeline.h>
#include <renderer/VulkanContext.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::SsaoPipeline;
using bimeup::renderer::VulkanContext;

class SsaoPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());

        // Descriptor set that ssao_main.comp expects:
        //   binding 0: linear depth pyramid  (combined-image-sampler)
        //   binding 1: view-space normal G-buffer (combined-image-sampler)
        //   binding 2: SsaoUbo { proj, invProj, kernel[]... } (uniform buffer)
        //   binding 3: R8 half-res AO target (storage image)
        //   binding 4: outline stencil G-buffer (combined-image-sampler, RP.12d)
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
                {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_compute = std::make_unique<Shader>(*m_device, ShaderStage::Compute,
                                             shaderDir + "/ssao_main.comp.spv");
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
    std::unique_ptr<SsaoPipeline> m_pipeline;
};

TEST_F(SsaoPipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/ssao_main.comp.spv"));
}

TEST_F(SsaoPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SsaoPipeline>(
        *m_device, *m_compute, m_layout->GetLayout());
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SsaoPipelineTest, DestructorCleansUp) {
    {
        SsaoPipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch a leaked pipeline/layout.
}

TEST(SsaoPipelinePushConstants, SizeIsFourFloats) {
    // radius + bias + intensity + shadowPower — the small, panel-tweakable
    // knobs live in push constants; matrices + kernel live in the UBO at
    // binding 2. 16 bytes keeps the constant block well under the 128-byte
    // Vulkan guarantee.
    EXPECT_EQ(sizeof(SsaoPipeline::PushConstants), 4U * sizeof(float));
}

// RP.12d — walks the compiled `ssao_main.comp.spv` OpDecorate stream and
// asserts the stencil G-buffer is declared at set 0 / binding 4. Catches a
// regression where the binding drops (or drifts to a different index) before
// it trips Vulkan validation at dispatch time. Mirrors the equivalent SSIL
// pin test added with RP.12c.2.
TEST_F(SsaoPipelineTest, ComputeShaderDeclaresStencilBindingAtFour) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/ssao_main.comp.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool found[5] = {false, false, false, false, false};
    size_t idx = 5;  // skip SPIR-V header
    while (idx < wordCount) {
        uint32_t word = w[idx];
        uint32_t opcode = word & 0xFFFFU;
        uint32_t len = (word >> 16U) & 0xFFFFU;
        if (len == 0 || idx + len > wordCount) {
            break;
        }
        // OpDecorate = 71. Arguments: target, decoration, literals...
        if (opcode == 71 && len >= 4) {
            uint32_t decoration = w[idx + 2];
            // Decoration::Binding = 33.
            if (decoration == 33) {
                uint32_t bindingIdx = w[idx + 3];
                if (bindingIdx < 5) {
                    found[bindingIdx] = true;
                }
            }
        }
        idx += len;
    }
    EXPECT_TRUE(found[4]) << "ssao_main.comp SPIR-V missing binding 4 (stencil G-buffer)";
}
