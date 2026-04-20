#version 450

// RP.18.2 / RP.18.7 — writes tinted sun attenuation for transparent geometry
// into the shadow pass's R16G16B16A16_SFLOAT transmission attachment. The CPU
// side pushes `glassTint = vec3(1 - effectiveAlpha)` (RP.18.6 — architectural
// glass is near-neutral in transmission, so the surface-colour factor was
// dropped). The pipeline's blend state is `VK_BLEND_OP_MIN` on every channel,
// so overlapping panes compose as the darkest/nearest tap (fast approximation
// of the true multiplicative attenuation).
//
// RP.18.7 — we store the glass fragment's own light-space depth (gl_FragCoord.z)
// in the alpha channel. The main pass reads it back and uses
// `transmit.a < fragLightZ - bias` to gate whether this pixel's tint is
// actually in front of the fragment being shaded. Without this, a 2D
// transmission map leaks sunlight into rooms whose wall-shadowed floors happen
// to share a shadow-map texel with a window in a different room.
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 glassTint;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(push.glassTint.rgb, gl_FragCoord.z);
}
