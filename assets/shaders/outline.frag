#version 450

// Screen-space selection/hover outline (RP.6b). GLSL mirrors of
// `renderer::EdgeFromStencil` and `renderer::SobelMagnitude` from
// `renderer/OutlineEdge.{h,cpp}` — RP.6a pins down the CPU contract these
// helpers must match, the 3×3 patch layout is row-major with the centre at
// index 4:
//     [0] [1] [2]
//     [3] [4] [5]
//     [6] [7] [8]
//
// The stencil attachment (binding 0, R8_UINT) is written during the main
// pass per RP.6c — low 2 bits hold 0 = background, 1 = selected, 2 = hovered.
// Bit 4 is the RP.12b "transparent surface" flag (OR'd in by the transparent
// draw pipeline) so the same texel can read 0/1/2 or 4/5/6. Mask out bit 4
// before the max-reduction so glass overlays never hijack the edge category;
// the outline still draws on the selection's silhouette regardless of what's
// in front of it. Hover (2) still wins over selected (1) when both appear in
// the 3×3 window.

layout(set = 0, binding = 0) uniform usampler2D stencilTexture;
layout(set = 0, binding = 1) uniform sampler2D linearDepthTexture;

layout(push_constant) uniform Push {
    vec4 selectedColor;       // RGBA, alpha drives the blend
    vec4 hoverColor;
    vec2 texelSize;           // (1/w, 1/h)
    float thickness;          // screen-space tap offset in pixels
    float depthEdgeThreshold; // Sobel(linear depth) cutoff in metres
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

uint edgeFromStencil(uint p[9]) {
    // Mask bit 4 (transparent flag) — see header comment / RP.12b.
    uint lo = p[0] & 0x3u;
    uint hi = lo;
    for (int i = 1; i < 9; ++i) {
        uint base = p[i] & 0x3u;
        lo = min(lo, base);
        hi = max(hi, base);
    }
    return (lo == hi) ? 0u : hi;
}

float sobelMagnitude(float p[9]) {
    float gx = -p[0] + p[2]
               - (2.0 * p[3]) + (2.0 * p[5])
               - p[6] + p[8];
    float gy = -p[0] - (2.0 * p[1]) - p[2]
               + p[6] + (2.0 * p[7]) + p[8];
    return sqrt((gx * gx) + (gy * gy));
}

void main() {
    uint sPatch[9];
    float dPatch[9];
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 3; ++i) {
            vec2 off = vec2(float(i - 1), float(j - 1)) * pc.thickness * pc.texelSize;
            vec2 uv = inUv + off;
            sPatch[j * 3 + i] = texture(stencilTexture, uv).r;
            dPatch[j * 3 + i] = texture(linearDepthTexture, uv).r;
        }
    }

    uint cat = edgeFromStencil(sPatch);

    // Depth-discontinuity fallback: when the stencil is uniform but the centre
    // is inside a selected element, a large Sobel-on-linear-depth signals a
    // within-selection silhouette/crease that should still be outlined. Mask
    // bit 4 (transparent flag) before the gate so a glass-only centre never
    // promotes itself to an outline category.
    uint centerCategory = sPatch[4] & 0x3u;
    if (cat == 0u && centerCategory > 0u) {
        float depthMag = sobelMagnitude(dPatch);
        if (depthMag > pc.depthEdgeThreshold) {
            cat = centerCategory;
        }
    }

    if (cat == 0u) {
        discard;
    }

    outColor = (cat == 2u) ? pc.hoverColor : pc.selectedColor;
}
