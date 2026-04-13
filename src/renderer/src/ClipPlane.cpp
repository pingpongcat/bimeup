#include "renderer/ClipPlane.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

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

glm::mat4 PlaneToTransform(const ClipPlane& plane) {
    const glm::vec3 n = glm::normalize(glm::vec3{plane.equation});
    const glm::vec3 origin = -plane.equation.w * n;  // closest point on plane to world origin
    // Rotate +Z to n. glm::rotation handles the antiparallel case via a stable axis.
    const glm::quat q = glm::rotation(glm::vec3{0.0F, 0.0F, 1.0F}, n);
    glm::mat4 m = glm::mat4_cast(q);
    m[3] = glm::vec4(origin, 1.0F);
    return m;
}

ClipPlane TransformToPlane(const glm::mat4& transform) {
    const glm::vec3 n = glm::normalize(glm::vec3{transform[2]});  // Z column
    const glm::vec3 t = glm::vec3{transform[3]};                   // translation
    return ClipPlane{glm::vec4{n, -glm::dot(n, t)}, true};
}

}  // namespace bimeup::renderer
