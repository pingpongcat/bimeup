#include <gtest/gtest.h>

#include <scene/SceneBuilder.h>
#include <scene/Scene.h>
#include <scene/SceneMesh.h>
#include <scene/SceneNode.h>

#include <set>
#include <string>

namespace {

bimeup::scene::SceneMesh MakeSmallMesh(const glm::vec4& color) {
    bimeup::scene::SceneMesh mesh;
    mesh.SetPositions({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
    mesh.SetNormals({{0, 0, 1}, {0, 0, 1}, {0, 0, 1}});
    mesh.SetIndices({0, 1, 2});
    mesh.SetUniformColor(color);
    return mesh;
}

bimeup::scene::SceneMesh MakeLargeMesh(const glm::vec4& color,
                                        size_t vertexCount) {
    bimeup::scene::SceneMesh mesh;
    std::vector<glm::vec3> positions(vertexCount, {0, 0, 0});
    std::vector<glm::vec3> normals(vertexCount, {0, 0, 1});
    std::vector<uint32_t> indices;
    // Create triangles from consecutive vertices
    for (size_t i = 0; i + 2 < vertexCount; i += 3) {
        indices.push_back(static_cast<uint32_t>(i));
        indices.push_back(static_cast<uint32_t>(i + 1));
        indices.push_back(static_cast<uint32_t>(i + 2));
    }
    mesh.SetPositions(std::move(positions));
    mesh.SetNormals(std::move(normals));
    mesh.SetIndices(std::move(indices));
    mesh.SetUniformColor(color);
    return mesh;
}

bimeup::scene::BuildResult MakeResultWithSmallMeshes(
    size_t count, const std::string& ifcType, const glm::vec4& color) {
    using namespace bimeup::scene;
    BuildResult result;
    for (size_t i = 0; i < count; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh(color));

        SceneNode node;
        node.name = ifcType + "_" + std::to_string(i);
        node.ifcType = ifcType;
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }
    return result;
}

} // namespace

TEST(BatchingTest, HundredSmallElementsProduceFewerMeshes) {
    auto result = MakeResultWithSmallMeshes(100, "IfcWall", {0.8f, 0.8f, 0.8f, 1.0f});
    ASSERT_EQ(result.meshes.size(), 100u);

    bimeup::scene::SceneBuilder::ApplyBatching(result);

    EXPECT_LT(result.meshes.size(), 100u);
    // All 100 same-type/color elements should merge into 1 mesh
    EXPECT_EQ(result.meshes.size(), 1u);
}

TEST(BatchingTest, BatchedMeshPreservesTotalVertexCount) {
    auto result = MakeResultWithSmallMeshes(50, "IfcWall", {0.5f, 0.5f, 0.5f, 1.0f});
    size_t totalVertsBefore = 0;
    size_t totalIdxBefore = 0;
    for (const auto& m : result.meshes) {
        totalVertsBefore += m.GetVertexCount();
        totalIdxBefore += m.GetIndexCount();
    }

    bimeup::scene::SceneBuilder::ApplyBatching(result);

    size_t totalVertsAfter = 0;
    size_t totalIdxAfter = 0;
    for (const auto& m : result.meshes) {
        totalVertsAfter += m.GetVertexCount();
        totalIdxAfter += m.GetIndexCount();
    }

    EXPECT_EQ(totalVertsAfter, totalVertsBefore);
    EXPECT_EQ(totalIdxAfter, totalIdxBefore);
}

TEST(BatchingTest, DifferentTypesSeparateBatches) {
    using namespace bimeup::scene;
    BuildResult result;
    glm::vec4 color(0.5f, 0.5f, 0.5f, 1.0f);

    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh(color));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }
    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh(color));
        SceneNode node;
        node.ifcType = "IfcSlab";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }

    ASSERT_EQ(result.meshes.size(), 20u);

    SceneBuilder::ApplyBatching(result);

    // Two types → two batched meshes
    EXPECT_EQ(result.meshes.size(), 2u);
}

TEST(BatchingTest, DifferentColorsSeparateBatches) {
    using namespace bimeup::scene;
    BuildResult result;

    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh({1.0f, 0.0f, 0.0f, 1.0f}));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }
    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh({0.0f, 0.0f, 1.0f, 1.0f}));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }

    ASSERT_EQ(result.meshes.size(), 20u);

    SceneBuilder::ApplyBatching(result);

    // Same type but different colors → two batched meshes
    EXPECT_EQ(result.meshes.size(), 2u);
}

TEST(BatchingTest, LargeMeshesNotBatched) {
    using namespace bimeup::scene;
    BuildResult result;
    glm::vec4 color(0.5f, 0.5f, 0.5f, 1.0f);

    // Add 5 large meshes (above threshold)
    for (int i = 0; i < 5; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeLargeMesh(color, 2000));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }

    ASSERT_EQ(result.meshes.size(), 5u);

    SceneBuilder::ApplyBatching(result, 1024);

    // Large meshes should remain individual
    EXPECT_EQ(result.meshes.size(), 5u);
}

TEST(BatchingTest, MixedSmallAndLargeMeshes) {
    using namespace bimeup::scene;
    BuildResult result;
    glm::vec4 color(0.5f, 0.5f, 0.5f, 1.0f);

    // 10 small meshes
    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh(color));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }
    // 3 large meshes
    for (int i = 0; i < 3; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeLargeMesh(color, 2000));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }

    ASSERT_EQ(result.meshes.size(), 13u);

    SceneBuilder::ApplyBatching(result, 1024);

    // 10 small → 1 batched + 3 large kept = 4
    EXPECT_EQ(result.meshes.size(), 4u);
}

TEST(BatchingTest, NodesStillReferenceValidMeshes) {
    auto result = MakeResultWithSmallMeshes(20, "IfcWall", {0.8f, 0.8f, 0.8f, 1.0f});

    bimeup::scene::SceneBuilder::ApplyBatching(result);

    for (size_t i = 0; i < result.scene.GetNodeCount(); ++i) {
        const auto& node =
            result.scene.GetNode(static_cast<bimeup::scene::NodeId>(i));
        if (node.mesh.has_value()) {
            EXPECT_LT(node.mesh.value(), result.meshes.size());
        }
    }
}

TEST(BatchingTest, SingleMeshGroupNotMerged) {
    // A group with only 1 small mesh shouldn't change
    auto result = MakeResultWithSmallMeshes(1, "IfcWall", {0.8f, 0.8f, 0.8f, 1.0f});

    bimeup::scene::SceneBuilder::ApplyBatching(result);

    EXPECT_EQ(result.meshes.size(), 1u);
}

// 7.8b: opaque and translucent meshes sharing the same IFC type + RGB must NOT
// merge into one batch, because a later stage draws them in separate passes
// (opaque then alpha-blended). SceneMesh::IsTransparent() is the bucket key.
TEST(BatchingTest, OpacityBucketSeparatesBatches) {
    using namespace bimeup::scene;
    BuildResult result;

    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh({0.5f, 0.5f, 0.5f, 1.0f}));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }
    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh({0.5f, 0.5f, 0.5f, 0.3f}));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }

    SceneBuilder::ApplyBatching(result);

    ASSERT_EQ(result.meshes.size(), 2u);
    int transparentCount = 0;
    int opaqueCount = 0;
    for (const auto& m : result.meshes) {
        if (m.IsTransparent()) ++transparentCount; else ++opaqueCount;
    }
    EXPECT_EQ(opaqueCount, 1);
    EXPECT_EQ(transparentCount, 1);
}

TEST(BatchingTest, NodesWithoutMeshUnaffected) {
    using namespace bimeup::scene;
    BuildResult result;

    // Add 5 nodes without meshes (structural/hierarchy nodes)
    for (int i = 0; i < 5; ++i) {
        SceneNode node;
        node.name = "Container_" + std::to_string(i);
        node.ifcType = "IfcBuildingStorey";
        result.scene.AddNode(std::move(node));
    }
    // Add 10 nodes with small meshes
    glm::vec4 color(0.5f, 0.5f, 0.5f, 1.0f);
    for (int i = 0; i < 10; ++i) {
        auto handle = static_cast<MeshHandle>(result.meshes.size());
        result.meshes.push_back(MakeSmallMesh(color));
        SceneNode node;
        node.ifcType = "IfcWall";
        node.mesh = handle;
        result.scene.AddNode(std::move(node));
    }

    SceneBuilder::ApplyBatching(result);

    // Container nodes should still have no mesh
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(result.scene.GetNode(static_cast<NodeId>(i)).mesh.has_value());
    }
    // Mesh nodes should still have a mesh
    for (int i = 5; i < 15; ++i) {
        EXPECT_TRUE(result.scene.GetNode(static_cast<NodeId>(i)).mesh.has_value());
    }
}
