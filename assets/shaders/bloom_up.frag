#version 450

// Bloom dual-filter tent upsample (RP.10b) — Marius Bjørge, GDC 2015.
// Combines 8 samples from the smaller source mip — 4 cardinal + 4
// diagonal neighbours at ±1 source pixel — into one pixel of the
// double-resolution target mip. No centre tap: the RP.10c composite
// stage adds the upsampled output onto the higher mip, and a centre
// term would double-count.
//
// `intensity` is 1.0 for intermediate upsamples inside the pyramid and
// the user-picked `bloomIntensity` panel value only for the final
// composite into the tonemap pass. Byte-for-byte mirror of
// `renderer::BloomUpsample` in `renderer/BloomMath.cpp`.

layout(set = 0, binding = 0) uniform sampler2D sourceTexture;

layout(push_constant) uniform Push {
    vec2 rcpSrcSize;   // (1/srcWidth, 1/srcHeight) — source texel size
    float intensity;   // final-composite scale, 1.0 for intermediate passes
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main() {
    // 4 cardinals (weight 1/12 each) + 4 diagonals (weight 2/12 each) —
    // a 3×3 tent centred on the target pixel, weights sum to 1 so a flat
    // source mip upsamples to itself.
    vec3 t  = texture(sourceTexture, inUv + vec2( 0.0,              pc.rcpSrcSize.y)).rgb;
    vec3 b  = texture(sourceTexture, inUv + vec2( 0.0,             -pc.rcpSrcSize.y)).rgb;
    vec3 l  = texture(sourceTexture, inUv + vec2(-pc.rcpSrcSize.x,  0.0)).rgb;
    vec3 r  = texture(sourceTexture, inUv + vec2( pc.rcpSrcSize.x,  0.0)).rgb;
    vec3 tl = texture(sourceTexture, inUv + vec2(-pc.rcpSrcSize.x,  pc.rcpSrcSize.y)).rgb;
    vec3 tr = texture(sourceTexture, inUv + vec2( pc.rcpSrcSize.x,  pc.rcpSrcSize.y)).rgb;
    vec3 bl = texture(sourceTexture, inUv + vec2(-pc.rcpSrcSize.x, -pc.rcpSrcSize.y)).rgb;
    vec3 br = texture(sourceTexture, inUv + vec2( pc.rcpSrcSize.x, -pc.rcpSrcSize.y)).rgb;

    vec3 result = (t + b + l + r + 2.0 * (tl + tr + bl + br)) / 12.0;
    outColor = vec4(result * pc.intensity, 1.0);
}
