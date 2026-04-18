#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

// Convert Vulkan-style non-linear depth (z/w in [0, 1], 0 = near, 1 = far) to
// view-space linear depth (positive distance from the camera along -z). For a
// right-handed projection with depth range [0, 1] (Vulkan convention):
//
//     linearDepth = (nearZ * farZ) / (farZ * (1 - z_nl) + nearZ * z_nl)
//
// CPU mirror of the `depth_linearize.comp` pass that will land with RP.4b/c.
float LinearizeDepth(float nonLinearDepth, float nearZ, float farZ);

// Reconstruct view-space position from screen UV and view-space linear depth.
//   - uv: [0, 1]^2 — Vulkan convention (y = 0 at top, y = 1 at bottom).
//   - linearDepth: positive view-space distance (output of `LinearizeDepth`).
//   - invProj: inverse of the projection matrix. If the forward projection
//     carries a Vulkan y-flip (proj[1][1] *= -1), `invProj` must be the
//     inverse of that same flipped matrix.
//
// Implementation rays a uv through `invProj` at any valid NDC z (far plane
// z_nl = 1) and rescales the result so `viewPos.z == -linearDepth`. The
// returned vector is in the same view space the projection maps from (camera
// looks toward -z). CPU mirror of the SSAO helper that RP.5 will use.
//
// Note: the Stage RP PLAN spec lists this helper's third arg as `invViewProj`,
// but that would produce world-space positions; SSAO needs view space, so we
// take `invProj` and document the departure here.
glm::vec3 ReconstructViewPosFromDepth(const glm::vec2& uv,
                                      float linearDepth,
                                      const glm::mat4& invProj);

}  // namespace bimeup::renderer
