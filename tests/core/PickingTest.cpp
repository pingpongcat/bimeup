#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <optional>
#include <vector>

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Picking.h"

#include <scene/Scene.h>
#include <scene/SceneMesh.h>

namespace bimeup::core {

namespace {

scene::SceneMesh MakeQuadMesh() {
    scene::SceneMesh mesh;
    mesh.SetPositions({
        {-1.0f, -1.0f, 0.0f},
        {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f},
    });
    mesh.SetIndices({0, 1, 2, 0, 2, 3});
    return mesh;
}

}  // namespace

TEST(PickingTest, ScreenCenterProducesForwardRay) {
    glm::vec2 size(800.0f, 600.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                                 glm::vec3(0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      size.x / size.y, 0.1f, 100.0f);

    scene::Ray ray = ScreenPointToRay(size * 0.5f, size, view, proj);

    EXPECT_NEAR(ray.origin.x, 0.0f, 1e-3f);
    EXPECT_NEAR(ray.origin.y, 0.0f, 1e-3f);
    EXPECT_LT(ray.origin.z, 5.0f);
    EXPECT_GT(ray.origin.z, 0.0f);

    glm::vec3 dir = glm::normalize(ray.direction);
    EXPECT_NEAR(dir.x, 0.0f, 1e-4f);
    EXPECT_NEAR(dir.y, 0.0f, 1e-4f);
    EXPECT_NEAR(dir.z, -1.0f, 1e-4f);
}

TEST(PickingTest, ClickOnElementPublishesExpressIdFromNode) {
    // The published event must carry the node's IFC expressId, not the scene NodeId.
    scene::Scene scene;
    std::vector<scene::SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    scene::SceneNode node;
    node.expressId = 42;  // distinct from the scene NodeId (which will be 0).
    node.mesh = 0;
    node.transform = glm::mat4(1.0f);
    node.bounds = scene::AABB(glm::vec3(-1.0f, -1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 0.0f));
    scene::NodeId id = scene.AddNode(node);
    ASSERT_NE(id, node.expressId);

    glm::vec2 size(800.0f, 600.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                                 glm::vec3(0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      size.x / size.y, 0.1f, 100.0f);

    EventBus bus;
    std::optional<ElementSelected> received;
    bus.Subscribe<ElementSelected>(
        [&](const ElementSelected& e) { received = e; });

    bool hit = PickElement(size * 0.5f, size, view, proj, scene, meshes, bus,
                           /*additive=*/false);

    EXPECT_TRUE(hit);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->expressId, 42u);
    EXPECT_FALSE(received->additive);
}

TEST(PickingTest, ClickOnEmptySpaceDoesNotPublish) {
    scene::Scene scene;
    std::vector<scene::SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    scene::SceneNode node;
    node.mesh = 0;
    node.transform = glm::translate(glm::mat4(1.0f),
                                    glm::vec3(50.0f, 0.0f, 0.0f));
    node.bounds = scene::AABB(glm::vec3(49.0f, -1.0f, 0.0f),
                              glm::vec3(51.0f, 1.0f, 0.0f));
    scene.AddNode(node);

    glm::vec2 size(800.0f, 600.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                                 glm::vec3(0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      size.x / size.y, 0.1f, 100.0f);

    EventBus bus;
    int callCount = 0;
    bus.Subscribe<ElementSelected>(
        [&](const ElementSelected&) { ++callCount; });

    bool hit = PickElement(size * 0.5f, size, view, proj, scene, meshes, bus,
                           /*additive=*/false);

    EXPECT_FALSE(hit);
    EXPECT_EQ(callCount, 0);
}

TEST(PickingTest, HoverOverElementPublishesHoveredWithExpressId) {
    scene::Scene scene;
    std::vector<scene::SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    scene::SceneNode node;
    node.expressId = 77;
    node.mesh = 0;
    node.transform = glm::mat4(1.0f);
    node.bounds = scene::AABB(glm::vec3(-1.0f, -1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 0.0f));
    scene::NodeId id = scene.AddNode(node);
    ASSERT_NE(id, node.expressId);

    glm::vec2 size(800.0f, 600.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                                 glm::vec3(0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      size.x / size.y, 0.1f, 100.0f);

    EventBus bus;
    std::optional<ElementHovered> received;
    bus.Subscribe<ElementHovered>(
        [&](const ElementHovered& e) { received = e; });

    auto result = HoverElement(size * 0.5f, size, view, proj, scene, meshes, bus);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, id);
    ASSERT_TRUE(received.has_value());
    ASSERT_TRUE(received->expressId.has_value());
    EXPECT_EQ(*received->expressId, 77u);
}

TEST(PickingTest, HoverOverEmptySpacePublishesHoveredWithNullopt) {
    scene::Scene scene;
    std::vector<scene::SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    scene::SceneNode node;
    node.mesh = 0;
    node.transform = glm::translate(glm::mat4(1.0f),
                                    glm::vec3(50.0f, 0.0f, 0.0f));
    node.bounds = scene::AABB(glm::vec3(49.0f, -1.0f, 0.0f),
                              glm::vec3(51.0f, 1.0f, 0.0f));
    scene.AddNode(node);

    glm::vec2 size(800.0f, 600.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                                 glm::vec3(0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      size.x / size.y, 0.1f, 100.0f);

    EventBus bus;
    std::optional<ElementHovered> received;
    bus.Subscribe<ElementHovered>(
        [&](const ElementHovered& e) { received = e; });

    auto result = HoverElement(size * 0.5f, size, view, proj, scene, meshes, bus);

    EXPECT_FALSE(result.has_value());
    ASSERT_TRUE(received.has_value());
    EXPECT_FALSE(received->expressId.has_value());
}

TEST(PickingTest, ClickWithAdditivePropagatesFlag) {
    scene::Scene scene;
    std::vector<scene::SceneMesh> meshes;
    meshes.push_back(MakeQuadMesh());

    scene::SceneNode node;
    node.mesh = 0;
    node.transform = glm::mat4(1.0f);
    node.bounds = scene::AABB(glm::vec3(-1.0f, -1.0f, 0.0f),
                              glm::vec3(1.0f, 1.0f, 0.0f));
    scene.AddNode(node);

    glm::vec2 size(800.0f, 600.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                                 glm::vec3(0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f),
                                      size.x / size.y, 0.1f, 100.0f);

    EventBus bus;
    std::optional<ElementSelected> received;
    bus.Subscribe<ElementSelected>(
        [&](const ElementSelected& e) { received = e; });

    bool hit = PickElement(size * 0.5f, size, view, proj, scene, meshes, bus,
                           /*additive=*/true);

    EXPECT_TRUE(hit);
    ASSERT_TRUE(received.has_value());
    EXPECT_TRUE(received->additive);
}

}  // namespace bimeup::core
