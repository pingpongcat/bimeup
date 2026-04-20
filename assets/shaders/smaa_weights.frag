#version 450

// SMAA 1x blending-weight calculation (RP.11b.3). Ported from
// `external/smaa/SMAA.hlsl` — see `SMAABlendingWeightCalculationPS` and its
// helpers (search_length, search_xy, area, diag, corner). Kept faithful to
// the HIGH preset (MAX_SEARCH_STEPS=16, MAX_SEARCH_STEPS_DIAG=8,
// CORNER_ROUNDING=25); diagonal + corner detection are enabled.
//
// Bindings (set 0):
//   0 = edges texture (RG8, output of `smaa_edge.frag` — RP.11b.2)
//   1 = AreaTex LUT   (RG8, 160×560, vendored from iryoku/smaa — RP.11b.1)
//   2 = SearchTex LUT (R8,  64×16,   vendored from iryoku/smaa — RP.11b.1)
//
// The LUTs are sampled with LOD 0 (they have no mips by construction).
// `smaa.vert` is the shared fullscreen-triangle vert — the offset vectors
// that the HLSL reference computes in its vertex stage are folded into this
// fragment instead, so the same vert can drive all three SMAA passes.

layout(set = 0, binding = 0) uniform sampler2D edgesTex;
layout(set = 0, binding = 1) uniform sampler2D areaTex;
layout(set = 0, binding = 2) uniform sampler2D searchTex;

layout(push_constant) uniform Push {
    // `subsampleIndices` first so the vec4 sits on its natural 16-byte
    // boundary (std430 push-constant alignment); `rcpFrame` follows at
    // offset 16; RP.19 quality ints pack at 24 / 28. Total 32 bytes — the
    // CPU contract tests pin this layout.
    //
    // `subsampleIndices` is always (0,0,0,0) for SMAA 1x; kept in the
    // contract so this shader can be reused verbatim if SMAA T2x ever lands.
    // `maxSearchSteps` / `maxSearchStepsDiag` were `const int` in the shader
    // until RP.19 promoted them to runtime knobs so the panel can offer the
    // iryoku LOW/MEDIUM/HIGH presets without per-preset pipeline variants.
    vec4 subsampleIndices;     // @ 0
    vec2 rcpFrame;             // @ 16 — (1/width, 1/height)
    int maxSearchSteps;        // @ 24 — LOW=4, MED=8, HIGH=16
    int maxSearchStepsDiag;    // @ 28 — LOW=2, MED=4, HIGH=8
} pc;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outWeights;

const float SMAA_CORNER_ROUNDING_NORM    = 0.25;
const float SMAA_AREATEX_MAX_DISTANCE    = 16.0;
const float SMAA_AREATEX_MAX_DISTANCE_DIAG = 20.0;
const vec2  SMAA_AREATEX_PIXEL_SIZE      = vec2(1.0 / 160.0, 1.0 / 560.0);
const float SMAA_AREATEX_SUBTEX_SIZE     = 1.0 / 7.0;
const vec2  SMAA_SEARCHTEX_SIZE          = vec2(66.0, 33.0);
const vec2  SMAA_SEARCHTEX_PACKED_SIZE   = vec2(64.0, 16.0);

// -----------------------------------------------------------------------------
// Diagonal pattern helpers

vec2 DecodeDiagBilinearAccess2(vec2 e) {
    e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);
    return round(e);
}

vec4 DecodeDiagBilinearAccess4(vec4 e) {
    e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);
    return round(e);
}

vec2 SearchDiag1(vec2 texcoord, vec2 dir, out vec2 e) {
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    vec3 t = vec3(pc.rcpFrame, 1.0);
    while (coord.z < float(pc.maxSearchStepsDiag - 1) && coord.w > 0.9) {
        coord.xyz = t * vec3(dir, 1.0) + coord.xyz;
        e = textureLod(edgesTex, coord.xy, 0.0).rg;
        coord.w = dot(e, vec2(0.5));
    }
    return coord.zw;
}

vec2 SearchDiag2(vec2 texcoord, vec2 dir, out vec2 e) {
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    coord.x += 0.25 * pc.rcpFrame.x;
    vec3 t = vec3(pc.rcpFrame, 1.0);
    while (coord.z < float(pc.maxSearchStepsDiag - 1) && coord.w > 0.9) {
        coord.xyz = t * vec3(dir, 1.0) + coord.xyz;
        e = textureLod(edgesTex, coord.xy, 0.0).rg;
        e = DecodeDiagBilinearAccess2(e);
        coord.w = dot(e, vec2(0.5));
    }
    return coord.zw;
}

vec2 AreaDiag(vec2 dist, vec2 e, float offset) {
    vec2 texcoord = vec2(SMAA_AREATEX_MAX_DISTANCE_DIAG) * e + dist;
    texcoord = SMAA_AREATEX_PIXEL_SIZE * texcoord + 0.5 * SMAA_AREATEX_PIXEL_SIZE;
    // Diagonal areas sit on the second half of the texture.
    texcoord.x += 0.5;
    texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;
    return textureLod(areaTex, texcoord, 0.0).rg;
}

vec2 CalculateDiagWeights(vec2 texcoord, vec2 e, vec4 subsampleIndices) {
    vec2 weights = vec2(0.0);

    // Search line 1 — direction (-1, 1).
    vec4 d;
    vec2 endPt;
    if (e.r > 0.0) {
        d.xz = SearchDiag1(texcoord, vec2(-1.0, 1.0), endPt);
        d.x += float(endPt.y > 0.9);
    } else {
        d.xz = vec2(0.0);
    }
    d.yw = SearchDiag1(texcoord, vec2(1.0, -1.0), endPt);

    if (d.x + d.y > 2.0) {
        vec4 coords = vec4(-d.x + 0.25, d.x, d.y, -d.y - 0.25) * pc.rcpFrame.xyxy + texcoord.xyxy;
        vec4 c;
        c.xy = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(-1, 0)).rg;
        c.zw = textureLodOffset(edgesTex, coords.zw, 0.0, ivec2( 1, 0)).rg;
        c.yxwz = DecodeDiagBilinearAccess4(c.xyzw);

        vec2 cc = vec2(2.0) * c.xz + c.yw;
        cc = mix(cc, vec2(0.0), step(0.9, d.zw));

        weights += AreaDiag(d.xy, cc, subsampleIndices.z);
    }

    // Search line 2 — direction (-1, -1).
    d.xz = SearchDiag2(texcoord, vec2(-1.0, -1.0), endPt);
    if (textureLodOffset(edgesTex, texcoord, 0.0, ivec2(1, 0)).r > 0.0) {
        d.yw = SearchDiag2(texcoord, vec2(1.0, 1.0), endPt);
        d.y += float(endPt.y > 0.9);
    } else {
        d.yw = vec2(0.0);
    }

    if (d.x + d.y > 2.0) {
        vec4 coords = vec4(-d.x, -d.x, d.y, d.y) * pc.rcpFrame.xyxy + texcoord.xyxy;
        vec4 c;
        c.x = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2(-1, 0)).g;
        c.y = textureLodOffset(edgesTex, coords.xy, 0.0, ivec2( 0, -1)).r;
        c.zw = textureLodOffset(edgesTex, coords.zw, 0.0, ivec2(1, 0)).gr;
        vec2 cc = vec2(2.0) * c.xz + c.yw;
        cc = mix(cc, vec2(0.0), step(0.9, d.zw));

        weights += AreaDiag(d.xy, cc, subsampleIndices.w).gr;
    }

    return weights;
}

// -----------------------------------------------------------------------------
// Horizontal / vertical search helpers

float SearchLength(vec2 e, float offset) {
    vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
    vec2 bias = SMAA_SEARCHTEX_SIZE * vec2(offset, 1.0);
    scale += vec2(-1.0,  1.0);
    bias  += vec2( 0.5, -0.5);
    scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    bias  *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    return textureLod(searchTex, scale * e + bias, 0.0).r;
}

float SearchXLeft(vec2 texcoord, float end) {
    vec2 e = vec2(0.0, 1.0);
    while (texcoord.x > end && e.g > 0.8281 && e.r == 0.0) {
        e = textureLod(edgesTex, texcoord, 0.0).rg;
        texcoord = -vec2(2.0, 0.0) * pc.rcpFrame + texcoord;
    }
    float offset = -(255.0 / 127.0) * SearchLength(e, 0.0) + 3.25;
    return pc.rcpFrame.x * offset + texcoord.x;
}

float SearchXRight(vec2 texcoord, float end) {
    vec2 e = vec2(0.0, 1.0);
    while (texcoord.x < end && e.g > 0.8281 && e.r == 0.0) {
        e = textureLod(edgesTex, texcoord, 0.0).rg;
        texcoord = vec2(2.0, 0.0) * pc.rcpFrame + texcoord;
    }
    float offset = -(255.0 / 127.0) * SearchLength(e, 0.5) + 3.25;
    return -pc.rcpFrame.x * offset + texcoord.x;
}

float SearchYUp(vec2 texcoord, float end) {
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y > end && e.r > 0.8281 && e.g == 0.0) {
        e = textureLod(edgesTex, texcoord, 0.0).rg;
        texcoord = -vec2(0.0, 2.0) * pc.rcpFrame + texcoord;
    }
    float offset = -(255.0 / 127.0) * SearchLength(e.gr, 0.0) + 3.25;
    return pc.rcpFrame.y * offset + texcoord.y;
}

float SearchYDown(vec2 texcoord, float end) {
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y < end && e.r > 0.8281 && e.g == 0.0) {
        e = textureLod(edgesTex, texcoord, 0.0).rg;
        texcoord = vec2(0.0, 2.0) * pc.rcpFrame + texcoord;
    }
    float offset = -(255.0 / 127.0) * SearchLength(e.gr, 0.5) + 3.25;
    return -pc.rcpFrame.y * offset + texcoord.y;
}

vec2 Area(vec2 dist, float e1, float e2, float offset) {
    vec2 texcoord = vec2(SMAA_AREATEX_MAX_DISTANCE) * round(4.0 * vec2(e1, e2)) + dist;
    texcoord = SMAA_AREATEX_PIXEL_SIZE * texcoord + 0.5 * SMAA_AREATEX_PIXEL_SIZE;
    texcoord.y = SMAA_AREATEX_SUBTEX_SIZE * offset + texcoord.y;
    return textureLod(areaTex, texcoord, 0.0).rg;
}

// -----------------------------------------------------------------------------
// Corner detection

void DetectHorizontalCornerPattern(inout vec2 weights, vec4 tc, vec2 d) {
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
    rounding /= leftRight.x + leftRight.y;

    vec2 factor = vec2(1.0);
    factor.x -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2(0,  1)).r;
    factor.x -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2(1,  1)).r;
    factor.y -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2(0, -2)).r;
    factor.y -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2(1, -2)).r;

    weights *= clamp(factor, 0.0, 1.0);
}

void DetectVerticalCornerPattern(inout vec2 weights, vec4 tc, vec2 d) {
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;
    rounding /= leftRight.x + leftRight.y;

    vec2 factor = vec2(1.0);
    factor.x -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2( 1, 0)).g;
    factor.x -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2( 1, 1)).g;
    factor.y -= rounding.x * textureLodOffset(edgesTex, tc.xy, 0.0, ivec2(-2, 0)).g;
    factor.y -= rounding.y * textureLodOffset(edgesTex, tc.zw, 0.0, ivec2(-2, 1)).g;

    weights *= clamp(factor, 0.0, 1.0);
}

// -----------------------------------------------------------------------------
// Main — mirrors SMAABlendingWeightCalculationPS.

void main() {
    vec2 texcoord = inUv;
    vec2 pixcoord = texcoord / pc.rcpFrame;  // pixel-space coord

    // Offsets computed in HLSL's SMAABlendingWeightCalculationVS — folded
    // into the FS here since smaa.vert is shared across all three passes.
    vec4 offset0 = vec4(-0.25, -0.125,  1.25, -0.125) * pc.rcpFrame.xyxy + texcoord.xyxy;
    vec4 offset1 = vec4(-0.125, -0.25, -0.125,  1.25) * pc.rcpFrame.xyxy + texcoord.xyxy;
    vec4 offset2 = vec4(-2.0, 2.0, -2.0, 2.0) * vec4(pc.rcpFrame.xx, pc.rcpFrame.yy) *
                       float(pc.maxSearchSteps) +
                   vec4(offset0.xz, offset1.yw);

    vec4 weights = vec4(0.0);
    vec2 e = texture(edgesTex, texcoord).rg;

    if (e.g > 0.0) {
        // Edge at north — try diagonals first.
        weights.rg = CalculateDiagWeights(texcoord, e, pc.subsampleIndices);

        if (weights.r == -weights.g) {  // no diagonal pattern found
            vec2 d;

            vec3 coords;
            coords.x = SearchXLeft(offset0.xy, offset2.x);
            coords.y = offset1.y;
            d.x = coords.x;

            float e1 = textureLod(edgesTex, coords.xy, 0.0).r;

            coords.z = SearchXRight(offset0.zw, offset2.y);
            d.y = coords.z;

            // HLSL: d = abs(round(mad(SMAA_RT_METRICS.zz, d, -pixcoord.xx)))
            // where SMAA_RT_METRICS.zz = (width, width). Rewritten as
            // d * (1 / rcpFrame.x) - pixcoord.xx.
            d = abs(round((1.0 / pc.rcpFrame.x) * vec2(coords.x, coords.z) - pixcoord.xx));

            vec2 sqrt_d = sqrt(d);

            float e2 = textureLodOffset(edgesTex, coords.zy, 0.0, ivec2(1, 0)).r;

            weights.rg = Area(sqrt_d, e1, e2, pc.subsampleIndices.y);

            coords.y = texcoord.y;
            DetectHorizontalCornerPattern(weights.rg, vec4(coords.x, coords.y, coords.z, coords.y), d);
        } else {
            e.r = 0.0;  // skip vertical processing
        }
    }

    if (e.r > 0.0) {
        vec2 d;

        vec3 coords;
        coords.y = SearchYUp(offset1.xy, offset2.z);
        coords.x = offset0.x;
        d.x = coords.y;

        float e1 = textureLod(edgesTex, coords.xy, 0.0).g;

        coords.z = SearchYDown(offset1.zw, offset2.w);
        d.y = coords.z;

        d = abs(round((1.0 / pc.rcpFrame.y) * vec2(coords.y, coords.z) - pixcoord.yy));

        vec2 sqrt_d = sqrt(d);

        float e2 = textureLodOffset(edgesTex, coords.xz, 0.0, ivec2(0, 1)).g;

        weights.ba = Area(sqrt_d, e1, e2, pc.subsampleIndices.x);

        coords.x = texcoord.x;
        DetectVerticalCornerPattern(weights.ba, vec4(coords.x, coords.y, coords.x, coords.z), d);
    }

    outWeights = weights;
}
