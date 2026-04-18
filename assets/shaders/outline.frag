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
// pass per RP.6c — 0 = background, 1 = selected, 2 = hovered. Hover wins
// when both appear in the 3×3 window so a cursor over an already-selected
// element still renders the hover colour.

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
    uint lo = p[0];
    uint hi = p[0];
    for (int i = 1; i < 9; ++i) {
        lo = min(lo, p[i]);
        hi = max(hi, p[i]);
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
    // within-selection silhouette/crease that should still be outlined.
    uint centerStencil = sPatch[4];
    if (cat == 0u && centerStencil > 0u) {
        float depthMag = sobelMagnitude(dPatch);
        if (depthMag > pc.depthEdgeThreshold) {
            cat = centerStencil;
        }
    }

    if (cat == 0u) {
        discard;
    }

    outColor = (cat == 2u) ? pc.hoverColor : pc.selectedColor;
}
