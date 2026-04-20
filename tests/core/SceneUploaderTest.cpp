#include <gtest/gtest.h>
#include <core/SceneUploader.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/VulkanContext.h>
#include <scene/Scene.h>
#include <scene/SceneBuilder.h>
#include <scene/SceneMesh.h>

using bimeup::core::SceneUploader;
using bimeup::renderer::Device;
using bimeup::renderer::MeshBuffer;
using bimeup::renderer::MeshData;
using bimeup::renderer::Vertex;
using bimeup::renderer::VulkanContext;
using bimeup::scene::BuildResult;
using bimeup::scene::Scene;
using bimeup::scene::SceneMesh;
using bimeup::scene::SceneNode;

// --- Pure conversion tests (no Vulkan needed) ---

TEST(SceneUploaderTest, ToMeshDataConvertsPositions) {
    SceneMesh mesh;
    mesh.SetPositions({{1.0F, 2.0F, 3.0F}, {4.0F, 5.0F, 6.0F}});
    mesh.SetNormals({{0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}});
    mesh.SetColors({{1.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}});
    mesh.SetIndices({0, 1});

    MeshData data = SceneUploader::ToMeshData(mesh);

    ASSERT_EQ(data.vertices.size(), 2);
    EXPECT_EQ(data.vertices[0].position, glm::vec3(1.0F, 2.0F, 3.0F));
    EXPECT_EQ(data.vertices[1].position, glm::vec3(4.0F, 5.0F, 6.0F));
}

TEST(SceneUploaderTest, ToMeshDataConvertsNormals) {
    SceneMesh mesh;
    mesh.SetPositions({{0.0F, 0.0F, 0.0F}});
    mesh.SetNormals({{0.0F, 0.0F, 1.0F}});
    mesh.SetColors({{1.0F, 1.0F, 1.0F, 1.0F}});
    mesh.SetIndices({0});

    MeshData data = SceneUploader::ToMeshData(mesh);

    ASSERT_EQ(data.vertices.size(), 1);
    EXPECT_EQ(data.vertices[0].normal, glm::vec3(0.0F, 0.0F, 1.0F));
}

TEST(SceneUploaderTest, ToMeshDataConvertsColors) {
    SceneMesh mesh;
    mesh.SetPositions({{0.0F, 0.0F, 0.0F}});
    mesh.SetNormals({{0.0F, 1.0F, 0.0F}});
    mesh.SetColors({{0.2F, 0.4F, 0.6F, 0.8F}});
    mesh.SetIndices({0});

    MeshData data = SceneUploader::ToMeshData(mesh);

    ASSERT_EQ(data.vertices.size(), 1);
    EXPECT_EQ(data.vertices[0].color, glm::vec4(0.2F, 0.4F, 0.6F, 0.8F));
}

TEST(SceneUploaderTest, ToMeshDataConvertsIndices) {
    SceneMesh mesh;
    mesh.SetPositions({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
    mesh.SetNormals({{0, 0, 1}, {0, 0, 1}, {0, 0, 1}});
    mesh.SetColors({{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}});
    mesh.SetIndices({0, 1, 2});

    MeshData data = SceneUploader::ToMeshData(mesh);

    ASSERT_EQ(data.indices.size(), 3);
    EXPECT_EQ(data.indices[0], 0);
    EXPECT_EQ(data.indices[1], 1);
    EXPECT_EQ(data.indices[2], 2);
}

TEST(SceneUploaderTest, ToMeshDataEmptyMesh) {
    SceneMesh mesh;
    MeshData data = SceneUploader::ToMeshData(mesh);

    EXPECT_TRUE(data.vertices.empty());
    EXPECT_TRUE(data.indices.empty());
    EXPECT_TRUE(data.edgeIndices.empty());
}

TEST(SceneUploaderTest, ToMeshDataForwardsEdgeIndices) {
    SceneMesh mesh;
    mesh.SetPositions({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
    mesh.SetNormals({{0, 0, 1}, {0, 0, 1}, {0, 0, 1}});
    mesh.SetColors({{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}});
    mesh.SetIndices({0, 1, 2});
    mesh.SetEdgeIndices({0, 1, 1, 2, 2, 0});

    MeshData data = SceneUploader::ToMeshData(mesh);

    ASSERT_EQ(data.edgeIndices.size(), 6u);
    EXPECT_EQ(data.edgeIndices[0], 0u);
    EXPECT_EQ(data.edgeIndices[5], 0u);
}

// --- Upload tests (need Vulkan) ---

class SceneUploaderVulkanTest : public ::testing::Test {
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

    static SceneMesh MakeTriangleMesh(const glm::vec4& color = {1, 0, 0, 1}) {
        SceneMesh mesh;
        mesh.SetPositions({{0.0F, 0.5F, 0.0F}, {-0.5F, -0.5F, 0.0F}, {0.5F, -0.5F, 0.0F}});
        mesh.SetNormals({{0, 0, 1}, {0, 0, 1}, {0, 0, 1}});
        mesh.SetUniformColor(color);
        mesh.SetIndices({0, 1, 2});
        return mesh;
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<MeshBuffer> m_meshBuffer;
};

TEST_F(SceneUploaderVulkanTest, UploadAssignsMeshHandlesToNodes) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);

    BuildResult result;
    SceneNode node;
    node.name = "Wall1";
    node.mesh = static_cast<bimeup::scene::MeshHandle>(result.meshes.size());
    result.scene.AddNode(node);
    result.meshes.push_back(MakeTriangleMesh());

    SceneUploader::Upload(result, *m_meshBuffer);

    const auto& uploadedNode = result.scene.GetNode(0);
    ASSERT_TRUE(uploadedNode.mesh.has_value());
    EXPECT_TRUE(m_meshBuffer->IsValid(uploadedNode.mesh.value()));
}

TEST_F(SceneUploaderVulkanTest, UploadMultipleMeshes) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);

    BuildResult result;
    SceneNode node1;
    node1.name = "Wall1";
    node1.mesh = static_cast<bimeup::scene::MeshHandle>(result.meshes.size());
    result.scene.AddNode(node1);
    result.meshes.push_back(MakeTriangleMesh({1, 0, 0, 1}));

    SceneNode node2;
    node2.name = "Wall2";
    node2.mesh = static_cast<bimeup::scene::MeshHandle>(result.meshes.size());
    result.scene.AddNode(node2);
    result.meshes.push_back(MakeTriangleMesh({0, 1, 0, 1}));

    SceneUploader::Upload(result, *m_meshBuffer);

    EXPECT_EQ(m_meshBuffer->MeshCount(), 2);

    const auto& n1 = result.scene.GetNode(0);
    const auto& n2 = result.scene.GetNode(1);
    ASSERT_TRUE(n1.mesh.has_value());
    ASSERT_TRUE(n2.mesh.has_value());
    EXPECT_NE(n1.mesh.value(), n2.mesh.value());
}

TEST_F(SceneUploaderVulkanTest, UploadSkipsNodesWithoutMeshIndex) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);

    // Node 0 has a mesh (index 0), node 1 has no mesh (structural node)
    BuildResult result;

    SceneNode nodeWithMesh;
    nodeWithMesh.name = "Wall";
    nodeWithMesh.mesh = static_cast<bimeup::scene::MeshHandle>(result.meshes.size());
    result.scene.AddNode(nodeWithMesh);
    result.meshes.push_back(MakeTriangleMesh());

    // Structural node - no corresponding mesh
    SceneNode structuralNode;
    structuralNode.name = "Storey";
    result.scene.AddNode(structuralNode);
    // No mesh pushed for this node

    SceneUploader::Upload(result, *m_meshBuffer);

    EXPECT_EQ(m_meshBuffer->MeshCount(), 1);
    EXPECT_TRUE(result.scene.GetNode(0).mesh.has_value());
    EXPECT_FALSE(result.scene.GetNode(1).mesh.has_value());
}
