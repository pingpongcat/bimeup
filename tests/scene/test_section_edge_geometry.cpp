#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>

#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>

#include <scene/Scene.h>
#include <scene/SceneMesh.h>
#include <scene/SceneNode.h>
#include <scene/SectionEdgeGeometry.h>

using bimeup::renderer::ClipPlaneManager;
using bimeup::scene::BuildSectionEdgeVertices;
using bimeup::scene::MeshHandle;
using bimeup::scene::NodeId;
using bimeup::scene::Scene;
using bimeup::scene::SceneMesh;
using bimeup::scene::SceneNode;

namespace {

SceneMesh MakeUnitCubeMesh() {
    SceneMesh mesh;
    mesh.SetPositions({
        {-0.5F, -0.5F, -0.5F},  // 0
        { 0.5F, -0.5F, -0.5F},  // 1
        { 0.5F,  0.5F, -0.5F},  // 2
        {-0.5F,  0.5F, -0.5F},  // 3
        {-0.5F, -0.5F,  0.5F},  // 4
        { 0.5F, -0.5F,  0.5F},  // 5
        { 0.5F,  0.5F,  0.5F},  // 6
        {-0.5F,  0.5F,  0.5F},  // 7
    });
    mesh.SetIndices({
        0, 2, 1, 0, 3, 2,  // -Z
        4, 5, 6, 4, 6, 7,  // +Z
        0, 1, 5, 0, 5, 4,  // -Y
        3, 7, 6, 3, 6, 2,  // +Y
        0, 4, 7, 0, 7, 3,  // -X
        1, 2, 6, 1, 6, 5,  // +X
    });
    return mesh;
}

Scene MakeSceneWithCubeNode(MeshHandle handle,
                            const glm::mat4& transform = glm::mat4(1.0F)) {
    Scene scene;
    SceneNode node;
    node.transform = transform;
    node.mesh = handle;
    scene.AddNode(std::move(node));
    return scene;
}

SceneMesh MakeTwoCubeBatchedMesh(NodeId ownerA, NodeId ownerB) {
    // Two unit cubes in world-space: A at (-1.5, 0, 0), B at (1.5, 0, 0).
    SceneMesh mesh;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> colors;
    std::vector<std::uint32_t> indices;
    std::vector<NodeId> owners;

    auto addCube = [&](float cx, NodeId owner) {
        const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
        positions.push_back({cx - 0.5F, -0.5F, -0.5F});
        positions.push_back({cx + 0.5F, -0.5F, -0.5F});
        positions.push_back({cx + 0.5F,  0.5F, -0.5F});
        positions.push_back({cx - 0.5F,  0.5F, -0.5F});
        positions.push_back({cx - 0.5F, -0.5F,  0.5F});
        positions.push_back({cx + 0.5F, -0.5F,  0.5F});
        positions.push_back({cx + 0.5F,  0.5F,  0.5F});
        positions.push_back({cx - 0.5F,  0.5F,  0.5F});
        for (int i = 0; i < 8; ++i) colors.emplace_back(1.0F);

        const std::array<std::uint32_t, 36> idx = {
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            0, 1, 5, 0, 5, 4,
            3, 7, 6, 3, 6, 2,
            0, 4, 7, 0, 7, 3,
            1, 2, 6, 1, 6, 5,
        };
        for (auto i : idx) indices.push_back(base + i);
        for (int t = 0; t < 12; ++t) owners.push_back(owner);
    };

    addCube(-1.5F, ownerA);
    addCube( 1.5F, ownerB);

    mesh.SetPositions(std::move(positions));
    mesh.SetColors(std::move(colors));
    mesh.SetIndices(std::move(indices));
    mesh.SetTriangleOwners(std::move(owners));
    return mesh;
}

}  // namespace

TEST(SectionEdgeGeometryBuild, EmptyManagerProducesNoVertices) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;

    const auto verts = BuildSectionEdgeVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionEdgeGeometryBuild, SectionFillFalseProducesNoVertices) {
    // Section outlines are gated on `sectionFill` — a plane that only clips but
    // doesn't fill has no outline to draw.
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));

    const auto verts = BuildSectionEdgeVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionEdgeGeometryBuild, DisabledPlaneSkipped) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);
    manager.SetEnabled(id, false);

    const auto verts = BuildSectionEdgeVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionEdgeGeometryBuild, PlaneCuttingCubeProducesClosedPerimeterLineList) {
    // The slicer emits 8 coplanar segments (two per side face) that stitch into
    // a closed 8-vertex loop. Emitting that loop as a line list = 8 segments
    // = 16 vertex positions. All vertices must lie on the plane.
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionEdgeVertices(scene, {&cube, 1}, manager);

    ASSERT_FALSE(verts.empty());
    EXPECT_EQ(verts.size() % 2u, 0u);
    EXPECT_EQ(verts.size(), 16u);
    for (const auto& v : verts) {
        EXPECT_NEAR(v.position.y, 0.0F, 1e-4F);
        EXPECT_GE(v.position.x, -0.5F - 1e-4F);
        EXPECT_LE(v.position.x,  0.5F + 1e-4F);
        EXPECT_GE(v.position.z, -0.5F - 1e-4F);
        EXPECT_LE(v.position.z,  0.5F + 1e-4F);
    }
}

TEST(SectionEdgeGeometryBuild, TwoDisconnectedBatchedCubesProduceIndependentOutlines) {
    Scene scene;
    const NodeId idA = scene.AddNode(SceneNode{});
    const NodeId idB = scene.AddNode(SceneNode{});
    const SceneMesh mesh = MakeTwoCubeBatchedMesh(idA, idB);
    SceneNode batchRoot;
    batchRoot.mesh = 0U;
    scene.AddNode(std::move(batchRoot));

    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionEdgeVertices(scene, {&mesh, 1}, manager);

    ASSERT_FALSE(verts.empty());
    // Two cubes × 16 line-list verts each = 32. Slicing per-owner keeps the
    // loops independent even though they're batched into one mesh.
    EXPECT_EQ(verts.size(), 32u);

    bool sawLeft = false;
    bool sawRight = false;
    for (const auto& v : verts) {
        EXPECT_NEAR(v.position.y, 0.0F, 1e-4F);
        if (v.position.x < -0.9F) sawLeft = true;
        if (v.position.x >  0.9F) sawRight = true;
    }
    EXPECT_TRUE(sawLeft);
    EXPECT_TRUE(sawRight);
}

TEST(SectionEdgeGeometryBuild, BatchedMeshSkipsInvisibleOwner) {
    Scene scene;
    const NodeId idA = scene.AddNode(SceneNode{});
    const NodeId idB = scene.AddNode(SceneNode{});
    scene.GetNode(idB).visible = false;
    const SceneMesh mesh = MakeTwoCubeBatchedMesh(idA, idB);
    SceneNode batchRoot;
    batchRoot.mesh = 0U;
    scene.AddNode(std::move(batchRoot));

    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionEdgeVertices(scene, {&mesh, 1}, manager);
    ASSERT_FALSE(verts.empty());
    for (const auto& v : verts) {
        EXPECT_LT(v.position.x, 0.0F);  // only the left (visible) cube contributes
    }
}

TEST(SectionEdgeGeometryBuild, RespectsNodeWorldTransform) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const glm::mat4 shift = glm::translate(glm::mat4(1.0F), glm::vec3(10.0F, 0.0F, 0.0F));
    const Scene scene = MakeSceneWithCubeNode(0, shift);
    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionEdgeVertices(scene, {&cube, 1}, manager);

    ASSERT_FALSE(verts.empty());
    for (const auto& v : verts) {
        EXPECT_NEAR(v.position.y, 0.0F, 1e-4F);
        EXPECT_GE(v.position.x,  9.5F - 1e-4F);
        EXPECT_LE(v.position.x, 10.5F + 1e-4F);
    }
}
