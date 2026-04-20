#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/RtIndoorCompositePipeline.h>
#include <renderer/Shader.h>
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
using bimeup::renderer::RtIndoorCompositePipeline;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

// Stage 9.8.c.2 — Indoor-fill composite compute pass. Mirrors basic.frag's
// useRtIndoorPath==0 fill block (single Lambert dot against the fill
// directional light), but sources visibility from the RtIndoorPass output
// so walls occlude the overhead fill, and applies the contribution
// additively to the HDR image. The pipeline owns only the compute
// pipeline + layout; descriptor resources are caller-owned, matching
// RtSunCompositePipeline (9.8.b.2) so the 9.8.c.3 RenderLoop wire can plug
// in a per-swap descriptor set without wrapping.
//
// Descriptor set laid out as:
//   binding 0: depth G-buffer              (combined-image-sampler)
//   binding 1: oct-packed view-space normal (combined-image-sampler)
//   binding 2: RT indoor visibility         (combined-image-sampler, R8_UNORM)
//   binding 3: LightingUBO                  (uniform buffer)
//   binding 4: HDR image                    (storage image, additive RW)
class RtIndoorCompositePipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());

        s_layout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
                {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
                {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_compute = std::make_unique<Shader>(*s_device, ShaderStage::Compute,
                                             shaderDir + "/rt_indoor_composite.comp.spv");
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
    std::unique_ptr<RtIndoorCompositePipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static std::unique_ptr<DescriptorSetLayout> s_layout;
    static std::unique_ptr<Shader> s_compute;
};

std::unique_ptr<VulkanContext> RtIndoorCompositePipelineTest::s_context;
std::unique_ptr<Device> RtIndoorCompositePipelineTest::s_device;
std::unique_ptr<DescriptorSetLayout> RtIndoorCompositePipelineTest::s_layout;
std::unique_ptr<Shader> RtIndoorCompositePipelineTest::s_compute;

TEST_F(RtIndoorCompositePipelineTest, ShaderCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/rt_indoor_composite.comp.spv"));
}

TEST_F(RtIndoorCompositePipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<RtIndoorCompositePipeline>(
        *m_device, *m_compute, m_layout->GetLayout());
    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(RtIndoorCompositePipelineTest, DestructorCleansUp) {
    {
        RtIndoorCompositePipeline pipeline(*m_device, *m_compute, m_layout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch a leaked pipeline/layout.
}

TEST(RtIndoorCompositePipelinePushConstants, SizeIs144Bytes) {
    // Same shape as RtSunCompositePipeline::PushConstants:
    // mat4 invViewProj (64) + mat4 invView (64) + uvec2 extent (8) +
    // uint pad0 (4) + uint pad1 (4) = 144 B. Pinned so the shader's push
    // block can't drift from the CPU struct. Above the 128 B Vulkan-1.2
    // minimum guarantee — acceptable because this pass is Hybrid-RT only
    // and RT-capable GPUs advertise ≥256 B.
    EXPECT_EQ(sizeof(RtIndoorCompositePipeline::PushConstants), 144U);
}

TEST(RtIndoorCompositePipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    EXPECT_EQ(offsetof(RtIndoorCompositePipeline::PushConstants, invViewProj), 0U);
    EXPECT_EQ(offsetof(RtIndoorCompositePipeline::PushConstants, invView), 64U);
    EXPECT_EQ(offsetof(RtIndoorCompositePipeline::PushConstants, extent), 128U);
}

// Walks the compiled SPIR-V and asserts every expected descriptor binding
// is declared. Catches binding-drop or renumbering before Vulkan validation
// fires at dispatch time.
TEST_F(RtIndoorCompositePipelineTest, ComputeShaderDeclaresAllFiveBindings) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/rt_indoor_composite.comp.spv", std::ios::binary);
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
    for (uint32_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(found[i]) << "rt_indoor_composite.comp SPIR-V missing binding " << i;
    }
}
