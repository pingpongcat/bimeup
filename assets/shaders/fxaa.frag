#version 450

// FXAA post-process (RP.8b). Edge-aware screen-space anti-aliasing derived
// from NVIDIA FXAA 3.11 (Timothy Lottes, 2011) — simplified to the core
// early-exit + edge-orientation + neighbour-blend path since BIM scenes
// produce mostly polygon-silhouette aliasing, which is handled well without
// the full edge-walk search.
//
// Runs after tonemap + outline in the present pass on an LDR colour input
// (set 0, binding 0). `luma()` matches `renderer::FxaaLuminance` (Rec.709)
// and the early-exit predicate matches `renderer::FxaaIsEdge` — both pinned
// by CPU unit tests in RP.8a so this shader can't drift silently.

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(push_constant) uniform Push {
    vec2 rcpFrame;            // (1/width, 1/height), frame-space texel size
    float subpixel;           // 0..1, amount of sub-pixel AA applied (HIGH only)
    float edgeThreshold;      // relative edge gate, default 0.166
    float edgeThresholdMin;   // absolute edge gate floor, default 0.0833
    int quality;              // 0 = LOW (edge-only), 1 = HIGH (edge + subpixel)
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

float luma(vec3 rgb) {
    // Rec.709 — byte-for-byte mirror of `renderer::FxaaLuminance` in
    // `renderer/FxaaMath.cpp`. The weights sum to 1 exactly so grayscale is
    // a fixed point (no false-positive edges on flat grey regions).
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    // 5-tap NESW+centre cross.
    vec3 rgbC = texture(inputTexture, inUv).rgb;
    vec3 rgbN = texture(inputTexture, inUv + vec2( 0.0, -pc.rcpFrame.y)).rgb;
    vec3 rgbS = texture(inputTexture, inUv + vec2( 0.0,  pc.rcpFrame.y)).rgb;
    vec3 rgbE = texture(inputTexture, inUv + vec2( pc.rcpFrame.x, 0.0)).rgb;
    vec3 rgbW = texture(inputTexture, inUv + vec2(-pc.rcpFrame.x, 0.0)).rgb;

    float lC = luma(rgbC);
    float lN = luma(rgbN);
    float lS = luma(rgbS);
    float lE = luma(rgbE);
    float lW = luma(rgbW);

    float lumaMax = max(lC, max(max(lN, lS), max(lE, lW)));
    float lumaMin = min(lC, min(min(lN, lS), min(lE, lW)));
    float range = lumaMax - lumaMin;

    // Early-exit: not an edge. Mirror of `renderer::FxaaIsEdge` negated.
    if (range < max(pc.edgeThresholdMin, lumaMax * pc.edgeThreshold)) {
        outColor = vec4(rgbC, 1.0);
        return;
    }

    // Edge orientation: perpendicular-gradient dominates. A horizontal edge
    // (pixels stacked top/bottom with different colours) shows a big vertical
    // luma gradient (|lN - lS|), so we blend with the north/south neighbour;
    // a vertical edge is the mirror case.
    float gradH = abs(lE - lW);
    float gradV = abs(lN - lS);
    bool horizontalEdge = gradV >= gradH;

    // Pick the neighbour on the side of the edge with the bigger luma jump —
    // that's the "other side" of the edge; blending centre toward it reduces
    // the stair-step artefact.
    float neighbourLuma;
    vec3 neighbourRgb;
    if (horizontalEdge) {
        bool pickNorth = abs(lN - lC) >= abs(lS - lC);
        neighbourLuma = pickNorth ? lN : lS;
        neighbourRgb = pickNorth ? rgbN : rgbS;
    } else {
        bool pickEast = abs(lE - lC) >= abs(lW - lC);
        neighbourLuma = pickEast ? lE : lW;
        neighbourRgb = pickEast ? rgbE : rgbW;
    }

    // 50/50 edge blend — the canonical FXAA output at the end of the
    // edge-detection stage.
    vec3 edgeBlend = mix(rgbC, neighbourRgb, 0.5);

    // HIGH quality adds a sub-pixel contribution: when the 3×3 neighbourhood
    // is smoothly graded (centre luma near the average of the 4 neighbours)
    // we lerp toward the 5-tap average, which softens sub-pixel features that
    // the edge-only path leaves shimmering.
    if (pc.quality >= 1) {
        vec3 avgRgb = (rgbN + rgbS + rgbE + rgbW + rgbC) * 0.2;
        float avgLuma = (lN + lS + lE + lW) * 0.25;
        // `subpixAmount` peaks when the centre sits near the neighbour average
        // (smooth gradient) and falls off on sharp edges.
        float subpixAmount = clamp(1.0 - abs(avgLuma - lC) / max(range, 1e-4), 0.0, 1.0);
        subpixAmount *= pc.subpixel;
        edgeBlend = mix(edgeBlend, avgRgb, subpixAmount);
    }

    outColor = vec4(edgeBlend, 1.0);
}
