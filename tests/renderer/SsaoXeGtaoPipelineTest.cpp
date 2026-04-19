#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SsaoXeGtaoPipeline.h>
#include <renderer/VulkanContext.h>

#include <cstddef>
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
using bimeup::renderer::SsaoXeGtaoPipeline;
using bimeup::renderer::VulkanContext;

class SsaoXeGtaoPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());

        // Descriptor set ssao_xegtao.comp expects (mirrors the classic SSAO
        // layout so RP.12e.3 can swap pipelines without re-plumbing RenderLoop):
        //   binding 0: linear depth pyramid        (combined-image-sampler)
        //   binding 1: view-space normal G-buffer  (combined-image-sampler)
        //   binding 2: XeGtaoUbo { proj, invProj } (uniform buffer — no kernel[])
        //   binding 3: R8 half-res AO target       (storage image)
        //   binding 4: transparency stencil G-buffer (combined-image-sampler, RP.12d)
        s_layout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
                {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_compute = std::make_unique<Shader>(*s_device, ShaderStage::Compute,
                                             shaderDir + "/ssao_xegtao.comp.spv");
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
    std::unique_ptr<SsaoXeGtaoPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static std::unique_ptr<DescriptorSetLayout> s_layout;
    static std::unique_ptr<Shader> s_compute;
};

std::unique_ptr<VulkanContext> SsaoXeGtaoPipelineTest::s_context;
std::unique_ptr<Device> SsaoXeGtaoPipelineTest::s_device;
std::unique_ptr<DescriptorSetLayout> SsaoXeGtaoPipelineTest::s_layout;
std::unique_ptr<Shader> SsaoXeGtaoPipelineTest::s_compute;

TEST_F(SsaoXeGtaoPipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/ssao_xegtao.comp.spv"));
}

TEST_F(SsaoXeGtaoPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SsaoXeGtaoPipeline>(
        *m_device, *m_compute, m_layout->GetLayout());
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SsaoXeGtaoPipelineTest, DestructorCleansUp) {
    {
        SsaoXeGtaoPipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch a leaked pipeline/layout.
}

TEST(SsaoXeGtaoPipelinePushConstants, SizeIsFourFloats) {
    // radius + falloff + intensity + shadowPower — matches the 16-byte slot
    // the Chapman `SsaoPipeline::PushConstants` used, so the RP.12e.3
    // RenderLoop swap doesn't have to re-size the push range. No Chapman
    // bias field — XeGTAO's horizon integration doesn't use depth-compare.
    EXPECT_EQ(sizeof(SsaoXeGtaoPipeline::PushConstants), 4U * sizeof(float));
}

TEST(SsaoXeGtaoPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    EXPECT_EQ(offsetof(SsaoXeGtaoPipeline::PushConstants, radius), 0U);
    EXPECT_EQ(offsetof(SsaoXeGtaoPipeline::PushConstants, falloff), 4U);
    EXPECT_EQ(offsetof(SsaoXeGtaoPipeline::PushConstants, intensity), 8U);
    EXPECT_EQ(offsetof(SsaoXeGtaoPipeline::PushConstants, shadowPower), 12U);
}

// RP.12d transparency-gate contract carried over to the XeGTAO port —
// walks the compiled `ssao_xegtao.comp.spv` OpDecorate stream and asserts
// the stencil G-buffer sits at set 0 / binding 4, matching the old SSAO.
// Catches a binding-drop before Vulkan validation fires at dispatch time.
TEST_F(SsaoXeGtaoPipelineTest, ComputeShaderDeclaresStencilBindingAtFour) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/ssao_xegtao.comp.spv", std::ios::binary);
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
    EXPECT_TRUE(found[4]) << "ssao_xegtao.comp SPIR-V missing binding 4 (stencil G-buffer)";
}
