#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

/// Compute a directional-light view-projection matrix ("light space matrix") that
/// tightly bounds a sphere of radius `sceneRadius` centered at `sceneCenter`.
///
/// `lightDirection` is the direction light *travels* (i.e. points FROM the light
/// source, matching the convention in `renderer::DirectionalLight::direction`).
/// It does not need to be normalized.
///
/// The returned matrix maps world-space points into Vulkan clip space with depth
/// range [0, 1] and NDC.xy in [-1, 1] — points inside the bounding sphere land
/// inside the shadow map, and `uv = ndc.xy * 0.5 + 0.5` gives the shadow-map
/// sample coordinate.
[[nodiscard]] glm::mat4 ComputeLightSpaceMatrix(const glm::vec3& lightDirection,
                                                const glm::vec3& sceneCenter,
                                                float sceneRadius);

}  // namespace bimeup::renderer
