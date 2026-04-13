#include <renderer/ShadowPass.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace bimeup::renderer {

namespace {

// Pick a stable up vector that is not parallel to `forward`. If the light points
// nearly along world +Y/-Y, fall back to +Z so lookAt does not degenerate.
glm::vec3 PickStableUp(const glm::vec3& forward) {
    constexpr float kParallelThreshold = 0.999F;
    if (std::abs(forward.y) > kParallelThreshold) {
        return glm::vec3(0.0F, 0.0F, 1.0F);
    }
    return glm::vec3(0.0F, 1.0F, 0.0F);
}

}  // namespace

glm::mat4 ComputeLightSpaceMatrix(const glm::vec3& lightDirection,
                                  const glm::vec3& sceneCenter,
                                  float sceneRadius) {
    const glm::vec3 dir = glm::normalize(lightDirection);

    // Place the light "eye" at sceneCenter shifted back along -dir by sceneRadius,
    // so the full bounding sphere sits between near=0 and far=2*sceneRadius.
    const glm::vec3 eye = sceneCenter - dir * sceneRadius;
    const glm::vec3 up = PickStableUp(dir);
    const glm::mat4 view = glm::lookAtRH(eye, sceneCenter, up);

    // Zero-to-one depth range matches Vulkan's clip space.
    const glm::mat4 proj = glm::orthoRH_ZO(-sceneRadius, sceneRadius,
                                           -sceneRadius, sceneRadius,
                                           0.0F, 2.0F * sceneRadius);

    return proj * view;
}

}  // namespace bimeup::renderer
