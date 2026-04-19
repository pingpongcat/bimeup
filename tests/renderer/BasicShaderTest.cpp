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
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
    }

    static void TearDownTestSuite() {
        s_device.reset();
        s_context.reset();
    }

    void SetUp() override { m_device = s_device.get(); }

    Device* m_device = nullptr;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
};

std::unique_ptr<VulkanContext> BasicShaderTest::s_context;
std::unique_ptr<Device> BasicShaderTest::s_device;

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
// second MRT attachment gets populated for SSAO.
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
    // RP.6c (slimmed in RP.15.b) contract: basic.frag emits a third output
    // — the transparency stencil bit (R8_UINT, 0 for opaque / 4 for
    // transparent per the RP.12b transparentBit push). Kept in the same
    // shader-output test so an accidental `out` removal fails one test,
    // not three.
    EXPECT_TRUE(hasLoc2) << "basic.frag missing transparency-stencil output at location 2";
}

TEST_F(BasicShaderTest, FragmentShaderStageInfoCorrect) {
    std::string path = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    Shader fragShader(*m_device, ShaderStage::Fragment, path);

    VkPipelineShaderStageCreateInfo info = fragShader.GetStageInfo();

    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    EXPECT_EQ(info.stage, VK_SHADER_STAGE_FRAGMENT_BIT);
    EXPECT_STREQ(info.pName, "main");
}

// RP.15.b contract: basic.frag declares a fragment-stage push constant block
// containing exactly one 32-bit unsigned int member (`transparentBit`) at
// byte offset 64 — the byte after the 64-byte vertex-stage model matrix push
// range. Walk the SPIR-V module: find the OpVariable in the PushConstant
// storage class, follow its OpTypePointer → OpTypeStruct, assert the struct
// has exactly one member, that member is a 32-bit OpTypeInt with
// signedness=0, and an OpMemberDecorate Offset=64 is attached at member
// index 0. The "exactly one member" clause catches a resurrection of the
// retired `stencilId` member (RP.6d) alongside `transparentBit` (RP.12b).
TEST_F(BasicShaderTest, FragmentShaderDeclaresTransparentBitPushConstant) {
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
    // assert exactly one has a single 32-bit unsigned int member at Offset=64.
    // "Exactly one member" catches a resurrection of the retired `stencilId`
    // member — the post-RP.15.b block is `{ uint transparentBit @ 64 }`, not
    // `{ uint stencilId @ 64, uint transparentBit @ 68 }`.
    bool found = false;
    for (uint32_t pointerTypeId : pushConstantVarTypeIds) {
        auto pIt = pointers.find(pointerTypeId);
        if (pIt == pointers.end()) continue;
        auto sIt = structs.find(pIt->second.typeId);
        if (sIt == structs.end()) continue;
        if (sIt->second.memberTypes.size() != 1U) continue;
        auto offIt = memberOffsets.find(pIt->second.typeId);
        if (offIt == memberOffsets.end()) continue;
        auto memberOffsetIt = offIt->second.find(0U);
        if (memberOffsetIt == offIt->second.end()) continue;
        if (memberOffsetIt->second != 64U) continue;
        auto intIt = ints.find(sIt->second.memberTypes[0]);
        if (intIt == ints.end()) continue;
        if (intIt->second.width == 32U && intIt->second.signedness == 0U) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "basic.frag missing fragment push constant `uint transparentBit` at "
                          "offset 64 (exactly one 32-bit uint member expected)";
}
