#include <gtest/gtest.h>
#include <scene/SceneMesh.h>

using namespace bimeup::scene;

TEST(SceneMeshTest, DefaultConstruction) {
    SceneMesh mesh;
    EXPECT_TRUE(mesh.GetPositions().empty());
    EXPECT_TRUE(mesh.GetNormals().empty());
    EXPECT_TRUE(mesh.GetColors().empty());
    EXPECT_TRUE(mesh.GetIndices().empty());
    EXPECT_EQ(mesh.GetVertexCount(), 0);
    EXPECT_EQ(mesh.GetIndexCount(), 0);
}

TEST(SceneMeshTest, SetAndGetPositions) {
    SceneMesh mesh;
    std::vector<glm::vec3> positions = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    };
    mesh.SetPositions(positions);

    ASSERT_EQ(mesh.GetPositions().size(), 3);
    EXPECT_EQ(mesh.GetPositions()[0], glm::vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(mesh.GetPositions()[1], glm::vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(mesh.GetPositions()[2], glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(mesh.GetVertexCount(), 3);
}

TEST(SceneMeshTest, SetAndGetNormals) {
    SceneMesh mesh;
    std::vector<glm::vec3> normals = {
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
    };
    mesh.SetNormals(normals);

    ASSERT_EQ(mesh.GetNormals().size(), 3);
    EXPECT_EQ(mesh.GetNormals()[0], glm::vec3(0.0f, 0.0f, 1.0f));
}

TEST(SceneMeshTest, SetAndGetColors) {
    SceneMesh mesh;
    std::vector<glm::vec4> colors = {
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
    };
    mesh.SetColors(colors);

    ASSERT_EQ(mesh.GetColors().size(), 3);
    EXPECT_EQ(mesh.GetColors()[0], glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

TEST(SceneMeshTest, SetAndGetIndices) {
    SceneMesh mesh;
    std::vector<uint32_t> indices = {0, 1, 2};
    mesh.SetIndices(indices);

    ASSERT_EQ(mesh.GetIndices().size(), 3);
    EXPECT_EQ(mesh.GetIndices()[0], 0);
    EXPECT_EQ(mesh.GetIndices()[1], 1);
    EXPECT_EQ(mesh.GetIndices()[2], 2);
    EXPECT_EQ(mesh.GetIndexCount(), 3);
}

TEST(SceneMeshTest, GetInterleavedVerticesMatchesRendererLayout) {
    // Renderer expects: vec3 position, vec3 normal, vec4 color per vertex
    // Total: 10 floats per vertex
    SceneMesh mesh;
    mesh.SetPositions({{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}});
    mesh.SetNormals({{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}});
    mesh.SetColors({{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}});

    auto interleaved = mesh.GetInterleavedVertices();
    // 2 vertices * 10 floats = 20 floats
    ASSERT_EQ(interleaved.size(), 20);

    // Vertex 0: position
    EXPECT_FLOAT_EQ(interleaved[0], 1.0f);
    EXPECT_FLOAT_EQ(interleaved[1], 2.0f);
    EXPECT_FLOAT_EQ(interleaved[2], 3.0f);
    // Vertex 0: normal
    EXPECT_FLOAT_EQ(interleaved[3], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[4], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[5], 1.0f);
    // Vertex 0: color
    EXPECT_FLOAT_EQ(interleaved[6], 1.0f);
    EXPECT_FLOAT_EQ(interleaved[7], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[8], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[9], 1.0f);

    // Vertex 1: position
    EXPECT_FLOAT_EQ(interleaved[10], 4.0f);
    EXPECT_FLOAT_EQ(interleaved[11], 5.0f);
    EXPECT_FLOAT_EQ(interleaved[12], 6.0f);
    // Vertex 1: normal
    EXPECT_FLOAT_EQ(interleaved[13], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[14], 1.0f);
    EXPECT_FLOAT_EQ(interleaved[15], 0.0f);
    // Vertex 1: color
    EXPECT_FLOAT_EQ(interleaved[16], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[17], 1.0f);
    EXPECT_FLOAT_EQ(interleaved[18], 0.0f);
    EXPECT_FLOAT_EQ(interleaved[19], 1.0f);
}

TEST(SceneMeshTest, GetInterleavedVerticesEmptyMesh) {
    SceneMesh mesh;
    auto interleaved = mesh.GetInterleavedVertices();
    EXPECT_TRUE(interleaved.empty());
}

TEST(SceneMeshTest, MoveSemantics) {
    SceneMesh mesh;
    mesh.SetPositions({{1.0f, 0.0f, 0.0f}});
    mesh.SetNormals({{0.0f, 0.0f, 1.0f}});
    mesh.SetColors({{1.0f, 1.0f, 1.0f, 1.0f}});
    mesh.SetIndices({0});

    SceneMesh moved(std::move(mesh));
    EXPECT_EQ(moved.GetVertexCount(), 1);
    EXPECT_EQ(moved.GetIndexCount(), 1);
    EXPECT_EQ(moved.GetPositions()[0], glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(SceneMeshTest, SetUniformColor) {
    SceneMesh mesh;
    mesh.SetPositions({{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
    mesh.SetUniformColor({0.8f, 0.2f, 0.1f, 1.0f});

    ASSERT_EQ(mesh.GetColors().size(), 3);
    for (const auto& c : mesh.GetColors()) {
        EXPECT_EQ(c, glm::vec4(0.8f, 0.2f, 0.1f, 1.0f));
    }
}

TEST(SceneMeshTest, VertexStride) {
    // Stride should match renderer::Vertex: vec3 + vec3 + vec4 = 10 floats = 40 bytes
    EXPECT_EQ(SceneMesh::VertexStrideBytes(), 40);
}
