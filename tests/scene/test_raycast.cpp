#include <gtest/gtest.h>

#include <scene/Raycast.h>
#include <scene/Scene.h>
#include <scene/SceneMesh.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace bimeup::scene;

TEST(RaycastTest, RayHitsAABB) {
    AABB box(glm::vec3(-1.0f), glm::vec3(1.0f));
    Ray ray{glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto t = RayAABBIntersect(ray, box);
    ASSERT_TRUE(t.has_value());
    EXPECT_FLOAT_EQ(*t, 4.0f);
}

TEST(RaycastTest, RayMissesAABB) {
    AABB box(glm::vec3(-1.0f), glm::vec3(1.0f));
    Ray ray{glm::vec3(5.0f, 5.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto t = RayAABBIntersect(ray, box);
    EXPECT_FALSE(t.has_value());
}

TEST(RaycastTest, RayBehindAABBMisses) {
    AABB box(glm::vec3(-1.0f), glm::vec3(1.0f));
    Ray ray{glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto t = RayAABBIntersect(ray, box);
    EXPECT_FALSE(t.has_value());
}

TEST(RaycastTest, RayHitsTriangle) {
    glm::vec3 v0(-1.0f, -1.0f, 0.0f);
    glm::vec3 v1(1.0f, -1.0f, 0.0f);
    glm::vec3 v2(0.0f, 1.0f, 0.0f);
    Ray ray{glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto t = RayTriangleIntersect(ray, v0, v1, v2);
    ASSERT_TRUE(t.has_value());
    EXPECT_FLOAT_EQ(*t, 5.0f);
}

TEST(RaycastTest, RayMissesTriangle) {
    glm::vec3 v0(-1.0f, -1.0f, 0.0f);
    glm::vec3 v1(1.0f, -1.0f, 0.0f);
    glm::vec3 v2(0.0f, 1.0f, 0.0f);
    Ray ray{glm::vec3(5.0f, 5.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto t = RayTriangleIntersect(ray, v0, v1, v2);
    EXPECT_FALSE(t.has_value());
}

TEST(RaycastTest, RayParallelToTriangleMisses) {
    glm::vec3 v0(-1.0f, -1.0f, 0.0f);
    glm::vec3 v1(1.0f, -1.0f, 0.0f);
    glm::vec3 v2(0.0f, 1.0f, 0.0f);
    Ray ray{glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(1.0f, 0.0f, 0.0f)};

    auto t = RayTriangleIntersect(ray, v0, v1, v2);
    EXPECT_FALSE(t.has_value());
}

namespace {

SceneMesh MakeQuadMesh() {
    SceneMesh mesh;
    mesh.SetPositions({
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
    });
    mesh.SetIndices({0, 1, 2, 0, 2, 3});
    return mesh;
}

} // namespace

TEST(RaycastTest, RaycastScenePicksClosestNode) {
    Scene scene;
    std::vector<SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    SceneNode nearNode;
    nearNode.mesh = 0;
    nearNode.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
    nearNode.bounds = AABB(glm::vec3(-1.0f, -1.0f, -2.0f), glm::vec3(1.0f, 1.0f, -2.0f));
    NodeId nearId = scene.AddNode(nearNode);

    SceneNode farNode;
    farNode.mesh = 0;
    farNode.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
    farNode.bounds = AABB(glm::vec3(-1.0f, -1.0f, 2.0f), glm::vec3(1.0f, 1.0f, 2.0f));
    scene.AddNode(farNode);

    Ray ray{glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto hit = RaycastScene(ray, scene, meshes);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->nodeId, nearId);
    EXPECT_FLOAT_EQ(hit->t, 8.0f);
}

TEST(RaycastTest, RaycastSceneIgnoresInvisibleNodes) {
    Scene scene;
    std::vector<SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    SceneNode hiddenNode;
    hiddenNode.mesh = 0;
    hiddenNode.transform = glm::mat4(1.0f);
    hiddenNode.bounds = AABB(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
    hiddenNode.visible = false;
    scene.AddNode(hiddenNode);

    Ray ray{glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto hit = RaycastScene(ray, scene, meshes);
    EXPECT_FALSE(hit.has_value());
}

TEST(RaycastTest, BakedMeshRoutesTrianglesToOwners) {
    // Two nodes that share one merged mesh (positions in world-space, identity
    // node transforms). The ray passes through BOTH nodes' AABBs and hits B's
    // triangle first. The legacy per-node loop would attribute B's triangle to
    // the first node whose AABB the ray enters (A), returning the wrong owner;
    // the baked-mesh path must resolve ownership per triangle.
    Scene scene;

    SceneNode nodeA;
    nodeA.mesh = 0;
    nodeA.transform = glm::mat4(1.0f);
    // A's element occupies z∈[-0.1, 0.1]
    nodeA.bounds = AABB(glm::vec3(-1.0f, -1.0f, -0.1f), glm::vec3(1.0f, 1.0f, 0.1f));
    NodeId idA = scene.AddNode(nodeA);

    SceneNode nodeB;
    nodeB.mesh = 0;
    nodeB.transform = glm::mat4(1.0f);
    // B's element sits CLOSER to the camera at z∈[-5.1, -4.9]
    nodeB.bounds = AABB(glm::vec3(-1.0f, -1.0f, -5.1f), glm::vec3(1.0f, 1.0f, -4.9f));
    NodeId idB = scene.AddNode(nodeB);

    SceneMesh merged;
    merged.SetPositions({
        // Triangle 0 at z=0 (A's)
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        // Triangle 1 at z=-5 (B's) — closer to the ray origin
        {-1.0f, -1.0f, -5.0f}, {1.0f, -1.0f, -5.0f}, {0.0f, 1.0f, -5.0f},
    });
    merged.SetIndices({0, 1, 2, 3, 4, 5});
    merged.SetTriangleOwners({idA, idB});

    std::vector<SceneMesh> meshes;
    meshes.push_back(std::move(merged));

    Ray ray{glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
    auto hit = RaycastScene(ray, scene, meshes);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->nodeId, idB);
    EXPECT_FLOAT_EQ(hit->t, 5.0f);
}

TEST(RaycastTest, BakedMeshSkipsInvisibleOwnerTriangles) {
    Scene scene;

    SceneNode hiddenNode;
    hiddenNode.mesh = 0;
    hiddenNode.transform = glm::mat4(1.0f);
    hiddenNode.bounds = AABB(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
    hiddenNode.visible = false;
    NodeId hiddenId = scene.AddNode(hiddenNode);

    SceneNode visibleNode;
    visibleNode.mesh = 0;
    visibleNode.transform = glm::mat4(1.0f);
    visibleNode.bounds = AABB(glm::vec3(9.0f, -1.0f, 0.0f), glm::vec3(11.0f, 1.0f, 0.0f));
    NodeId visibleId = scene.AddNode(visibleNode);

    SceneMesh merged;
    merged.SetPositions({
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {9.0f, -1.0f, 0.0f}, {11.0f, -1.0f, 0.0f}, {10.0f, 1.0f, 0.0f},
    });
    merged.SetIndices({0, 1, 2, 3, 4, 5});
    merged.SetTriangleOwners({hiddenId, visibleId});

    std::vector<SceneMesh> meshes;
    meshes.push_back(std::move(merged));

    // Ray pointing at triangle 0 (hidden) — should miss.
    Ray rayHidden{glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
    auto hit = RaycastScene(rayHidden, scene, meshes);
    EXPECT_FALSE(hit.has_value());

    // Ray pointing at triangle 1 (visible) — still works.
    Ray rayVisible{glm::vec3(10.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
    auto hitV = RaycastScene(rayVisible, scene, meshes);
    ASSERT_TRUE(hitV.has_value());
    EXPECT_EQ(hitV->nodeId, visibleId);
}

TEST(RaycastTest, BakedMeshHitReturnsTriangleVertices) {
    Scene scene;
    SceneNode node;
    node.mesh = 0;
    node.transform = glm::mat4(1.0f);
    node.bounds = AABB(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
    NodeId id = scene.AddNode(node);

    SceneMesh mesh;
    mesh.SetPositions({
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    });
    mesh.SetIndices({0, 1, 2});
    mesh.SetTriangleOwners({id});

    std::vector<SceneMesh> meshes;
    meshes.push_back(std::move(mesh));

    Ray ray{glm::vec3(0.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
    auto hit = RaycastScene(ray, scene, meshes);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->triV0, glm::vec3(-1.0f, -1.0f, 0.0f));
    EXPECT_EQ(hit->triV1, glm::vec3(1.0f, -1.0f, 0.0f));
    EXPECT_EQ(hit->triV2, glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(RaycastTest, AttachedMeshHitReturnsWorldSpaceTriangleVertices) {
    Scene scene;
    std::vector<SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    SceneNode node;
    node.mesh = 0;
    node.transform = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
    node.bounds = AABB(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
    scene.AddNode(node);

    Ray ray{glm::vec3(10.0f, 0.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};
    auto hit = RaycastScene(ray, scene, meshes);
    ASSERT_TRUE(hit.has_value());
    // World-space triangle vertices should be translated by the node transform.
    EXPECT_FLOAT_EQ(hit->triV0.x, 9.0f);
    EXPECT_FLOAT_EQ(hit->triV1.x, 11.0f);
}

// Fix #2 — PoV placement needs to see the slab even when a wall stands in
// front of it. The type-filtered overload skips non-matching elements so the
// hover disk / teleport click finds the slab behind them.
TEST(RaycastTest, RaycastSceneFilterSkipsNonMatchingElements) {
    Scene scene;
    std::vector<SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    // Wall in front (z=-2), Slab behind (z=2). Without a filter, the wall is
    // the closest hit and wins.
    SceneNode wall;
    wall.ifcType = "IfcWall";
    wall.mesh = 0;
    wall.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
    wall.bounds = AABB(glm::vec3(-1.0f, -1.0f, -2.0f), glm::vec3(1.0f, 1.0f, -2.0f));
    NodeId wallId = scene.AddNode(wall);

    SceneNode slab;
    slab.ifcType = "IfcSlab";
    slab.mesh = 0;
    slab.transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 2.0f));
    slab.bounds = AABB(glm::vec3(-1.0f, -1.0f, 2.0f), glm::vec3(1.0f, 1.0f, 2.0f));
    NodeId slabId = scene.AddNode(slab);

    Ray ray{glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    // Without filter: closest (wall) wins.
    auto unfiltered = RaycastScene(ray, scene, meshes);
    ASSERT_TRUE(unfiltered.has_value());
    EXPECT_EQ(unfiltered->nodeId, wallId);

    // With filter restricting to IfcSlab: slab wins even though wall is closer.
    NodeFilter slabOnly = [](const SceneNode& n) { return n.ifcType == "IfcSlab"; };
    auto filtered = RaycastScene(ray, scene, meshes, slabOnly);
    ASSERT_TRUE(filtered.has_value());
    EXPECT_EQ(filtered->nodeId, slabId);
}

TEST(RaycastTest, RaycastSceneMissReturnsNullopt) {
    Scene scene;
    std::vector<SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    SceneNode node;
    node.mesh = 0;
    node.transform = glm::mat4(1.0f);
    node.bounds = AABB(glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.0f));
    scene.AddNode(node);

    Ray ray{glm::vec3(10.0f, 10.0f, -5.0f), glm::vec3(0.0f, 0.0f, 1.0f)};

    auto hit = RaycastScene(ray, scene, meshes);
    EXPECT_FALSE(hit.has_value());
}
