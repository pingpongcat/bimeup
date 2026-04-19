#pragma once

namespace bimeup::renderer {

// Linear distance-fog factor in [0, 1] for a pixel at view-space distance
// `viewZ` (positive units in front of the camera). Returns 0 when `viewZ <=
// start`, 1 when `viewZ >= end`, and a linear ramp `(viewZ - start) / (end -
// start)` between — the value `tonemap.frag` feeds into `mix(colour,
// fogColour, factor)` to paint distant geometry with the fog tint. CPU
// mirror of the GLSL `computeFog()` (RP.9b) so tests can pin the formula
// and the degenerate-range guard before any shader code runs.
//
// Degenerate range: when `end - start` is ≤ 1e-6 (panel sliders dragged
// together), degrades to a step function at `start` — `viewZ < start`
// returns 0, `viewZ >= start` returns 1. Avoids division-by-zero producing
// INF/NaN in the shader path.
float ComputeFog(float viewZ, float start, float end);

}  // namespace bimeup::renderer
