#version 450

// Narkowicz ACES fit — mirrors `bimeup::renderer::AcesTonemap` in
// renderer/Tonemap.{h,cpp}. Negatives are clamped before the curve (the
// rational form is non-monotonic below zero) and the result is clamped to
// [0,1] since the sRGB swapchain does the final encode.
//
// Composite order before ACES:
//   RP.5d: multiply the half-res SSAO term (binding 1) into the HDR colour.
//          AO A is cleared to 1.0, so the multiply is a no-op when SSAO is
//          gated off.
//
// RP.12a retired the bloom composite that previously consumed binding 4.
// RP.13a retired the SSIL composite that consumed binding 2 and shifted
// the depth pyramid down to binding 2.
// RP.13b retired the distance-fog composite — the `computeFog` helper,
// and the `fogColorEnabled`/`fogStart`/`fogEnd` push-constant fields are
// gone.
// Stage 9.8.a reclaimed binding 2 for the RT AO image. `useRtAo` selects
// the AO source — 0 samples XeGTAO (bit-compatible raster path), 1
// samples the RT AO pass. RenderLoop updates binding 2 to point at the
// RT AO view when Hybrid RT is active on an RT-capable device; otherwise
// binding 2 falls back to the XeGTAO AO view so an accidental flag of 1
// in raster mode still degenerates to a no-op.

layout(set = 0, binding = 0) uniform sampler2D hdrTexture;
layout(set = 0, binding = 1) uniform sampler2D aoTexture;
layout(set = 0, binding = 2) uniform sampler2D rtAoTexture;

layout(push_constant) uniform PushConstants {
    float exposure;  // pre-ACES linear scale on composited HDR
    float useRtAo;   // 0 = XeGTAO sample, 1 = RT AO sample (Stage 9.8.a)
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

vec3 aces(vec3 x) {
    x = max(x, vec3(0.0));
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3(0.0), vec3(1.0));
}

void main() {
    vec3 hdr = texture(hdrTexture, inUv).rgb;
    float aoXe = texture(aoTexture, inUv).r;
    float aoRt = texture(rtAoTexture, inUv).r;
    float ao = mix(aoXe, aoRt, pc.useRtAo);
    vec3 lit = hdr * ao;

    outColor = vec4(aces(lit * pc.exposure), 1.0);
}
