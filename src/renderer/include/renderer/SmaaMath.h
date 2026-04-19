#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace bimeup::renderer {

/// Rec.709 luma — matches the SMAA 1x reference edge-detection weighting.
/// Shader mirror: `dot(color, vec3(0.2126, 0.7152, 0.0722))` in `smaa_edge.frag`
/// (RP.11b).
[[nodiscard]] float SmaaLuminance(glm::vec3 rgb);

/// CPU mirror of the SMAA 1x luma-based edge detection predicate (RP.11a).
/// Given a pixel's luma `L` and the luma of its four 4-neighbours, returns
/// per-axis edge flags after applying:
///   1. absolute-threshold gate — the edge-local delta must exceed `threshold`;
///   2. local-contrast adaptation — an edge is suppressed when a much stronger
///      neighbouring edge dominates the region, i.e. kept only while
///      `delta * localContrastFactor >= max(neighbourhood delta)`.
///
/// Defaults match the SMAA reference constants `SMAA_THRESHOLD = 0.1` and
/// `SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR = 2.0`.
[[nodiscard]] glm::bvec2 SmaaDetectEdgeLuma(
    float L,
    float Lleft,
    float Ltop,
    float Lright,
    float Lbottom,
    float threshold = 0.1F,
    float localContrastFactor = 2.0F);

}  // namespace bimeup::renderer
