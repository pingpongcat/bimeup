#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

// Narkowicz ACES-fitted tonemap curve, per-channel.
//   f(x) = clamp((x*(2.51*x + 0.03)) / (x*(2.43*x + 0.59) + 0.14), 0, 1)
// Negative inputs are clamped to 0 before the curve (the rational form is
// non-monotonic below zero). CPU mirror of the `tonemap.frag` pass. Input
// is HDR linear; output is display-referred linear in [0, 1] (gamma is
// applied by the sRGB swapchain format).
glm::vec3 AcesTonemap(const glm::vec3& hdrLinear);

}  // namespace bimeup::renderer
