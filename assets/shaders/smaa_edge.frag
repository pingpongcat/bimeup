#version 450

// SMAA 1x luma edge detection (RP.11b.2). Byte-for-byte mirror of the CPU
// predicate `renderer::SmaaDetectEdgeLuma` (RP.11a):
//   1. Rec.709 luma of the current + four 4-neighbours.
//   2. Absolute threshold — reject edges whose left/top delta is below
//      `pc.threshold` (default 0.1, matches SMAA_THRESHOLD).
//   3. Local-contrast adaptation — suppress an edge when a stronger edge
//      nearby dominates the neighbourhood, i.e. keep only while
//      `delta.xy * localContrastFactor >= max(all neighbour deltas)`
//      (default factor 2.0, matches SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR).
// Output is a 2-channel edges texture — R = left-axis edge, G = top-axis
// edge — consumed by `smaa_weights.frag` in RP.11b.3.

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform Push {
    vec2 rcpFrame;              // (1/width, 1/height)
    float threshold;            // absolute edge gate
    float localContrastFactor;  // neighbourhood suppression factor
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec2 outEdges;

float luma(vec3 rgb) {
    // Rec.709 — identical weights to `renderer::SmaaLuminance`.
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    float L = luma(texture(inputTexture, inUv).rgb);
    float Lleft = luma(texture(inputTexture, inUv + vec2(-pc.rcpFrame.x, 0.0)).rgb);
    float Ltop = luma(texture(inputTexture, inUv + vec2(0.0, -pc.rcpFrame.y)).rgb);
    float Lright = luma(texture(inputTexture, inUv + vec2(pc.rcpFrame.x, 0.0)).rgb);
    float Lbottom = luma(texture(inputTexture, inUv + vec2(0.0, pc.rcpFrame.y)).rgb);

    vec2 delta = vec2(abs(L - Lleft), abs(L - Ltop));

    // Absolute-threshold gate: `step(threshold, delta)` is 1 where delta > threshold.
    vec2 edges = step(vec2(pc.threshold), delta);

    // Skip the neighbourhood max + suppression when both axes rejected — saves
    // the two extra abs() on fully flat regions, which dominate BIM frames.
    if (dot(edges, vec2(1.0)) == 0.0) {
        discard;
    }

    // Local-contrast adaptation: compute the max neighbour delta, suppress an
    // edge whose delta doesn't clear `maxDelta / localContrastFactor`.
    // `step(maxDelta, factor * delta)` is 1 when `factor * delta >= maxDelta`,
    // i.e. the edge survives suppression — matches the CPU helper's
    // "suppress if delta * factor < maxNeighbour" predicate.
    float deltaR = abs(L - Lright);
    float deltaB = abs(L - Lbottom);
    float maxDelta = max(max(delta.x, delta.y), max(deltaR, deltaB));
    edges *= step(vec2(maxDelta), pc.localContrastFactor * delta);

    outEdges = edges;
}
