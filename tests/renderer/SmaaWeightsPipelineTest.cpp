#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/SmaaWeightsPipeline.h>
#include <renderer/VulkanContext.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::SmaaWeightsPipeline;
using bimeup::renderer::VulkanContext;

namespace {

// Weights pass writes the RGBA8 weights texture consumed by the blend pass
// (RP.11b.4). Matches what RP.11c will wire — LOAD_OP_DONT_CARE since the
// full target is overwritten.
VkRenderPass CreateRgba8ColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
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

class SmaaWeightsPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        s_renderPass = CreateRgba8ColorOnlyRenderPass(s_device->GetDevice());

        // smaa_weights.frag expects at set 0:
        //   binding 0: edges texture (RG8, from smaa_edge.frag)
        //   binding 1: AreaTex LUT (RG8, 160×560)
        //   binding 2: SearchTex LUT (R8, 64×16)
        s_layout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/smaa.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/smaa_weights.frag.spv");
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
    std::unique_ptr<SmaaWeightsPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static VkRenderPass s_renderPass;
    static std::unique_ptr<DescriptorSetLayout> s_layout;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> SmaaWeightsPipelineTest::s_context;
std::unique_ptr<Device> SmaaWeightsPipelineTest::s_device;
VkRenderPass SmaaWeightsPipelineTest::s_renderPass = VK_NULL_HANDLE;
std::unique_ptr<DescriptorSetLayout> SmaaWeightsPipelineTest::s_layout;
std::unique_ptr<Shader> SmaaWeightsPipelineTest::s_vert;
std::unique_ptr<Shader> SmaaWeightsPipelineTest::s_frag;

TEST_F(SmaaWeightsPipelineTest, SmaaShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/smaa_weights.frag.spv"));
}

TEST_F(SmaaWeightsPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<SmaaWeightsPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(SmaaWeightsPipelineTest, ConstructsWithMsaa4x) {
    // SMAA 1x runs single-sampled in practice, but rasterisation-sample
    // parity with Fxaa/Bloom/Outline keeps the future-proofing open.
    m_pipeline = std::make_unique<SmaaWeightsPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(SmaaWeightsPipelineTest, DestructorCleansUp) {
    {
        SmaaWeightsPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                     m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
}

TEST(SmaaWeightsPipelinePushConstants, SizeIsTwentyFourBytes) {
    // vec4 subsampleIndices (16) + vec2 rcpFrame (8) = 24 bytes. vec4 leads
    // so the 16-byte alignment under std430 is satisfied naturally, no pad.
    EXPECT_EQ(sizeof(SmaaWeightsPipeline::PushConstants), 24U);
}

TEST(SmaaWeightsPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    EXPECT_EQ(offsetof(SmaaWeightsPipeline::PushConstants, subsampleIndices), 0U);
    EXPECT_EQ(offsetof(SmaaWeightsPipeline::PushConstants, rcpFrame), 16U);
}

// Walks the SPIR-V OpDecorate stream and asserts sampled images are bound
// at locations 0, 1, and 2 — a regression that drops any of the three
// (edges, AreaTex, SearchTex) would otherwise let the renderer link a
// mismatched descriptor layout against the shader and only trip at Vulkan
// validation time.
TEST_F(SmaaWeightsPipelineTest, FragmentShaderDeclaresBindingsAtLocationsZeroOneTwo) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/smaa_weights.frag.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool found[3] = {false, false, false};
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
                if (bindingIdx < 3) {
                    found[bindingIdx] = true;
                }
            }
        }
        idx += len;
    }
    EXPECT_TRUE(found[0]) << "smaa_weights.frag SPIR-V missing binding 0 (edges)";
    EXPECT_TRUE(found[1]) << "smaa_weights.frag SPIR-V missing binding 1 (AreaTex)";
    EXPECT_TRUE(found[2]) << "smaa_weights.frag SPIR-V missing binding 2 (SearchTex)";
}
