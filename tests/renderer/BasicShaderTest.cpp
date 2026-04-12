#include <gtest/gtest.h>
#include <renderer/Shader.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <filesystem>

using bimeup::renderer::Device;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

class BasicShaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
};

TEST_F(BasicShaderTest, VertexShaderCompiledToValidSpirv) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.vert.spv";
    ASSERT_TRUE(std::filesystem::exists(path)) << "Compiled vertex shader not found: " << path;

    Shader vertShader(*m_device, ShaderStage::Vertex, path);

    EXPECT_NE(vertShader.GetModule(), VK_NULL_HANDLE);
    EXPECT_EQ(vertShader.GetStage(), ShaderStage::Vertex);
}

TEST_F(BasicShaderTest, FragmentShaderCompiledToValidSpirv) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    ASSERT_TRUE(std::filesystem::exists(path)) << "Compiled fragment shader not found: " << path;

    Shader fragShader(*m_device, ShaderStage::Fragment, path);

    EXPECT_NE(fragShader.GetModule(), VK_NULL_HANDLE);
    EXPECT_EQ(fragShader.GetStage(), ShaderStage::Fragment);
}

TEST_F(BasicShaderTest, VertexShaderStageInfoCorrect) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.vert.spv";
    Shader vertShader(*m_device, ShaderStage::Vertex, path);

    VkPipelineShaderStageCreateInfo info = vertShader.GetStageInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_STREQ(info.pName, "main");
}

TEST_F(BasicShaderTest, FragmentShaderStageInfoCorrect) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    Shader fragShader(*m_device, ShaderStage::Fragment, path);

    VkPipelineShaderStageCreateInfo info = fragShader.GetStageInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_STREQ(info.pName, "main");
}
