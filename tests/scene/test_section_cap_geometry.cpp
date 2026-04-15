#include <gtest/gtest.h>

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

