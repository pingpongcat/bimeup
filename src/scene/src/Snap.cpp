#include "scene/Snap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>


namespace bimeup::scene {

namespace {

constexpr float kEps = 1e-12f;

float DistanceSq(const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 d = a - b;
    return glm::dot(d, d);
}

glm::vec3 ClosestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 ab = b - a;
    const float denom = glm::dot(ab, ab);
    if (denom < kEps) {
        return a;
    }
    const float t = std::clamp(glm::dot(p - a, ab) / denom, 0.0f, 1.0f);
    return a + t * ab;
}

// Closest point on triangle (v0,v1,v2) to p. Standard Real-Time Collision
// Detection algorithm (Ericson §5.1.5).
glm::vec3 ClosestPointOnTriangle(const glm::vec3& p,
                                 const glm::vec3& a,
                                 const glm::vec3& b,
                                 const glm::vec3& c) {
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;

    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a;
    }

    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b;
    }

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c;
    }

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    const float denom = 1.0f / (va + vb + vc);
    const float v = vb * denom;
    const float w = vc * denom;
    return a + ab * v + ac * w;
}

} // namespace

SnapResult SnapToVertex(const glm::vec3& query,
                        std::span<const glm::vec3> vertices,
                        float threshold) {
    SnapResult best;
    float bestDistSq = threshold * threshold;
    for (const auto& v : vertices) {
        const float d2 = DistanceSq(query,v);
        if (d2 <= bestDistSq) {
            bestDistSq = d2;
            best.type = SnapType::Vertex;
            best.point = v;
            best.distance = std::sqrt(d2);
        }
    }
    return best;
}

SnapResult SnapToEdge(const glm::vec3& query,
                      std::span<const glm::vec3> vertices,
                      std::span<const std::uint32_t> indices,
                      float threshold) {
    SnapResult best;
    float bestDistSq = threshold * threshold;
    const std::size_t triCount = indices.size() / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t i0 = indices[3 * t + 0];
        const std::uint32_t i1 = indices[3 * t + 1];
        const std::uint32_t i2 = indices[3 * t + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }
        const glm::vec3& v0 = vertices[i0];
        const glm::vec3& v1 = vertices[i1];
        const glm::vec3& v2 = vertices[i2];
        const std::array<std::pair<glm::vec3, glm::vec3>, 3> edges{{
            {v0, v1}, {v1, v2}, {v2, v0},
        }};
        for (const auto& [a, b] : edges) {
            const glm::vec3 cp = ClosestPointOnSegment(query, a, b);
            const float d2 = DistanceSq(query,cp);
            if (d2 <= bestDistSq) {
                bestDistSq = d2;
                best.type = SnapType::Edge;
                best.point = cp;
                best.distance = std::sqrt(d2);
            }
        }
    }
    return best;
}

SnapResult SnapToFace(const glm::vec3& query,
                      std::span<const glm::vec3> vertices,
                      std::span<const std::uint32_t> indices,
                      float threshold) {
    SnapResult best;
    float bestDistSq = threshold * threshold;
    const std::size_t triCount = indices.size() / 3;
    for (std::size_t t = 0; t < triCount; ++t) {
        const std::uint32_t i0 = indices[3 * t + 0];
        const std::uint32_t i1 = indices[3 * t + 1];
        const std::uint32_t i2 = indices[3 * t + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            continue;
        }
        const glm::vec3 cp =
            ClosestPointOnTriangle(query, vertices[i0], vertices[i1], vertices[i2]);
        const float d2 = DistanceSq(query,cp);
        if (d2 <= bestDistSq) {
            bestDistSq = d2;
            best.type = SnapType::Face;
            best.point = cp;
            best.distance = std::sqrt(d2);
        }
    }
    return best;
}

SnapResult Snap(const glm::vec3& query,
                std::span<const glm::vec3> vertices,
                std::span<const std::uint32_t> indices,
                float vertexThreshold,
                float edgeThreshold,
                float faceThreshold) {
    if (auto v = SnapToVertex(query, vertices, vertexThreshold); v.IsValid()) {
        return v;
    }
    if (auto e = SnapToEdge(query, vertices, indices, edgeThreshold); e.IsValid()) {
        return e;
    }
    return SnapToFace(query, vertices, indices, faceThreshold);
}

} // namespace bimeup::scene
