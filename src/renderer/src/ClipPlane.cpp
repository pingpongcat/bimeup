#include "renderer/ClipPlane.h"

#include <cmath>

namespace bimeup::renderer {

ClipPlane ClipPlane::FromPointNormal(const glm::vec3& point, const glm::vec3& normal) {
    const glm::vec3 n = glm::normalize(normal);
    return ClipPlane{glm::vec4{n, -glm::dot(n, point)}, true};
}

float SignedDistance(const ClipPlane& plane, const glm::vec3& point) {
    return glm::dot(glm::vec3{plane.equation}, point) + plane.equation.w;
}

PointSide ClassifyPoint(const ClipPlane& plane, const glm::vec3& point, float epsilon) {
    const float d = SignedDistance(plane, point);
    if (std::fabs(d) <= epsilon) return PointSide::OnPlane;
    return d > 0.0F ? PointSide::Front : PointSide::Back;
}

}  // namespace bimeup::renderer
