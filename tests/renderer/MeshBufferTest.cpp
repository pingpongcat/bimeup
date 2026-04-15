#include <gtest/gtest.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Device;
using bimeup::renderer::MeshBuffer;
using bimeup::renderer::MeshData;
using bimeup::renderer::MeshHandle;
using bimeup::renderer::Vertex;
using bimeup::renderer::VulkanContext;

class MeshBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_meshBuffer.reset();
        m_device.reset();
        m_context.reset();
    }

    static MeshData MakeTriangle() {
        MeshData data;
        data.vertices = {
            {{0.0F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F, 1.0F}},
            {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}},
            {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}},
        };
        data.indices = {0, 1, 2};
        return data;
    }

    static MeshData MakeQuad() {
        MeshData data;
        data.vertices = {
            {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
            {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
            {{0.5F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
            {{-0.5F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
        };
        data.indices = {0, 1, 2, 2, 3, 0};
        return data;
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<MeshBuffer> m_meshBuffer;
};

TEST_F(MeshBufferTest, UploadMeshReturnsValidHandle) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle handle = m_meshBuffer->Upload(MakeTriangle());
    EXPECT_NE(handle, MeshBuffer::InvalidHandle);
}

TEST_F(MeshBufferTest, UploadMultipleMeshesReturnsDifferentHandles) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    MeshHandle h2 = m_meshBuffer->Upload(MakeQuad());
    EXPECT_NE(h1, h2);
}

TEST_F(MeshBufferTest, GetDrawParamsReturnsCorrectCounts) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle handle = m_meshBuffer->Upload(MakeTriangle());

    auto params = m_meshBuffer->GetDrawParams(handle);
    EXPECT_EQ(params.indexCount, 3);
    EXPECT_EQ(params.firstIndex, 0);
    EXPECT_EQ(params.vertexOffset, 0);
}

TEST_F(MeshBufferTest, SecondMeshDrawParamsOffsetCorrectly) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    MeshHandle h2 = m_meshBuffer->Upload(MakeQuad());

    auto p1 = m_meshBuffer->GetDrawParams(h1);
    auto p2 = m_meshBuffer->GetDrawParams(h2);

    EXPECT_EQ(p1.indexCount, 3);
    EXPECT_EQ(p1.firstIndex, 0);
    EXPECT_EQ(p1.vertexOffset, 0);

    EXPECT_EQ(p2.indexCount, 6);
    EXPECT_EQ(p2.firstIndex, 3);  // after triangle's 3 indices
    EXPECT_EQ(p2.vertexOffset, 3);  // after triangle's 3 vertices
}

TEST_F(MeshBufferTest, RemoveMeshMakesHandleInvalid) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle handle = m_meshBuffer->Upload(MakeTriangle());
    m_meshBuffer->Remove(handle);
    EXPECT_FALSE(m_meshBuffer->IsValid(handle));
}

TEST_F(MeshBufferTest, SetVertexColorOverrideTintsGivenIndices) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    MeshHandle h2 = m_meshBuffer->Upload(MakeQuad());
    (void)h1;
    (void)h2;

    // Override vertex indices belonging to the quad (global indices 3..6) with yellow.
    glm::vec4 tint(1.0F, 1.0F, 0.0F, 1.0F);
    m_meshBuffer->SetVertexColorOverride({3, 4, 5, 6}, tint);

    const auto& verts = m_meshBuffer->GetVerticesForTesting();
    ASSERT_EQ(verts.size(), 7u);
    // Triangle (indices 0..2) remains at its original colors (red, green, blue).
    EXPECT_EQ(verts[0].color, glm::vec4(1.0F, 0.0F, 0.0F, 1.0F));
    EXPECT_EQ(verts[1].color, glm::vec4(0.0F, 1.0F, 0.0F, 1.0F));
    EXPECT_EQ(verts[2].color, glm::vec4(0.0F, 0.0F, 1.0F, 1.0F));
    // Quad vertices are tinted.
    for (uint32_t i = 3; i <= 6; ++i) {
        EXPECT_EQ(verts[i].color, tint) << "vertex " << i;
    }
}

TEST_F(MeshBufferTest, SetVertexColorOverrideRestoresPreviousOverride) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    m_meshBuffer->Upload(MakeTriangle());

    m_meshBuffer->SetVertexColorOverride({0, 1, 2}, glm::vec4(1.0F, 1.0F, 0.0F, 1.0F));
    m_meshBuffer->SetVertexColorOverride({}, glm::vec4(0.0F));

    const auto& verts = m_meshBuffer->GetVerticesForTesting();
    EXPECT_EQ(verts[0].color, glm::vec4(1.0F, 0.0F, 0.0F, 1.0F));
    EXPECT_EQ(verts[1].color, glm::vec4(0.0F, 1.0F, 0.0F, 1.0F));
    EXPECT_EQ(verts[2].color, glm::vec4(0.0F, 0.0F, 1.0F, 1.0F));
}

TEST_F(MeshBufferTest, MeshCountTracksUploadAndRemove) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    EXPECT_EQ(m_meshBuffer->MeshCount(), 0);

    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    EXPECT_EQ(m_meshBuffer->MeshCount(), 1);

    m_meshBuffer->Upload(MakeQuad());
    EXPECT_EQ(m_meshBuffer->MeshCount(), 2);

    m_meshBuffer->Remove(h1);
    EXPECT_EQ(m_meshBuffer->MeshCount(), 1);
}

// ----- 7.8d.4 Per-vertex alpha override -----------------------------------

TEST_F(MeshBufferTest, SetVertexAlphaOverridePreservesBaselineRGB) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    m_meshBuffer->Upload(MakeTriangle());

    m_meshBuffer->SetVertexAlphaOverride({{0, 0.25F}, {1, 0.5F}});

    const auto& verts = m_meshBuffer->GetVerticesForTesting();
    // RGB unchanged from baseline.
    EXPECT_EQ(glm::vec3(verts[0].color), glm::vec3(1.0F, 0.0F, 0.0F));
    EXPECT_EQ(glm::vec3(verts[1].color), glm::vec3(0.0F, 1.0F, 0.0F));
    EXPECT_EQ(glm::vec3(verts[2].color), glm::vec3(0.0F, 0.0F, 1.0F));
    // Alpha replaced for listed indices; baseline 1.0 for the rest.
    EXPECT_FLOAT_EQ(verts[0].color.a, 0.25F);
    EXPECT_FLOAT_EQ(verts[1].color.a, 0.5F);
    EXPECT_FLOAT_EQ(verts[2].color.a, 1.0F);
}

TEST_F(MeshBufferTest, SetVertexAlphaOverrideEmptyRestoresBaselineAlpha) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    m_meshBuffer->Upload(MakeTriangle());

    m_meshBuffer->SetVertexAlphaOverride({{0, 0.1F}, {2, 0.2F}});
    m_meshBuffer->SetVertexAlphaOverride({});

    const auto& verts = m_meshBuffer->GetVerticesForTesting();
    EXPECT_FLOAT_EQ(verts[0].color.a, 1.0F);
    EXPECT_FLOAT_EQ(verts[1].color.a, 1.0F);
    EXPECT_FLOAT_EQ(verts[2].color.a, 1.0F);
}

TEST_F(MeshBufferTest, ColorOverrideWinsOverAlphaOverrideForSameIndex) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    m_meshBuffer->Upload(MakeTriangle());

    m_meshBuffer->SetVertexAlphaOverride({{0, 0.3F}, {1, 0.3F}, {2, 0.3F}});
    glm::vec4 tint(1.0F, 1.0F, 0.0F, 1.0F);
    m_meshBuffer->SetVertexColorOverride({1}, tint);

    const auto& verts = m_meshBuffer->GetVerticesForTesting();
    // Vertex 1 replaced wholesale by color override.
    EXPECT_EQ(verts[1].color, tint);
    // Vertices 0 and 2 still show alpha override on baseline RGB.
    EXPECT_EQ(glm::vec3(verts[0].color), glm::vec3(1.0F, 0.0F, 0.0F));
    EXPECT_FLOAT_EQ(verts[0].color.a, 0.3F);
    EXPECT_EQ(glm::vec3(verts[2].color), glm::vec3(0.0F, 0.0F, 1.0F));
    EXPECT_FLOAT_EQ(verts[2].color.a, 0.3F);
}

TEST_F(MeshBufferTest, SetVertexAlphaOverrideAfterColorOverrideClearsColorLayerAlpha) {
    // Layer order: baseline → alpha override → color override (full RGBA replace).
    // Setting alpha after color should recompute from baseline — color override stays.
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    m_meshBuffer->Upload(MakeTriangle());

    glm::vec4 tint(1.0F, 1.0F, 0.0F, 1.0F);
    m_meshBuffer->SetVertexColorOverride({0}, tint);
    m_meshBuffer->SetVertexAlphaOverride({{2, 0.5F}});

    const auto& verts = m_meshBuffer->GetVerticesForTesting();
    EXPECT_EQ(verts[0].color, tint) << "color override layer preserved";
    EXPECT_FLOAT_EQ(verts[2].color.a, 0.5F);
}
