#include <gtest/gtest.h>

#include <renderer/BloomDownPipeline.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using bimeup::renderer::BloomDownPipeline;
using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::LayoutBinding;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

// Bloom downsample writes to an offscreen HDR mip target (R16G16B16A16_SFLOAT
// in practice); the real render pass will be per-mip framebuffers built in
// RP.10c. For the pipeline-creation contract, a single colour attachment
// with LOAD_OP_DONT_CARE is sufficient.
VkRenderPass CreateHdrColorOnlyRenderPass(VkDevice device) {
    VkAttachmentDescription color{};
    color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
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

class BloomDownPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
        m_renderPass = CreateHdrColorOnlyRenderPass(m_device->GetDevice());

        // bloom_down.frag expects at set 0:
        //   binding 0: source HDR colour or previous-mip (COMBINED_IMAGE_SAMPLER).
        // Single binding — downsample is pure colour-to-colour, the prefilter
        // branch is driven entirely by push constants.
        m_layout = std::make_unique<DescriptorSetLayout>(
            *m_device,
            std::vector<LayoutBinding>{
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
            });

        std::string shaderDir = BIMEUP_SHADER_DIR;
        m_vert = std::make_unique<Shader>(*m_device, ShaderStage::Vertex,
                                          shaderDir + "/bloom.vert.spv");
        m_frag = std::make_unique<Shader>(*m_device, ShaderStage::Fragment,
                                          shaderDir + "/bloom_down.frag.spv");
    }

    void TearDown() override {
        m_pipeline.reset();
        m_vert.reset();
        m_frag.reset();
        m_layout.reset();
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device->GetDevice(), m_renderPass, nullptr);
        }
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::unique_ptr<DescriptorSetLayout> m_layout;
    std::unique_ptr<Shader> m_vert;
    std::unique_ptr<Shader> m_frag;
    std::unique_ptr<BloomDownPipeline> m_pipeline;
};

TEST_F(BloomDownPipelineTest, BloomShadersCompiledToSpirv) {
    // The shared fullscreen `bloom.vert` and the downsample frag must both
    // land in the build output — a stale build or glob miss (the shader list
    // is auto-globbed by `cmake/CompileShaders.cmake`) would otherwise surface
    // as a confusing "file not found" from the Shader constructor downstream.
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/bloom.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/bloom_down.frag.spv"));
}

TEST_F(BloomDownPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<BloomDownPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(BloomDownPipelineTest, ConstructsWithMsaa4x) {
    // Bloom mips are single-sampled in practice (the HDR source has already
    // been MSAA-resolved before the bloom chain runs), but keeping the
    // rasterisation-sample count parametrised mirrors Fxaa/OutlinePipeline's
    // shape and future-proofs a direct-MSAA HDR path.
    m_pipeline = std::make_unique<BloomDownPipeline>(
        *m_device, *m_vert, *m_frag, m_renderPass,
        m_layout->GetLayout(), VK_SAMPLE_COUNT_4_BIT);

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
}

TEST_F(BloomDownPipelineTest, DestructorCleansUp) {
    {
        BloomDownPipeline pipeline(*m_device, *m_vert, *m_frag, m_renderPass,
                                   m_layout->GetLayout(), VK_SAMPLE_COUNT_1_BIT);
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout.
}

TEST(BloomDownPipelinePushConstants, SizeIsTwentyBytes) {
    // vec2 rcpSrcSize (8) + float threshold (4) + float knee (4) +
    // int applyPrefilter (4) = 20 bytes. Well under the 128-byte Vulkan
    // minimum push-constant guarantee. `applyPrefilter` is an int32 so the
    // shader can branch between the "HDR → mip0" (prefilter on) and
    // "mip_n → mip_{n+1}" (prefilter off) cases without a pipeline rebuild.
    EXPECT_EQ(sizeof(BloomDownPipeline::PushConstants), 20U);
}

TEST(BloomDownPipelinePushConstants, FieldOffsetsMatchShaderLayout) {
    // Vulkan push_constant block layout: scalars 4-aligned, vec2 8-aligned.
    // Pins the exact byte offsets so a field reorder that still totals 20
    // bytes (e.g. swapping threshold and knee) gets caught here, not at
    // validation time or as a visual regression in the bloom halo shape.
    BloomDownPipeline::PushConstants pc{};
    const auto base = reinterpret_cast<const std::byte*>(&pc);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.rcpSrcSize) - base, 0);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.threshold) - base, 8);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.knee) - base, 12);
    EXPECT_EQ(reinterpret_cast<const std::byte*>(&pc.applyPrefilter) - base, 16);
}
