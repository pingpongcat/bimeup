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
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        s_renderPass = CreateColorOnlyRenderPass(s_device->GetDevice());
        // tonemap.frag: binding 0 = HDR colour, binding 1 = XeGTAO AO
        // (RP.5d), binding 2 = RT AO (Stage 9.8.a; mixed with binding 1 via
        // the `useRtAo` push constant).
        s_samplerLayout = std::make_unique<DescriptorSetLayout>(
            *s_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 VK_SHADER_STAGE_FRAGMENT_BIT}});

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/tonemap.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/tonemap.frag.spv");
    }

    static void TearDownTestSuite() {
        s_vert.reset();
        s_frag.reset();
        s_samplerLayout.reset();
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
        m_samplerLayout = s_samplerLayout.get();
        m_vert = s_vert.get();
        m_frag = s_frag.get();
    }
    void TearDown() override { m_pipeline.reset(); }

    Device* m_device = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    DescriptorSetLayout* m_samplerLayout = nullptr;
    Shader* m_vert = nullptr;
    Shader* m_frag = nullptr;
    std::unique_ptr<TonemapPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static VkRenderPass s_renderPass;
    static std::unique_ptr<DescriptorSetLayout> s_samplerLayout;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> TonemapPipelineTest::s_context;
std::unique_ptr<Device> TonemapPipelineTest::s_device;
VkRenderPass TonemapPipelineTest::s_renderPass = VK_NULL_HANDLE;
std::unique_ptr<DescriptorSetLayout> TonemapPipelineTest::s_samplerLayout;
std::unique_ptr<Shader> TonemapPipelineTest::s_vert;
std::unique_ptr<Shader> TonemapPipelineTest::s_frag;

TEST_F(TonemapPipelineTest, TonemapShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/tonemap.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/tonemap.frag.spv"));
}

TEST_F(TonemapPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<TonemapPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_samplerLayout->GetLayout());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(TonemapPipelineTest, DestructorCleansUp) {
    {
        TonemapPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                 m_samplerLayout->GetLayout());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout
}

// Stage 9.8.a reclaimed binding 2 for the RT AO sampler (fog was retired
// in RP.13b). Walks the SPIR-V OpDecorate stream and asserts that a
// sampled image IS declared at (set 0, binding 2) — a regression that
// removed the binding would silently degrade Hybrid RT to XeGTAO AO with
// no compile-time signal.
TEST_F(TonemapPipelineTest, FragmentShaderDeclaresRtAoBindingTwo) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/tonemap.frag.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool foundBinding2 = false;
    size_t idx = 5;
    while (idx < wordCount) {
        uint32_t word = w[idx];
        uint32_t opcode = word & 0xFFFFU;
        uint32_t len = (word >> 16U) & 0xFFFFU;
        if (len == 0 || idx + len > wordCount) {
            break;
        }
        if (opcode == 71 && len >= 4) {
            uint32_t decoration = w[idx + 2];
            if (decoration == 33 && w[idx + 3] == 2U) {
                foundBinding2 = true;
            }
        }
        idx += len;
    }
    EXPECT_TRUE(foundBinding2)
        << "tonemap.frag SPIR-V missing binding 2 (RT AO source, Stage 9.8.a)";
}

// RP.13a — tonemap.frag must NOT declare a binding-3 sampler after SSIL
// retirement. RP.13b then retired binding 2 (fog), so nothing should
// declare binding 3 either. Walks the SPIR-V OpDecorate stream and
// asserts no sampled image exists at (set 0, binding 3).
TEST_F(TonemapPipelineTest, FragmentShaderDoesNotDeclareBindingThree) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    std::ifstream file(shaderDir + "/tonemap.frag.spv", std::ios::binary);
    ASSERT_TRUE(file.good());
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    ASSERT_GE(bytes.size(), 5U * sizeof(uint32_t));
    const uint32_t* w = reinterpret_cast<const uint32_t*>(bytes.data());
    const size_t wordCount = bytes.size() / sizeof(uint32_t);

    bool foundBinding3 = false;
    size_t idx = 5;
    while (idx < wordCount) {
        uint32_t word = w[idx];
        uint32_t opcode = word & 0xFFFFU;
        uint32_t len = (word >> 16U) & 0xFFFFU;
        if (len == 0 || idx + len > wordCount) {
            break;
        }
        if (opcode == 71 && len >= 4) {
            uint32_t decoration = w[idx + 2];
            if (decoration == 33 && w[idx + 3] == 3U) {
                foundBinding3 = true;
            }
        }
        idx += len;
    }
    EXPECT_FALSE(foundBinding3)
        << "tonemap.frag SPIR-V still declares binding 3 (SSIL retired in RP.13a)";
}

// Stage 9.8.a push-constant contract between tonemap.frag and the CPU
// struct. `float exposure` at offset 0 (kept from RP.13b), `float useRtAo`
// at offset 4 (added by Stage 9.8.a to select between XeGTAO AO at binding
// 1 and RT AO at binding 2). 8 bytes total — RP.13b had this at 4.
TEST(TonemapPushConstantsTest, SizeIsEightBytes) {
    EXPECT_EQ(sizeof(TonemapPipeline::PushConstants), 8U);
}

TEST(TonemapPushConstantsTest, FieldOffsetsMatchShaderLayout) {
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, exposure), 0U);
    EXPECT_EQ(offsetof(TonemapPipeline::PushConstants, useRtAo), 4U);
}

// RP.12a — tonemap.frag must NOT declare a binding-4 sampler after bloom
// retirement. Walks the SPIR-V OpDecorate stream and asserts no sampled
// image exists at (set 0, binding 4); a regression that re-added the
// bloom binding would let a 5-binding shader link against the 4-binding
// CPU layout and only trip at validation time.
TEST_F(TonemapPipelineTest, FragmentShaderDoesNotDeclareBindingFour) {
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
    EXPECT_FALSE(foundBinding4)
        << "tonemap.frag SPIR-V still declares binding 4 (bloom retired in RP.12a)";
}
