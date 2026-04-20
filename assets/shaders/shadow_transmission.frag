#version 450

// RP.18.2 — writes tinted sun attenuation for transparent geometry into the
// shadow pass's R16G16B16A16_SFLOAT transmission attachment. The CPU side pushes
// `glassTint = surfaceColor.rgb * (1 - effectiveAlpha)` — i.e. the colour that
// survives one pane of glass. The pipeline's blend state is `VK_BLEND_OP_MIN`
// so overlapping panes compose as the darkest/most-tinted tap (a fast
// approximation of the true multiplicative attenuation). Alpha is forced to 1
// so the min-blend never narrows the alpha channel toward 0.
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 glassTint;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(push.glassTint.rgb, 1.0);
}
