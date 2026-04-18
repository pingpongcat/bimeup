#version 450

// Narkowicz ACES fit — mirrors `bimeup::renderer::AcesTonemap` in
// renderer/Tonemap.{h,cpp}. Negatives are clamped before the curve (the
// rational form is non-monotonic below zero) and the result is clamped to
// [0,1] since the sRGB swapchain does the final encode.

layout(set = 0, binding = 0) uniform sampler2D hdrTexture;

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
    outColor = vec4(aces(hdr), 1.0);
}
