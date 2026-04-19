#version 450

// Narkowicz ACES fit — mirrors `bimeup::renderer::AcesTonemap` in
// renderer/Tonemap.{h,cpp}. Negatives are clamped before the curve (the
// rational form is non-monotonic below zero) and the result is clamped to
// [0,1] since the sRGB swapchain does the final encode.
//
// Composite order before ACES:
//   RP.7d: add the half-res SSIL indirect colour (binding 2) into the HDR.
//          SSIL A is cleared to 0 at creation time and stays at 0 when the
//          pass is gated off, so the add is a no-op then.
//   RP.5d: multiply the half-res SSAO term (binding 1) into the composited
//          HDR. AO A is cleared to 1.0, so the multiply is a no-op when
//          SSAO is gated off.
//   RP.9:  linear distance fog `mix(lit, fogColor, factor)` where
//          `factor = computeFog(viewZ, start, end)` — applied pre-ACES so
//          the panel-picked fog colour goes through the same tonemap curve
//          as the sky / scene. Gated by `pc.fogColorEnabled.w > 0.5`;
//          under MSAA the depth pyramid binding 3 is undefined (pyramid
//          build is gated off) so RenderLoop forces w = 0 in that mode.
//
// RP.12a retired the bloom composite that previously consumed binding 4
// and the bloomIntensity / bloomEnabled push-constant fields.

layout(set = 0, binding = 0) uniform sampler2D hdrTexture;
layout(set = 0, binding = 1) uniform sampler2D aoTexture;
layout(set = 0, binding = 2) uniform sampler2D ssilTexture;
layout(set = 0, binding = 3) uniform sampler2D linearDepthTexture;

layout(push_constant) uniform PushConstants {
    vec4 fogColorEnabled;  // rgb = colour, w = enabled (0.0 / 1.0)
    float fogStart;
    float fogEnd;
    float exposure;        // pre-ACES linear scale on composited HDR
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

// Byte-for-byte mirror of `renderer::ComputeFog` (RP.9a). The `1e-6` floor
// on `denom` matches the CPU path — panel sliders dragged together
// degrade to a step function at `start` rather than producing INF/NaN.
float computeFog(float viewZ, float start, float end) {
    float denom = end - start;
    if (denom <= 1e-6) {
        return viewZ >= start ? 1.0 : 0.0;
    }
    return clamp((viewZ - start) / denom, 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(hdrTexture, inUv).rgb;
    vec3 ssil = texture(ssilTexture, inUv).rgb;
    float ao = texture(aoTexture, inUv).r;
    vec3 lit = (hdr + ssil) * ao;

    if (pc.fogColorEnabled.w > 0.5) {
        float viewZ = texture(linearDepthTexture, inUv).r;
        float factor = computeFog(viewZ, pc.fogStart, pc.fogEnd);
        lit = mix(lit, pc.fogColorEnabled.rgb, factor);
    }

    outColor = vec4(aces(lit * pc.exposure), 1.0);
}
