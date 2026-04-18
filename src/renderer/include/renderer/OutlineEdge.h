#pragma once

#include <array>
#include <cstdint>

namespace bimeup::renderer {

// Classic 3x3 Sobel gradient magnitude for screen-space outline detection. The
// 9 patch samples are row-major with the center at index 4:
//   [0] [1] [2]
//   [3] [4] [5]
//   [6] [7] [8]
//
// Returns sqrt(Gx^2 + Gy^2) where Gx/Gy use the standard Sobel kernels
// ([-1,0,1;-2,0,2;-1,0,1] and [-1,-2,-1;0,0,0;1,2,1]). CPU mirror of the
// `outline.frag` path that will land with RP.6b — input will be linear depth
// (discontinuity fallback) or a per-object id (when stencil matches don't
// resolve an edge cleanly).
float SobelMagnitude(const std::array<float, 9>& patch);

// Detect a stencil-id boundary in a 3x3 patch of outline-stencil values
// (0 = nothing, 1 = selected, 2 = hovered — written by the main pass per
// RP.6c). Returns 0 when the patch is uniform (interior of a region or empty
// background), otherwise the max stencil id present in the window. Hover (2)
// beats selected (1) by construction so a cursor hovering over an already-
// selected element still shows the hover colour. CPU mirror of the
// stencil-edge branch of `outline.frag`.
std::uint8_t EdgeFromStencil(const std::array<std::uint8_t, 9>& patch);

}  // namespace bimeup::renderer
