#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

// Soft-knee threshold prefilter applied before the bloom downsample chain
// (Jorge Jimenez / Call of Duty Advanced Warfare form). Returns `color`
// scaled by a 0..1 contribution factor so that sub-threshold pixels drop
// out of the bloom pyramid entirely while pixels above threshold pass
// through with a smooth knee — avoids the hard popping of a binary cutoff
// when a single pixel's luma drifts across the threshold between frames.
//
// Luma is `max(r, g, b)` (max-channel) rather than Rec.709 — saturated
// lights (e.g. pure red (2, 0, 0)) should still bloom, and Rec.709 would
// reject them based on their low perceived brightness. CPU mirror of the
// GLSL `bloomPrefilter()` planned for `bloom_down.frag` (RP.10b).
glm::vec3 BloomPrefilter(const glm::vec3& color, float threshold, float knee);

// Dual-filter downsample weights (Marius Bjørge, GDC 2015). Combines 5
// samples from the source mip — centre + 4 diagonals at ±half-pixel
// offsets — into a single pixel of the target (half-resolution) mip.
// Weights: centre 4/8, each diagonal 1/8 — sum = 1 so a flat field
// downsamples to itself. CPU mirror of the GLSL planned for
// `bloom_down.frag` (RP.10b).
glm::vec3 BloomDownsample(const glm::vec3& center,
                          const glm::vec3& topLeft, const glm::vec3& topRight,
                          const glm::vec3& bottomLeft, const glm::vec3& bottomRight);

// Dual-filter tent upsample weights (Marius Bjørge, GDC 2015). Combines
// 8 samples from the smaller source mip — 4 cardinal + 4 diagonal
// neighbours — into a single pixel of the target (double-resolution)
// mip. No centre tap: a centre term would double-count when the composite
// stage adds the upsampled result onto the higher mip. Weights: each
// cardinal 1/12, each diagonal 2/12 — sum = 1. CPU mirror of the GLSL
// planned for `bloom_up.frag` (RP.10b).
glm::vec3 BloomUpsample(const glm::vec3& top, const glm::vec3& bottom,
                        const glm::vec3& left, const glm::vec3& right,
                        const glm::vec3& topLeft, const glm::vec3& topRight,
                        const glm::vec3& bottomLeft, const glm::vec3& bottomRight);

}  // namespace bimeup::renderer
