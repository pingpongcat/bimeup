#pragma once

#include <array>
#include <cstdint>
#include <span>
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

/// Stitch coplanar segments into closed polygons. Endpoints within `epsilon`
/// are treated as the same point (snapped via a quantised grid). Each output
/// polygon is the ordered list of unique vertices around a closed loop (the
/// closing edge from last back to first is implicit). Open polylines (gaps in
/// the input) are discarded.
std::vector<std::vector<glm::vec3>> StitchSegments(
    std::span<const Segment> segments, float epsilon = 1e-4F);

/// Ear-clip a planar polygon into triangles. The polygon is projected to the
/// 2D plane dominant to `planeNormal` (the axis with the largest normal
/// component is dropped). Winding is detected from the 2D signed area, so
/// either CW or CCW input works. Returns a flat vector of triangle vertices
/// (size is a multiple of 3); output vertices are the input's original 3D
/// positions. Degenerate input (< 3 verts) returns empty.
std::vector<glm::vec3> TriangulatePolygon(const std::vector<glm::vec3>& polygon,
                                          const glm::vec3& planeNormal);

}  // namespace bimeup::scene
