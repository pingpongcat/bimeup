#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace bimeup::renderer {

// Screen-space slice direction for XeGTAO horizon integration (Intel
// XeGTAO 2022 / Jimenez GTAO 2016). Slices span [0, π) since
// `(cos φ, sin φ)` and `(-cos φ, -sin φ)` describe the same line and the
// shader walks both sides of each slice. Returns a unit vector
// `(cos φ, sin φ)` with `φ = π · (index + jitter) / sliceCount`.
// `jitter ∈ [0, 1)` is a per-pixel decorrelator (the shader mixes it from
// a blue-noise texture); `jitter = 0` gives the un-rotated base set.
glm::vec2 XeGtaoSliceDirection(std::uint32_t index,
                               std::uint32_t sliceCount,
                               float jitter = 0.0F);

// Per-slice visibility integral from the XeGTAO cosine-lobe form:
//   visArea = (1/4) · ( -cos(2·h1 − n) + cos(n) + 2·h1·sin(n)
//                       -cos(2·h2 − n) + cos(n) + 2·h2·sin(n) )
//   return projectedNormalLen · visArea
//
// Horizons are first clamped to the normal-facing hemisphere:
//   h1 ← max(h1, n − π/2)
//   h2 ← min(h2, n + π/2)
// so horizon rays can't reach behind the surface (mirrors the GLSL clamp
// in `ssao_xegtao.comp`).
//
//   horizonLeft  (h1) — signed angle of the "left" horizon ray from the
//                       view direction in the slice plane, radians.
//   horizonRight (h2) — signed angle of the "right" horizon ray, radians.
//   normalAngle  (n)  — signed angle between the slice-projected normal
//                       and the view direction, radians.
//   projectedNormalLen — length of the normal's projection onto the slice
//                        plane, in [0, 1]; a normal perpendicular to the
//                        slice contributes 0.
//
// Returns the per-slice visibility the GPU pass accumulates — sum over
// `sliceCount` slices, divide to form the final cos-weighted AO fraction.
// Known analytical case (n = 0, symmetric horizons h1 = −θ, h2 = +θ,
// projLen = 1): `visibility = sin²(θ)`.
float XeGtaoSliceVisibility(float horizonLeft, float horizonRight,
                            float normalAngle, float projectedNormalLen);

}  // namespace bimeup::renderer
