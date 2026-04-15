#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "renderer/ClipPlane.h"

namespace bimeup::scene {

class SceneMesh;

struct TriangleCut {
    std::uint8_t pointCount{0};        // 0 or 2 (single-point touches are dropped)
    std::array<glm::vec3, 2> points{};  // world-space intersection points
};

struct Segment {
    glm::vec3 a;
    glm::vec3 b;
};

TriangleCut SliceTriangle(const renderer::ClipPlane& plane,
                          const glm::vec3& a,
                          const glm::vec3& b,
                          const glm::vec3& c);

/// Walk every triangle of a SceneMesh, transform positions by worldTransform,
/// intersect against the plane. Returns world-space segments (pointCount==2).
std::vector<Segment> SliceSceneMesh(const SceneMesh& mesh,
                                    const glm::mat4& worldTransform,
                                    const renderer::ClipPlane& plane);

}  // namespace bimeup::scene
