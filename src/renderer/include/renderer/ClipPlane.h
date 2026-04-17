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
    bool sectionFill{false};
    // Per-plane tint multiplier for the section-fill pipeline. Default 1s so
    // caps render at the element's own color (SectionCapGeometry does
    // ownerColor * fillColor). 8.3e retired the UI surface that edited this;
    // the field stays on the struct for potential future re-exposure.
    glm::vec4 fillColor{1.0F, 1.0F, 1.0F, 1.0F};

    static ClipPlane FromPointNormal(const glm::vec3& point, const glm::vec3& normal);
};

float SignedDistance(const ClipPlane& plane, const glm::vec3& point);

PointSide ClassifyPoint(const ClipPlane& plane, const glm::vec3& point, float epsilon = 1e-4F);

// Build a transform whose translation is the point on the plane closest to the
// origin and whose Z axis is aligned with the plane normal. Useful as input to
// ImGuizmo::Manipulate.
glm::mat4 PlaneToTransform(const ClipPlane& plane);

// Recover a clip plane from a transform: normal = transform's Z axis,
// d = -dot(normal, translation). Preserves the `enabled` default (true).
ClipPlane TransformToPlane(const glm::mat4& transform);

}  // namespace bimeup::renderer
