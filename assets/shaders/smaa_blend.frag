#version 450

// SMAA 1x neighborhood blending (RP.11b.4). Ported from
// `SMAANeighborhoodBlendingPS` in `external/smaa/SMAA.hlsl` — reads the
// input LDR colour plus the per-axis blend weights produced by
// `smaa_weights.frag` (RP.11b.3), and blends toward the dominant
// horizontal-or-vertical neighbour using hardware bilinear filtering
// (SMAA's @BILINEAR_SAMPLING trick: offset by a fractional-pixel amount
// so the single sample averages the two relevant taps).
//
// Bindings (set 0):
//   0 = input LDR colour (from tonemap, pre-AA)
//   1 = blend weights (RGBA8, output of smaa_weights.frag)
//
// `smaa.vert` is the shared fullscreen-triangle vert — the right/bottom
// neighbour offsets that the HLSL reference computes in VS are folded
// into this FS instead, same approach as RP.11b.3.

layout(set = 0, binding = 0) uniform sampler2D colorTex;
layout(set = 0, binding = 1) uniform sampler2D blendTex;

layout(push_constant) uniform Push {
    vec2 rcpFrame;  // (1/width, 1/height)
    // RP.11c — SMAA enable/disable. When 0, the pass short-circuits to a
    // passthrough of the LDR input before the weights sample so a stale
    // weights buffer (from a previously-enabled frame) can't introduce
    // blending on a frame the user has toggled AA off for.
    float enabled;
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 texcoord = inUv;

    if (pc.enabled < 0.5) {
        outColor = textureLod(colorTex, texcoord, 0.0);
        return;
    }

    // HLSL: offset = mad(SMAA_RT_METRICS.xyxy, vec4(1,0,0,1), texcoord.xyxy)
    //   → offset.xy = texcoord + (rcpFrame.x, 0)  — right neighbour
    //   → offset.zw = texcoord + (0, rcpFrame.y)  — bottom neighbour
    vec4 offset = vec4(1.0, 0.0, 0.0, 1.0) * pc.rcpFrame.xyxy + texcoord.xyxy;

    // Gather the four per-axis blend weights:
    //   a.x = right  (alpha of right neighbour's entry)
    //   a.y = top    (green of top neighbour's entry — sampled via offset.zw,
    //                 which looks "wrong" but matches the HLSL reference: the
    //                 weights layout packs horizontal-pair weights in .ra and
    //                 vertical-pair weights in .gb, and the top/bottom channels
    //                 live in the neighbour's sample — see SMAA paper §3.3)
    //   a.w = bottom, a.z = left
    vec4 a;
    a.x = textureLod(blendTex, offset.xy, 0.0).a;
    a.y = textureLod(blendTex, offset.zw, 0.0).g;
    a.wz = textureLod(blendTex, texcoord, 0.0).xz;

    if (dot(a, vec4(1.0)) < 1e-5) {
        // No weight anywhere — passthrough. `textureLod(..., 0.0)` mirrors
        // the HLSL SMAASampleLevelZero.
        outColor = textureLod(colorTex, texcoord, 0.0);
        return;
    }

    // Pick the dominant axis (horizontal vs. vertical).
    bool h = max(a.x, a.z) > max(a.y, a.w);

    // SMAAMovc(cond, var, newVal): per-component select. GLSL `mix(a, b, t)`
    // with `t` as a {0,1} bool cast gives the same "if cond then b else a".
    vec4 blendingOffset = vec4(0.0, a.y, 0.0, a.w);
    vec2 blendingWeight = a.yw;
    blendingOffset = mix(blendingOffset, vec4(a.x, 0.0, a.z, 0.0), float(h));
    blendingWeight = mix(blendingWeight, a.xz, float(h));
    blendingWeight /= dot(blendingWeight, vec2(1.0));

    // HLSL: mad(blendingOffset, vec4(rcpFrame.xy, -rcpFrame.xy), texcoord.xyxy)
    // — push the UV a fractional pixel into the dominant neighbour so hardware
    // bilinear blends the current pixel with that neighbour in one sample.
    vec4 blendingCoord = blendingOffset * vec4(pc.rcpFrame, -pc.rcpFrame) + texcoord.xyxy;

    vec4 color = blendingWeight.x * textureLod(colorTex, blendingCoord.xy, 0.0);
    color += blendingWeight.y * textureLod(colorTex, blendingCoord.zw, 0.0);

    outColor = color;
}
