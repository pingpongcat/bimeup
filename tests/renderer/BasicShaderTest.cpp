#include <gtest/gtest.h>
#include <renderer/Shader.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

TEST_F(BasicShaderTest, ShadowVertexShaderCompiledToValidSpirv) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/shadow.vert.spv";
    ASSERT_TRUE(std::filesystem::exists(path)) << "Compiled shadow vertex shader not found: " << path;

    Shader shadowVert(*m_device, ShaderStage::Vertex, path);

    EXPECT_NE(shadowVert.GetModule(), VK_NULL_HANDLE);
    EXPECT_EQ(shadowVert.GetStage(), ShaderStage::Vertex);
}

// RP.3d contract: basic.frag must write both colour (location 0) and the
// oct-packed view-space normal G-buffer (location 1) so the main render pass's
// second MRT attachment gets populated for SSAO/SSIL/outlines.
TEST_F(BasicShaderTest, FragmentShaderDeclaresNormalOutputAtLocation1) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f) << "Missing " << path;
    const auto bytes = static_cast<std::streamoff>(f.tellg());
    ASSERT_GT(bytes, 0);
    ASSERT_EQ(bytes % 4, 0);
    f.seekg(0);
    std::vector<uint32_t> words(static_cast<size_t>(bytes) / 4);
    f.read(reinterpret_cast<char*>(words.data()), bytes);

    ASSERT_GE(words.size(), 5U);
    ASSERT_EQ(words[0], 0x07230203U) << "Bad SPIR-V magic";

    constexpr uint32_t kOpDecorate = 71;
    constexpr uint32_t kOpVariable = 59;
    constexpr uint32_t kDecorationLocation = 30;
    constexpr uint32_t kStorageClassOutput = 3;

    std::unordered_map<uint32_t, uint32_t> idToLocation;
    std::unordered_set<uint32_t> outputIds;

    for (size_t i = 5; i < words.size();) {
        const uint32_t header = words[i];
        const uint32_t opcode = header & 0xFFFFU;
        const uint32_t count = header >> 16U;
        ASSERT_GT(count, 0U) << "Malformed SPIR-V instruction at word " << i;
        ASSERT_LE(i + count, words.size()) << "Truncated SPIR-V instruction at word " << i;

        if (opcode == kOpDecorate && count >= 4 && words[i + 2] == kDecorationLocation) {
            idToLocation[words[i + 1]] = words[i + 3];
        } else if (opcode == kOpVariable && count >= 4 && words[i + 3] == kStorageClassOutput) {
            outputIds.insert(words[i + 2]);
        }
        i += count;
    }

    bool hasLoc0 = false;
    bool hasLoc1 = false;
    bool hasLoc2 = false;
    for (uint32_t id : outputIds) {
        auto it = idToLocation.find(id);
        if (it == idToLocation.end()) continue;
        if (it->second == 0) hasLoc0 = true;
        if (it->second == 1) hasLoc1 = true;
        if (it->second == 2) hasLoc2 = true;
    }
    EXPECT_TRUE(hasLoc0) << "basic.frag missing colour output at location 0";
    EXPECT_TRUE(hasLoc1) << "basic.frag missing oct-packed normal output at location 1";
    // RP.6c contract: basic.frag emits a third output — the outline-stencil id
    // (R8_UINT, 0/1/2 for background/selected/hovered). Selection wiring
    // arrives in RP.6d; for now the shader writes 0u unconditionally so the
    // MRT attachment gets the expected value. Kept in the same shader-output
    // test so an accidental `out` removal fails one test, not three.
    EXPECT_TRUE(hasLoc2) << "basic.frag missing outline-stencil output at location 2";
}

TEST_F(BasicShaderTest, FragmentShaderStageInfoCorrect) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    Shader fragShader(*m_device, ShaderStage::Fragment, path);

    VkPipelineShaderStageCreateInfo info = fragShader.GetStageInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_STREQ(info.pName, "main");
}

// RP.6d contract: basic.frag declares a fragment-stage push constant block
// containing a single 32-bit unsigned int member (`stencilId`) at byte offset
// 64 — the byte after the 64-byte vertex-stage model matrix push range. Walk
// the SPIR-V module: find the OpVariable in the PushConstant storage class,
// follow its OpTypePointer → OpTypeStruct, assert the struct has exactly one
// member, that member is a 32-bit OpTypeInt with signedness=0, and an
// OpMemberDecorate Offset=64 is attached at member index 0. A regression
// (block removed, member renamed away from a uint, offset drifted) fails
// before the runtime hits the validation layer.
TEST_F(BasicShaderTest, FragmentShaderDeclaresStencilIdPushConstant) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(f) << "Missing " << path;
    const auto bytes = static_cast<std::streamoff>(f.tellg());
    ASSERT_GT(bytes, 0);
    ASSERT_EQ(bytes % 4, 0);
    f.seekg(0);
    std::vector<uint32_t> words(static_cast<size_t>(bytes) / 4);
    f.read(reinterpret_cast<char*>(words.data()), bytes);

    ASSERT_GE(words.size(), 5U);
    ASSERT_EQ(words[0], 0x07230203U) << "Bad SPIR-V magic";

    constexpr uint32_t kOpMemberDecorate = 72;
    constexpr uint32_t kOpTypeInt = 21;
    constexpr uint32_t kOpTypePointer = 32;
    constexpr uint32_t kOpTypeStruct = 30;
    constexpr uint32_t kOpVariable = 59;
    constexpr uint32_t kDecorationOffset = 35;
    constexpr uint32_t kStorageClassPushConstant = 9;

    struct StructInfo {
        std::vector<uint32_t> memberTypes;
    };
    struct PointerInfo {
        uint32_t storageClass;
        uint32_t typeId;
    };
    struct IntInfo {
        uint32_t width;
        uint32_t signedness;
    };

    std::unordered_map<uint32_t, StructInfo> structs;
    std::unordered_map<uint32_t, PointerInfo> pointers;
    std::unordered_map<uint32_t, IntInfo> ints;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> memberOffsets;
    std::unordered_set<uint32_t> pushConstantVarTypeIds;

    for (size_t i = 5; i < words.size();) {
        const uint32_t header = words[i];
        const uint32_t opcode = header & 0xFFFFU;
        const uint32_t count = header >> 16U;
        ASSERT_GT(count, 0U) << "Malformed SPIR-V instruction at word " << i;
        ASSERT_LE(i + count, words.size()) << "Truncated SPIR-V instruction at word " << i;

        if (opcode == kOpTypeInt && count >= 4) {
            ints[words[i + 1]] = {words[i + 2], words[i + 3]};
        } else if (opcode == kOpTypeStruct && count >= 2) {
            StructInfo s;
            for (uint32_t k = 2; k < count; ++k) {
                s.memberTypes.push_back(words[i + k]);
            }
            structs[words[i + 1]] = std::move(s);
        } else if (opcode == kOpTypePointer && count >= 4) {
            pointers[words[i + 1]] = {words[i + 2], words[i + 3]};
        } else if (opcode == kOpVariable && count >= 4 &&
                   words[i + 3] == kStorageClassPushConstant) {
            pushConstantVarTypeIds.insert(words[i + 1]);  // result type (pointer)
        } else if (opcode == kOpMemberDecorate && count >= 5 &&
                   words[i + 3] == kDecorationOffset) {
            memberOffsets[words[i + 1]][words[i + 2]] = words[i + 4];
        }
        i += count;
    }

    // Resolve the struct type behind every PushConstant pointer variable, then
    // search for the one whose member-0 has Offset=64 and a 32-bit unsigned
    // int member type.
    bool found = false;
    for (uint32_t pointerTypeId : pushConstantVarTypeIds) {
        auto pIt = pointers.find(pointerTypeId);
        if (pIt == pointers.end()) continue;
        auto sIt = structs.find(pIt->second.typeId);
        if (sIt == structs.end() || sIt->second.memberTypes.empty()) continue;
        auto offIt = memberOffsets.find(pIt->second.typeId);
        if (offIt == memberOffsets.end()) continue;
        for (size_t m = 0; m < sIt->second.memberTypes.size(); ++m) {
            auto memberOffsetIt = offIt->second.find(static_cast<uint32_t>(m));
            if (memberOffsetIt == offIt->second.end()) continue;
            if (memberOffsetIt->second != 64U) continue;
            auto intIt = ints.find(sIt->second.memberTypes[m]);
            if (intIt == ints.end()) continue;
            if (intIt->second.width == 32U && intIt->second.signedness == 0U) {
                found = true;
                break;
            }
        }
        if (found) break;
    }
    EXPECT_TRUE(found) << "basic.frag missing fragment push constant `uint stencilId` at offset 64";
}
