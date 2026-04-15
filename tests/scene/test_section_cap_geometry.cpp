#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>

#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>

#include <scene/Scene.h>
#include <scene/SceneMesh.h>
#include <scene/SceneNode.h>
#include <scene/SectionCapGeometry.h>

using bimeup::renderer::ClipPlaneManager;
using bimeup::scene::BuildSectionCapVertices;
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
        // -Z back face (CCW when viewed from -Z)
        0, 2, 1, 0, 3, 2,
        // +Z front face
        4, 5, 6, 4, 6, 7,
        // -Y bottom
        0, 1, 5, 0, 5, 4,
        // +Y top
        3, 7, 6, 3, 6, 2,
        // -X left
        0, 4, 7, 0, 7, 3,
        // +X right
        1, 2, 6, 1, 6, 5,
    });
    return mesh;
}

Scene MakeSceneWithCubeNode(MeshHandle handle, const glm::mat4& transform = glm::mat4(1.0F)) {
    Scene scene;
    SceneNode node;
    node.transform = transform;
    node.mesh = handle;
    scene.AddNode(std::move(node));
    return scene;
}

SceneMesh MakeTwoCubeBatchedMesh(NodeId ownerA, NodeId ownerB,
                                 const glm::vec4& colorA,
                                 const glm::vec4& colorB) {
    // Two unit cubes in world-space: A centered at (-1.5, 0, 0), B at (1.5, 0, 0).
    // Plane y=0 slices both cross-sections horizontally.
    SceneMesh mesh;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec4> colors;
    std::vector<std::uint32_t> indices;
    std::vector<NodeId> owners;

    auto addCube = [&](float cx, NodeId owner, const glm::vec4& color) {
        const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
        positions.push_back({cx - 0.5F, -0.5F, -0.5F});
        positions.push_back({cx + 0.5F, -0.5F, -0.5F});
        positions.push_back({cx + 0.5F,  0.5F, -0.5F});
        positions.push_back({cx - 0.5F,  0.5F, -0.5F});
        positions.push_back({cx - 0.5F, -0.5F,  0.5F});
        positions.push_back({cx + 0.5F, -0.5F,  0.5F});
        positions.push_back({cx + 0.5F,  0.5F,  0.5F});
        positions.push_back({cx - 0.5F,  0.5F,  0.5F});
        for (int i = 0; i < 8; ++i) colors.push_back(color);

        const std::array<std::uint32_t, 36> idx = {
            0, 2, 1, 0, 3, 2,  // -Z
            4, 5, 6, 4, 6, 7,  // +Z
            0, 1, 5, 0, 5, 4,  // -Y
            3, 7, 6, 3, 6, 2,  // +Y
            0, 4, 7, 0, 7, 3,  // -X
            1, 2, 6, 1, 6, 5,  // +X
        };
        for (auto i : idx) indices.push_back(base + i);
        for (int t = 0; t < 12; ++t) owners.push_back(owner);
    };

    addCube(-1.5F, ownerA, colorA);
    addCube( 1.5F, ownerB, colorB);

    mesh.SetPositions(std::move(positions));
    mesh.SetColors(std::move(colors));
    mesh.SetIndices(std::move(indices));
    mesh.SetTriangleOwners(std::move(owners));
    return mesh;
}

}  // namespace

// ---- Free-function BuildSectionCapVertices tests (no Vulkan) ----

TEST(SectionCapGeometryBuild, EmptyManagerProducesNoVertices) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionCapGeometryBuild, SectionFillFalseProducesNoVertices) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    // y = 0 plane, normal +Y — straddles cube — but sectionFill=false.
    manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionCapGeometryBuild, PlaneMissingMeshProducesNoVertices) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    // y = 2 plane — cube ranges [-0.5, 0.5] — no straddle.
    auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, -2.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionCapGeometryBuild, PlaneCuttingCubeProducesSquareCapWithFillColor) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);
    const glm::vec4 kFill(0.2F, 0.7F, 0.3F, 1.0F);
    manager.SetFillColor(id, kFill);

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);

    ASSERT_FALSE(verts.empty());
    // Each of the 4 side faces contributes 2 straddling triangles → 8 perimeter
    // segments that stitch into an 8-vertex closed polygon (4 corners + 4 edge
    // midpoints), ear-clipped into 6 triangles = 18 vertices.
    EXPECT_EQ(verts.size() % 3u, 0u);
    EXPECT_EQ(verts.size(), 18u);
    for (const auto& v : verts) {
        EXPECT_NEAR(v.position.y, 0.0F, 1e-4F);
        EXPECT_GE(v.position.x, -0.5F - 1e-4F);
        EXPECT_LE(v.position.x,  0.5F + 1e-4F);
        EXPECT_GE(v.position.z, -0.5F - 1e-4F);
        EXPECT_LE(v.position.z,  0.5F + 1e-4F);
        EXPECT_FLOAT_EQ(v.color.r, kFill.r);
        EXPECT_FLOAT_EQ(v.color.g, kFill.g);
        EXPECT_FLOAT_EQ(v.color.b, kFill.b);
        EXPECT_FLOAT_EQ(v.color.a, kFill.a);
    }
}

TEST(SectionCapGeometryBuild, RespectsNodeWorldTransform) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const glm::mat4 shift = glm::translate(glm::mat4(1.0F), glm::vec3(10.0F, 0.0F, 0.0F));
    const Scene scene = MakeSceneWithCubeNode(0, shift);
    ClipPlaneManager manager;
    auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);

    ASSERT_FALSE(verts.empty());
    for (const auto& v : verts) {
        EXPECT_NEAR(v.position.y, 0.0F, 1e-4F);
        EXPECT_GE(v.position.x, 9.5F - 1e-4F);
        EXPECT_LE(v.position.x, 10.5F + 1e-4F);
    }
}

TEST(SectionCapGeometryBuild, InvisibleNodesSkipped) {
    const SceneMesh cube = MakeUnitCubeMesh();
    Scene scene;
    {
        SceneNode node;
        node.mesh = 0U;
        node.visible = false;
        scene.AddNode(std::move(node));
    }
    ClipPlaneManager manager;
    auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionCapGeometryBuild, DisabledPlaneSkipped) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);
    manager.SetEnabled(id, false);

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);
    EXPECT_TRUE(verts.empty());
}

TEST(SectionCapGeometryBuild, BatchedMeshCapsEachOwnerWithItsColor) {
    Scene scene;
    const NodeId idA = scene.AddNode(SceneNode{});
    const NodeId idB = scene.AddNode(SceneNode{});
    const glm::vec4 colorA(1.0F, 0.0F, 0.0F, 1.0F);
    const glm::vec4 colorB(0.0F, 1.0F, 0.0F, 1.0F);
    const SceneMesh mesh = MakeTwoCubeBatchedMesh(idA, idB, colorA, colorB);
    SceneNode batchRoot;
    batchRoot.mesh = 0U;
    scene.AddNode(std::move(batchRoot));

    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);
    // White fill lets us read element colour straight out of the tint.
    manager.SetFillColor(id, glm::vec4(1.0F, 1.0F, 1.0F, 1.0F));

    const auto verts = BuildSectionCapVertices(scene, {&mesh, 1}, manager);
    ASSERT_FALSE(verts.empty());
    EXPECT_EQ(verts.size() % 3u, 0u);

    bool sawRed = false;
    bool sawGreen = false;
    bool capOnLeft = false;
    bool capOnRight = false;
    for (const auto& v : verts) {
        EXPECT_NEAR(v.position.y, 0.0F, 1e-4F);
        if (v.color.r > 0.5F && v.color.g < 0.1F) sawRed = true;
        if (v.color.g > 0.5F && v.color.r < 0.1F) sawGreen = true;
        if (v.position.x < -0.9F) capOnLeft = true;
        if (v.position.x >  0.9F) capOnRight = true;
    }
    EXPECT_TRUE(sawRed);
    EXPECT_TRUE(sawGreen);
    EXPECT_TRUE(capOnLeft);
    EXPECT_TRUE(capOnRight);
}

TEST(SectionCapGeometryBuild, BatchedMeshSkipsInvisibleOwner) {
    Scene scene;
    const NodeId idA = scene.AddNode(SceneNode{});
    const NodeId idB = scene.AddNode(SceneNode{});
    scene.GetNode(idB).visible = false;
    const SceneMesh mesh = MakeTwoCubeBatchedMesh(
        idA, idB,
        glm::vec4(1.0F, 0.0F, 0.0F, 1.0F),
        glm::vec4(0.0F, 1.0F, 0.0F, 1.0F));
    SceneNode batchRoot;
    batchRoot.mesh = 0U;
    scene.AddNode(std::move(batchRoot));

    ClipPlaneManager manager;
    const auto id = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(id, true);
    manager.SetFillColor(id, glm::vec4(1.0F, 1.0F, 1.0F, 1.0F));

    const auto verts = BuildSectionCapVertices(scene, {&mesh, 1}, manager);
    ASSERT_FALSE(verts.empty());
    for (const auto& v : verts) {
        // Element B (green) must be absent; only element A (red) contributes.
        EXPECT_LT(v.color.g, 0.5F);
        EXPECT_LT(v.position.x, 0.0F);  // all caps on the left cube
    }
}

TEST(SectionCapGeometryBuild, MultipleSectionPlanesEachContribute) {
    const SceneMesh cube = MakeUnitCubeMesh();
    const Scene scene = MakeSceneWithCubeNode(0);
    ClipPlaneManager manager;
    auto idY = manager.AddPlane(glm::vec4(0.0F, 1.0F, 0.0F, 0.0F));
    manager.SetSectionFill(idY, true);
    manager.SetFillColor(idY, glm::vec4(1.0F, 0.0F, 0.0F, 1.0F));
    auto idX = manager.AddPlane(glm::vec4(1.0F, 0.0F, 0.0F, 0.0F));
    manager.SetSectionFill(idX, true);
    manager.SetFillColor(idX, glm::vec4(0.0F, 0.0F, 1.0F, 1.0F));

    const auto verts = BuildSectionCapVertices(scene, {&cube, 1}, manager);

    ASSERT_EQ(verts.size(), 36u);  // 18 per plane × 2 planes
    size_t red = 0;
    size_t blue = 0;
    for (const auto& v : verts) {
        if (v.color.r > 0.5F) ++red;
        if (v.color.b > 0.5F) ++blue;
    }
    EXPECT_EQ(red, 18u);
    EXPECT_EQ(blue, 18u);
}

