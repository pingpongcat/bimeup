#include <gtest/gtest.h>
#include <renderer/Shader.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <cstdio>
#include <fstream>

using bimeup::renderer::Device;
using bimeup::renderer::Shader;
using bimeup::renderer::ShaderStage;
using bimeup::renderer::VulkanContext;

namespace {

// Minimal valid SPIR-V vertex shader (void main entry point)
std::vector<uint32_t> MakeMinimalVertexSpirv() {
    return {
        // Header
        0x07230203,  // magic
        0x00010000,  // version 1.0
        0x00000000,  // generator
        0x00000005,  // bound
        0x00000000,  // reserved
        // OpCapability Shader
        0x00020011, 0x00000001,
        // OpMemoryModel Logical GLSL450
        0x0003000E, 0x00000000, 0x00000001,
        // OpEntryPoint Vertex %1 "main"
        0x0005000F, 0x00000000, 0x00000001, 0x6E69616D, 0x00000000,
        // %2 = OpTypeVoid
        0x00020013, 0x00000002,
        // %3 = OpTypeFunction %2
        0x00030021, 0x00000003, 0x00000002,
        // %1 = OpFunction %2 None %3
        0x00050036, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        // %4 = OpLabel
        0x000200F8, 0x00000004,
        // OpReturn
        0x000100FD,
        // OpFunctionEnd
        0x00010038,
    };
}

// Minimal valid SPIR-V fragment shader (void main, OriginUpperLeft)
std::vector<uint32_t> MakeMinimalFragmentSpirv() {
    return {
        // Header
        0x07230203,  // magic
        0x00010000,  // version 1.0
        0x00000000,  // generator
        0x00000005,  // bound
        0x00000000,  // reserved
        // OpCapability Shader
        0x00020011, 0x00000001,
        // OpMemoryModel Logical GLSL450
        0x0003000E, 0x00000000, 0x00000001,
        // OpEntryPoint Fragment %1 "main"
        0x0005000F, 0x00000004, 0x00000001, 0x6E69616D, 0x00000000,
        // OpExecutionMode %1 OriginUpperLeft
        0x00030010, 0x00000001, 0x00000007,
        // %2 = OpTypeVoid
        0x00020013, 0x00000002,
        // %3 = OpTypeFunction %2
        0x00030021, 0x00000003, 0x00000002,
        // %1 = OpFunction %2 None %3
        0x00050036, 0x00000002, 0x00000001, 0x00000000, 0x00000003,
        // %4 = OpLabel
        0x000200F8, 0x00000004,
        // OpReturn
        0x000100FD,
        // OpFunctionEnd
        0x00010038,
    };
}

}  // namespace

class ShaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_shader.reset();
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<Shader> m_shader;
};

TEST_F(ShaderTest, CreateVertexShaderFromSpirv) {
    auto spirv = MakeMinimalVertexSpirv();

    m_shader = std::make_unique<Shader>(*m_device, ShaderStage::Vertex, spirv);

    EXPECT_NE(m_shader->GetModule(), VK_NULL_HANDLE);
    EXPECT_EQ(m_shader->GetStage(), ShaderStage::Vertex);
}

TEST_F(ShaderTest, CreateFragmentShaderFromSpirv) {
    auto spirv = MakeMinimalFragmentSpirv();

    m_shader = std::make_unique<Shader>(*m_device, ShaderStage::Fragment, spirv);

    EXPECT_NE(m_shader->GetModule(), VK_NULL_HANDLE);
    EXPECT_EQ(m_shader->GetStage(), ShaderStage::Fragment);
}

TEST_F(ShaderTest, GetStageInfoVertex) {
    auto spirv = MakeMinimalVertexSpirv();
    m_shader = std::make_unique<Shader>(*m_device, ShaderStage::Vertex, spirv);

    VkPipelineShaderStageCreateInfo info = m_shader->GetStageInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_VERTEX_BIT);
    EXPECT_EQ(info.module, m_shader->GetModule());
    EXPECT_STREQ(info.pName, "main");
}

TEST_F(ShaderTest, GetStageInfoFragment) {
    auto spirv = MakeMinimalFragmentSpirv();
    m_shader = std::make_unique<Shader>(*m_device, ShaderStage::Fragment, spirv);

    VkPipelineShaderStageCreateInfo info = m_shader->GetStageInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_EQ(info.module, m_shader->GetModule());
    EXPECT_STREQ(info.pName, "main");
}

TEST_F(ShaderTest, LoadFromFile) {
    auto spirv = MakeMinimalVertexSpirv();

    // Write SPIR-V to a temp file
    std::string tmpPath = "/tmp/bimeup_test_shader.spv";
    {
        std::ofstream out(tmpPath, std::ios::binary);
        ASSERT_TRUE(out.is_open());
        out.write(reinterpret_cast<const char*>(spirv.data()),
                  static_cast<std::streamsize>(spirv.size() * sizeof(uint32_t)));
    }

    m_shader = std::make_unique<Shader>(*m_device, ShaderStage::Vertex, tmpPath);

    EXPECT_NE(m_shader->GetModule(), VK_NULL_HANDLE);
    EXPECT_EQ(m_shader->GetStage(), ShaderStage::Vertex);

    std::remove(tmpPath.c_str());
}

TEST_F(ShaderTest, LoadFromNonexistentFileThrows) {
    EXPECT_THROW(
        Shader(*m_device, ShaderStage::Vertex, std::string("/tmp/nonexistent_shader.spv")),
        std::runtime_error);
}

TEST_F(ShaderTest, EmptySpirvThrows) {
    std::vector<uint32_t> empty;

    EXPECT_THROW(
        Shader(*m_device, ShaderStage::Vertex, empty),
        std::runtime_error);
}

TEST_F(ShaderTest, DestructorCleansUp) {
    auto spirv = MakeMinimalVertexSpirv();
    {
        Shader shader(*m_device, ShaderStage::Vertex, spirv);
        EXPECT_NE(shader.GetModule(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked shader modules
}
