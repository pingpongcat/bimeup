#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

// Rec.709 luminance of a linear-or-tonemapped RGB triplet. FXAA treats luma
// as the edge-detection signal; the choice of weights is not load-bearing as
// long as they're non-zero per channel, but Rec.709 (0.2126, 0.7152, 0.0722)
// is the modern standard and sums to 1 by construction so grayscale passes
// through unchanged. CPU mirror of `fxaa.frag`'s `FxaaLuma()` (RP.8b) so tests
// can pin the weights before any shader code runs.
float FxaaLuminance(const glm::vec3& rgb);

// Local-contrast range across the 5-sample NESW+centre cross used by FXAA's
// early-exit test. Returns `max(lumaC,lumaN,lumaS,lumaE,lumaW) -
// min(lumaC,lumaN,lumaS,lumaE,lumaW)`. The centre sample must participate in
// both reductions — without it, pixels where the centre is the darkest or
// brightest of the cross would be misclassified as flat and miss AA.
float FxaaLocalContrast(float lumaCenter, float lumaNorth, float lumaSouth,
                        float lumaEast, float lumaWest);

// Edge predicate matching FXAA 3.11's early-exit condition. Returns true when
// the 5-sample luma range crosses both the absolute `edgeThresholdMin` floor
// (guards against false-positive edges in dark regions where shot noise
// dominates) and the relative `edgeThreshold` gate (`lumaMax * edgeThreshold`,
// so bright regions need proportionally more contrast to register). Either
// test rejecting the pixel means "not an edge, skip AA" in the shader's
// fast path. CPU mirror of the `fxaa.frag` gate so tests pin the contract.
bool FxaaIsEdge(float lumaCenter, float lumaNorth, float lumaSouth,
                float lumaEast, float lumaWest,
                float edgeThreshold, float edgeThresholdMin);

}  // namespace bimeup::renderer
