#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

// Reprojection matrix that maps a current-frame clip-space position to the
// corresponding clip-space position in the previous frame's camera. Intended
// for `ssil_main.comp` (RP.7b), which samples the previous-frame HDR target
// at the reprojected UV to gather one-bounce indirect colour.
//
// Derivation: a current clip point back-projects to world via `currInvViewProj`
// and then forward-projects through the previous frame's ViewProj, so the
// combined matrix is `prevViewProj * currInvViewProj`. When prev == curr the
// two cancel to identity (no reprojection needed). CPU mirror so panel /
// frame-index logic can reason about reprojection without binding a shader.
glm::mat4 ComputeReprojectionMatrix(const glm::mat4& prevViewProj,
                                    const glm::mat4& currInvViewProj);

// Weight in [0, 1] that attenuates an SSIL sample by how well the sampled
// pixel's normal aligns with the shaded pixel's normal. Matching normals
// (same surface) return 1; perpendicular or back-facing normals return 0.
// Computed as `max(0, dot(nCurr, nSampled))^strength` — `strength` narrows
// the acceptance lobe, matching the Intel/Godot `ssil.glsl` reference and
// the "normal rejection" panel slider. CPU mirror of the GLSL weight used
// in the compute shader's accumulation loop.
float SsilNormalRejectionWeight(const glm::vec3& currentNormal,
                                const glm::vec3& sampledNormal,
                                float strength);

// Per-channel clamp applied after the 64-tap accumulation in `ssil_main.comp`.
// `cap` comes from the panel "Max luminance" slider (RP.12c) — it prevents
// uniformly-lit walls from driving the indirect colour above the threshold,
// which otherwise reads as wide-area glow rather than colour-bleed. Channels
// are clamped independently so a saturated red can't drag the whole frame
// toward a desaturated cap (a single `clamp(luma)` would shift the hue).
// Lower bound is 0 to defend against negative samples leaking from a stale
// previous-HDR tap. CPU mirror of the GLSL `clamp(indirect, 0, cap)`.
glm::vec3 SsilClampLuminance(const glm::vec3& indirect, float cap);

}  // namespace bimeup::renderer
