#pragma once

#include "AABB.h"
#include "Scene.h"
#include "SceneMesh.h"

#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace bimeup::scene {

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
};

struct RayHit {
    NodeId nodeId = InvalidNodeId;
    float t = 0.0f;
    glm::vec3 point{0.0f};
};

std::optional<float> RayAABBIntersect(const Ray& ray, const AABB& box);

std::optional<float> RayTriangleIntersect(const Ray& ray,
                                          const glm::vec3& v0,
                                          const glm::vec3& v1,
                                          const glm::vec3& v2);

std::optional<RayHit> RaycastScene(const Ray& ray,
                                   const Scene& scene,
                                   std::span<const SceneMesh> meshes);

} // namespace bimeup::scene
