#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace bimeup::renderer {

enum class PointSide : std::uint8_t { Front, Back, OnPlane };

struct ClipPlane {
    // Plane equation: equation.xyz = unit normal, equation.w = d, so that
    // dot(normal, p) + d == 0 for any point p on the plane. A point is on
    // the "front" side (kept by the clip) when that expression is positive.
    glm::vec4 equation{0.0F, 1.0F, 0.0F, 0.0F};
    bool enabled{true};

    static ClipPlane FromPointNormal(const glm::vec3& point, const glm::vec3& normal);
};

float SignedDistance(const ClipPlane& plane, const glm::vec3& point);

PointSide ClassifyPoint(const ClipPlane& plane, const glm::vec3& point, float epsilon = 1e-4F);

}  // namespace bimeup::renderer
