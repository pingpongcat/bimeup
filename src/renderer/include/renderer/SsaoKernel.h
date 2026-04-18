#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace bimeup::renderer {

// Generate `count` hemisphere-kernel samples for classic SSAO (Chapman 2013 /
// LearnOpenGL). Each sample is a 3D offset in the +z unit hemisphere; samples
// are rescaled by a quadratic weight `lerp(0.1, 1.0, (i/N)^2)` so early
// samples cluster near the fragment origin (contact AO bias). The shader
// rotates each sample into the fragment's TBN basis at runtime. `seed`
// produces a deterministic sequence so the CPU mirror and GPU upload stay in
// lock-step. CPU mirror of the kernel table that `ssao_main.comp` (RP.5b)
// will consume as a UBO.
std::vector<glm::vec3> GenerateHemisphereKernel(std::size_t count,
                                                std::uint32_t seed = 0);

// Pack four edge weights in [0, 1] into a single byte — 2 bits per channel,
// quantised to the {0, 1/3, 2/3, 1} lattice, in bit order
// (L: 0-1, R: 2-3, T: 4-5, B: 6-7). Inputs outside [0, 1] are saturated.
// ASSAO-style: `ssao_main.comp` writes per-tap edge weights into a companion
// R8 edge target; `ssao_blur.comp` reads them back to respect depth/normal
// discontinuities when blurring AO. Intentionally lossy — 4 edge levels is
// enough weight for a 2-pass separable blur and keeps the edge buffer R8.
std::uint8_t PackEdges(float left, float right, float top, float bottom);

// Inverse of `PackEdges`. Returns the four quantised weights (x=left,
// y=right, z=top, w=bottom) in [0, 1].
glm::vec4 UnpackEdges(std::uint8_t packed);

}  // namespace bimeup::renderer
