#version 450

// Bloom dual-filter downsample (RP.10b) — Marius Bjørge, GDC 2015.
// Combines 5 samples from the source (centre + 4 diagonals at ±1 source
// pixel) into one pixel of the half-resolution target mip. When
// `applyPrefilter != 0` (first downsample only: HDR scene → bloom mip 0),
// each sample is first passed through a Jorge Jimenez / COD soft-knee
// threshold so sub-threshold scene light drops out of the bloom pyramid.
//
// Byte-for-byte mirror of `renderer::BloomPrefilter` + `BloomDownsample`
// in `renderer/BloomMath.cpp` — weights, epsilons, and clamp bounds all
// pinned by unit tests in `tests/renderer/BloomMathTest.cpp` (RP.10a)
// so this shader can't drift silently from the CPU contract.

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;

layout(push_constant) uniform Push {
    vec2 rcpSrcSize;      // (1/srcWidth, 1/srcHeight) — source texel size
    float threshold;      // prefilter cutoff; ignored when applyPrefilter==0
    float knee;           // half-width of the soft-knee transition region
    int applyPrefilter;   // 0 = mip→mip downsample, 1 = scene→mip0 with knee
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

vec3 prefilter(vec3 color) {
    // Max-channel luma rather than Rec.709: a saturated red light (2, 0, 0)
    // should bloom even though its Rec.709 luma ≈ 0.43 would reject it.
    float luma = max(color.r, max(color.g, color.b));
    // Soft-knee ramp: clamp length 2*knee centred on threshold, squared so
    // the ramp joins the linear regime smoothly at threshold + knee.
    float softness = clamp(luma - pc.threshold + pc.knee, 0.0, 2.0 * pc.knee);
    softness = (softness * softness) / (4.0 * pc.knee + 1e-4);
    // Above threshold + knee the hard branch `luma - threshold` dominates.
    float contribution = max(softness, luma - pc.threshold) / max(luma, 1e-4);
    return color * contribution;
}

void main() {
    // 5-tap dual-filter: centre + 4 diagonals at ±1 pixel in source space.
    vec3 c  = texture(sourceTexture, inUv).rgb;
    vec3 tl = texture(sourceTexture, inUv + vec2(-pc.rcpSrcSize.x,  pc.rcpSrcSize.y)).rgb;
    vec3 tr = texture(sourceTexture, inUv + vec2( pc.rcpSrcSize.x,  pc.rcpSrcSize.y)).rgb;
    vec3 bl = texture(sourceTexture, inUv + vec2(-pc.rcpSrcSize.x, -pc.rcpSrcSize.y)).rgb;
    vec3 br = texture(sourceTexture, inUv + vec2( pc.rcpSrcSize.x, -pc.rcpSrcSize.y)).rgb;

    if (pc.applyPrefilter != 0) {
        c  = prefilter(c);
        tl = prefilter(tl);
        tr = prefilter(tr);
        bl = prefilter(bl);
        br = prefilter(br);
    }

    // Bjørge weights: centre 4/8, each diagonal 1/8 — sum = 1.
    vec3 result = (4.0 * c + tl + tr + bl + br) / 8.0;
    outColor = vec4(result, 1.0);
}
