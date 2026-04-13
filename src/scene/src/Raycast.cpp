#include "scene/Raycast.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bimeup::scene {

std::optional<float> RayAABBIntersect(const Ray& ray, const AABB& box) {
    if (!box.IsValid()) {
        return std::nullopt;
    }

    const glm::vec3& bmin = box.GetMin();
    const glm::vec3& bmax = box.GetMax();

    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();

    for (int i = 0; i < 3; ++i) {
        float o = ray.origin[i];
        float d = ray.direction[i];
        float lo = bmin[i];
        float hi = bmax[i];

        if (std::abs(d) < 1e-8f) {
            if (o < lo || o > hi) {
                return std::nullopt;
            }
            continue;
        }

        float t1 = (lo - o) / d;
        float t2 = (hi - o) / d;
        if (t1 > t2) std::swap(t1, t2);

        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);

        if (tmin > tmax) {
            return std::nullopt;
        }
    }

    if (tmax < 0.0f) {
        return std::nullopt;
    }

    return tmin >= 0.0f ? tmin : tmax;
}

std::optional<float> RayTriangleIntersect(const Ray& ray,
                                          const glm::vec3& v0,
                                          const glm::vec3& v1,
                                          const glm::vec3& v2) {
    constexpr float kEpsilon = 1e-8f;

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    if (std::abs(a) < kEpsilon) {
        return std::nullopt;
    }

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    float u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) {
        return std::nullopt;
    }

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) {
        return std::nullopt;
    }

    float t = f * glm::dot(edge2, q);
    if (t < kEpsilon) {
        return std::nullopt;
    }

    return t;
}

namespace {

AABB TransformAABB(const AABB& box, const glm::mat4& transform) {
    if (!box.IsValid()) return {};
    const glm::vec3& lo = box.GetMin();
    const glm::vec3& hi = box.GetMax();

    AABB result;
    for (int i = 0; i < 8; ++i) {
        glm::vec3 corner{
            (i & 1) ? hi.x : lo.x,
            (i & 2) ? hi.y : lo.y,
            (i & 4) ? hi.z : lo.z,
        };
        glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
        result.ExpandToInclude(glm::vec3(transformed));
    }
    return result;
}

} // namespace

std::optional<RayHit> RaycastScene(const Ray& ray,
                                   const Scene& scene,
                                   std::span<const SceneMesh> meshes) {
    std::optional<RayHit> best;
    size_t nodeCount = scene.GetNodeCount();

    // Baked-mesh path: meshes whose triangleOwners are set are assumed to be in
    // world-space already (output of SceneBuilder batching), and each triangle
    // carries the NodeId of the IFC element it came from. Iterate triangles once.
    std::vector<bool> bakedMesh(meshes.size(), false);
    for (size_t handle = 0; handle < meshes.size(); ++handle) {
        const SceneMesh& mesh = meshes[handle];
        const auto& owners = mesh.GetTriangleOwners();
        if (owners.empty()) {
            continue;
        }
        bakedMesh[handle] = true;

        const auto& positions = mesh.GetPositions();
        const auto& indices = mesh.GetIndices();
        size_t triangleCount = indices.size() / 3;
        for (size_t tri = 0; tri < triangleCount; ++tri) {
            if (tri >= owners.size()) break;
            NodeId owner = owners[tri];
            if (owner >= nodeCount) continue;
            const SceneNode& ownerNode = scene.GetNode(owner);
            if (!ownerNode.visible) continue;

            const glm::vec3& v0 = positions[indices[tri * 3]];
            const glm::vec3& v1 = positions[indices[tri * 3 + 1]];
            const glm::vec3& v2 = positions[indices[tri * 3 + 2]];

            auto t = RayTriangleIntersect(ray, v0, v1, v2);
            if (!t.has_value()) continue;
            if (best.has_value() && *t >= best->t) continue;

            RayHit hit;
            hit.nodeId = owner;
            hit.t = *t;
            hit.point = ray.origin + ray.direction * (*t);
            hit.triV0 = v0;
            hit.triV1 = v1;
            hit.triV2 = v2;
            best = hit;
        }
    }

    // Attached-mesh path: legacy per-node iteration for meshes without owners.
    for (NodeId id = 0; id < nodeCount; ++id) {
        const SceneNode& node = scene.GetNode(id);
        if (!node.visible || !node.mesh.has_value()) {
            continue;
        }
        MeshHandle handle = *node.mesh;
        if (handle >= meshes.size() || bakedMesh[handle]) {
            continue;
        }

        AABB worldBounds = TransformAABB(node.bounds, node.transform);
        auto aabbT = RayAABBIntersect(ray, worldBounds);
        if (!aabbT.has_value()) {
            continue;
        }
        if (best.has_value() && *aabbT > best->t) {
            continue;
        }

        const SceneMesh& mesh = meshes[handle];
        const auto& positions = mesh.GetPositions();
        const auto& indices = mesh.GetIndices();

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            glm::vec3 v0 = glm::vec3(node.transform * glm::vec4(positions[indices[i]], 1.0f));
            glm::vec3 v1 = glm::vec3(node.transform * glm::vec4(positions[indices[i + 1]], 1.0f));
            glm::vec3 v2 = glm::vec3(node.transform * glm::vec4(positions[indices[i + 2]], 1.0f));

            auto t = RayTriangleIntersect(ray, v0, v1, v2);
            if (!t.has_value()) continue;
            if (best.has_value() && *t >= best->t) continue;

            RayHit hit;
            hit.nodeId = id;
            hit.t = *t;
            hit.point = ray.origin + ray.direction * (*t);
            hit.triV0 = v0;
            hit.triV1 = v1;
            hit.triV2 = v2;
            best = hit;
        }
    }

    return best;
}

} // namespace bimeup::scene
