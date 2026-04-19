#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SsilPipeline.h>
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
using bimeup::renderer::SsilPipeline;
using bimeup::renderer::VulkanContext;

class SsilPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());

        // Descriptor set that ssil_main.comp expects:
        //   binding 0: linear depth pyramid          (combined-image-sampler)
        //   binding 1: view-space normal G-buffer    (combined-image-sampler)
        //   binding 2: previous-frame HDR            (combined-image-sampler)
        //   binding 3: SsilUbo                       (uniform buffer)
        //   binding 4: RGBA16F half-res SSIL target  (storage image)
        //   binding 5: stencil G-buffer (R8_UINT)    (combined-image-sampler,
        //              RP.12c.2 — taps with bit 4 set contribute 0)
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
                {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
                {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_compute = std::make_unique<Shader>(*m_device, ShaderStage::Compute,
                                             shaderDir + "/ssil_main.comp.spv");
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
    std::unique_ptr<SsilPipeline> m_pipeline;
};

TEST_F(SsilPipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/ssil_main.comp.spv"));
}

TEST_F(SsilPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SsilPipeline>(
        *m_device, *m_compute, m_layout->GetLayout());
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SsilPipelineTest, DestructorCleansUp) {
    {
        SsilPipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch a leaked pipeline/layout.
}

TEST(SsilPipelinePushConstants, SizeIsFiveFloats) {
    // RP.7b: radius + intensity + normalRejection + frameSeed.
    // RP.12c: + maxLuminance (post-accumulation per-channel clamp). Matrices +
    // kernel live in the UBO at binding 3. 20 bytes keeps the block well
    // under the 128-byte Vulkan guarantee.
    EXPECT_EQ(sizeof(SsilPipeline::PushConstants), 5U * sizeof(float));
}

TEST(SsilPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    // Pin the field order against `ssil_main.comp`'s push-constant block so a
    // CPU-side reorder can't silently swap radius with intensity etc. The
    // shader reads these by name in the same scalar-pack order.
    EXPECT_EQ(offsetof(SsilPipeline::PushConstants, radius), 0U);
    EXPECT_EQ(offsetof(SsilPipeline::PushConstants, intensity), 4U);
    EXPECT_EQ(offsetof(SsilPipeline::PushConstants, normalRejection), 8U);
    EXPECT_EQ(offsetof(SsilPipeline::PushConstants, frameSeed), 12U);
    EXPECT_EQ(offsetof(SsilPipeline::PushConstants, maxLuminance), 16U);
}

// RP.12c.2 — walks the compiled `ssil_main.comp.spv` OpDecorate stream and
// asserts the stencil G-buffer is declared at set 0 / binding 5. Catches a
// regression where the binding drops (or drifts to a different index) before
// it trips Vulkan validation at dispatch time.
TEST_F(SsilPipelineTest, ComputeShaderDeclaresStencilBindingAtFive) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/ssil_main.comp.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool found[6] = {false, false, false, false, false, false};
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
                if (bindingIdx < 6) {
                    found[bindingIdx] = true;
                }
            }
        }
        idx += len;
    }
    EXPECT_TRUE(found[5]) << "ssil_main.comp SPIR-V missing binding 5 (stencil G-buffer)";
}
