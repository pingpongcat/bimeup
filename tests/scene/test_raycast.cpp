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
