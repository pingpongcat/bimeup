#include <gtest/gtest.h>

#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/TonemapPipeline.h>
#include <renderer/VulkanContext.h>

#include <glm/glm.hpp>

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
using bimeup::renderer::TonemapPipeline;
using bimeup::renderer::VulkanContext;

namespace {

// Tonemap is the final resolve-to-LDR pass, so it targets a color-only render
// pass (no depth). sRGB format matches the real swapchain final target.
VkRenderPass CreateColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_B8G8R8A8_SRGB;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

class TonemapPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateColorOnlyRenderPass(m_device->GetDevice());
        // tonemap.frag: binding 0 = HDR colour, binding 1 = half-res AO
        // (RP.5d), binding 2 = half-res SSIL (RP.7d), binding 3 = depth
        // pyramid mip 0 for the RP.9b fog factor, binding 4 = bloom mip 0
        // (RP.10c).
        m_samplerLayout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}});

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_vert = std::make_unique<Shader>(*m_device, ShaderStage::Vertex,
                                          shaderDir + "/tonemap.vert.spv");
        m_frag = std::make_unique<Shader>(*m_device, ShaderStage::Fragment,
                                          shaderDir + "/tonemap.frag.spv");
    }

    void TearDown() override {
        m_pipeline.reset();
        m_vert.reset();
        m_frag.reset();
        m_samplerLayout.reset();
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->GetDevice(), m_renderPass, nullptr);
        }
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::unique_ptr<DescriptorSetLayout> m_samplerLayout;
    std::unique_ptr<Shader> m_vert;
    std::unique_ptr<Shader> m_frag;
    std::unique_ptr<TonemapPipeline> m_pipeline;
};

TEST_F(TonemapPipelineTest, TonemapShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/tonemap.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/tonemap.frag.spv"));
}

TEST_F(TonemapPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<TonemapPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_samplerLayout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(TonemapPipelineTest, ConstructsWithMsaa4x) {
    m_pipeline = std::make_unique<TonemapPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_samplerLayout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

// RP.7d — tonemap.frag's third sampled binding is the half-res SSIL target.
// Walks the SPIR-V OpDecorate stream and asserts a sampled image exists at
// (set 0, binding 2); a regression that drops the binding would let the
// renderer link a 2-binding layout against a 3-binding shader and only trip
// at validation time.
TEST_F(TonemapPipelineTest, FragmentShaderDeclaresSsilBindingAtLocationTwo) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/tonemap.frag.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool foundBinding2 = false;
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
            if (decoration == 33 && w[idx + 3] == 2U) {
                foundBinding2 = true;
            }
        }
        idx += len;
    }
    EXPECT_TRUE(foundBinding2)
        << "tonemap.frag SPIR-V missing expected binding 2 (SSIL)";
}

TEST_F(TonemapPipelineTest, DestructorCleansUp) {
    {
        TonemapPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                 m_samplerLayout->GetLayout(),
                                 VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout
}

// RP.9b — tonemap.frag's binding-3 sampler is the depth pyramid mip 0
// source for the fog factor. Walks the SPIR-V OpDecorate stream and asserts
// a sampled image exists at (set 0, binding 3); a regression that dropped
// the binding would let the renderer link a 3-binding layout against a
// 4-binding shader and only trip at validation time.
TEST_F(TonemapPipelineTest, FragmentShaderDeclaresDepthBindingAtLocationThree) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/tonemap.frag.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool foundBinding3 = false;
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
            if (decoration == 33 && w[idx + 3] == 3U) {
                foundBinding3 = true;
            }
        }
        idx += len;
    }
    EXPECT_TRUE(foundBinding3)
        << "tonemap.frag SPIR-V missing expected binding 3 (depth pyramid)";
}

// RP.10c — the push-constant contract between tonemap.frag and the CPU
// struct. `vec4 fogColorEnabled` at offset 0 + `float fogStart` at 16 +
// `float fogEnd` at 20 + `float bloomIntensity` at 24 + `float bloomEnabled`
// at 28 = 32 bytes total. A reorder that still totals 32 bytes would need
// another guard but would not fail *this* test — that guard lives in
// FieldOffsetsMatchShaderLayout below.
TEST(TonemapPushConstantsTest, SizeIsThirtyTwoBytes) {
    EXPECT_EQ(sizeof(TonemapPipeline::PushConstants), 32U);
}

TEST(TonemapPushConstantsTest, FieldOffsetsMatchShaderLayout) {
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, fogColorEnabled), 0U);
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, fogStart), 16U);
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, fogEnd), 20U);
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, bloomIntensity), 24U);
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, bloomEnabled), 28U);
}

// RP.10c — tonemap.frag's binding-4 sampler is the bloom pyramid mip 0
// source for the composite. Walks the SPIR-V OpDecorate stream and asserts
// a sampled image exists at (set 0, binding 4); a regression that dropped
// the binding would let the renderer link a 4-binding layout against a
// 5-binding shader and only trip at validation time.
TEST_F(TonemapPipelineTest, FragmentShaderDeclaresBloomBindingAtLocationFour) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/tonemap.frag.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool foundBinding4 = false;
    size_t idx = 5;  // skip SPIR-V header
    while (idx < wordCount) {
        uint32_t word = w[idx];
        uint32_t opcode = word & 0xFFFFU;
        uint32_t len = (word >> 16U) & 0xFFFFU;
        if (len == 0 || idx + len > wordCount) {
            break;
        }
        if (opcode == 71 && len >= 4) {
            uint32_t decoration = w[idx + 2];
            if (decoration == 33 && w[idx + 3] == 4U) {
                foundBinding4 = true;
            }
        }
        idx += len;
    }
    EXPECT_TRUE(foundBinding4)
        << "tonemap.frag SPIR-V missing expected binding 4 (bloom mip 0)";
}
