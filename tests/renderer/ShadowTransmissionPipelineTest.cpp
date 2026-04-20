#include <gtest/gtest.h>

#include <renderer/Device.h>
#include <renderer/Shader.h>
#include <renderer/ShadowPass.h>
#include <renderer/ShadowTransmissionPipeline.h>
#include <renderer/VulkanContext.h>

#include <filesystem>
#include <memory>
#include <string>

using bimeup::renderer::Device;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::ShadowMap;
using bimeup::renderer::ShadowTransmissionPipeline;
using bimeup::renderer::VulkanContext;

class ShadowTransmissionPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
        // Use a real ShadowMap so the test pins the pipeline against the same
        // 2-attachment render pass that main.cpp will bind it to.
        s_shadowMap = std::make_unique<ShadowMap>(*s_device, 512U);

        std::string shaderDir = BIMEUP_SHADER_DIR;
        s_vert = std::make_unique<Shader>(*s_device, ShaderStage::Vertex,
                                          shaderDir + "/shadow_transmission.vert.spv");
        s_frag = std::make_unique<Shader>(*s_device, ShaderStage::Fragment,
                                          shaderDir + "/shadow_transmission.frag.spv");
    }

    static void TearDownTestSuite() {
        s_vert.reset();
        s_frag.reset();
        s_shadowMap.reset();
        s_device.reset();
        s_context.reset();
    }

    void SetUp() override {
        m_device = s_device.get();
        m_shadowMap = s_shadowMap.get();
        m_vert = s_vert.get();
        m_frag = s_frag.get();
    }
    void TearDown() override { m_pipeline.reset(); }

    Device* m_device = nullptr;
    ShadowMap* m_shadowMap = nullptr;
    Shader* m_vert = nullptr;
    Shader* m_frag = nullptr;
    std::unique_ptr<ShadowTransmissionPipeline> m_pipeline;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static std::unique_ptr<ShadowMap> s_shadowMap;
    static std::unique_ptr<Shader> s_vert;
    static std::unique_ptr<Shader> s_frag;
};

std::unique_ptr<VulkanContext> ShadowTransmissionPipelineTest::s_context;
std::unique_ptr<Device> ShadowTransmissionPipelineTest::s_device;
std::unique_ptr<ShadowMap> ShadowTransmissionPipelineTest::s_shadowMap;
std::unique_ptr<Shader> ShadowTransmissionPipelineTest::s_vert;
std::unique_ptr<Shader> ShadowTransmissionPipelineTest::s_frag;

TEST_F(ShadowTransmissionPipelineTest, ShadersCompiledToSpirv) {
    std::string shaderDir = BIMEUP_SHADER_DIR;
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/shadow_transmission.vert.spv"));
    EXPECT_TRUE(std::filesystem::exists(shaderDir + "/shadow_transmission.frag.spv"));
}

TEST_F(ShadowTransmissionPipelineTest, ConstructsWithValidHandles) {
    m_pipeline = std::make_unique<ShadowTransmissionPipeline>(
        *m_device, *m_vert, *m_frag, m_shadowMap->GetRenderPass());

    EXPECT_NE(m_pipeline->GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(m_pipeline->GetLayout(), VK_NULL_HANDLE);
}

TEST_F(ShadowTransmissionPipelineTest, DeclaresMinBlend) {
    // Pipeline construction under validation layers is the only external witness
    // that the blend state is valid for the R16G16B16A16_SFLOAT transmission
    // attachment; VK_BLEND_OP_MIN is core 1.0 and requires no format feature
    // beyond COLOR_ATTACHMENT_BIT, so building against the ShadowMap's pass
    // exercises the full contract (min-blend + depth-test-only + LESS compare).
    m_pipeline = std::make_unique<ShadowTransmissionPipeline>(
        *m_device, *m_vert, *m_frag, m_shadowMap->GetRenderPass());

    EXPECT_TRUE(m_pipeline->UsesMinBlend());
}

TEST_F(ShadowTransmissionPipelineTest, DestructorCleansUp) {
    {
        ShadowTransmissionPipeline pipeline(*m_device, *m_vert, *m_frag,
                                            m_shadowMap->GetRenderPass());
        EXPECT_NE(pipeline.GetPipeline(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked pipeline/layout
}
