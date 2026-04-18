#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormalWorld;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform LightingUBO {
    vec4 keyDirectionIntensity;
    vec4 keyColorEnabled;
    vec4 fillDirectionIntensity;
    vec4 fillColorEnabled;
    vec4 rimDirectionIntensity;
    vec4 rimColorEnabled;
    vec4 skyZenith;
    vec4 skyHorizon;
    vec4 skyGround;
    mat4 lightSpaceMatrix;
    vec4 shadowParams;  // x=enabled, y=bias, z=pcfRadius, w=1/resolution
} lights;

layout(set = 0, binding = 2) uniform sampler2DShadow shadowMap;

layout(set = 0, binding = 3) uniform ClipPlanesUBO {
    vec4 planes[6];  // ax + by + cz + d = 0; a point is kept when dot(n,p) + d >= 0
    ivec4 count;     // x = number of active planes
} clipPlanes;

vec3 lambertContribution(vec3 n, vec4 dirI, vec4 colE) {
    float enabled = colE.w;
    vec3 toLight = normalize(-dirI.xyz);
    float ndotl = max(dot(n, toLight), 0.0);
    return colE.rgb * (dirI.w * ndotl * enabled);
}

// 3-tone hemisphere ambient sampled by dot(n, +Y). Mirrors
// bimeup::renderer::ComputeHemisphereAmbient on the CPU.
vec3 hemisphereAmbient(vec3 n) {
    float t = n.y;
    if (t >= 0.0) {
        return mix(lights.skyHorizon.rgb, lights.skyZenith.rgb, t);
    }
    return mix(lights.skyHorizon.rgb, lights.skyGround.rgb, -t);
}

// 3x3 PCF mirror of bimeup::renderer::ComputePcfShadow. Returns visibility in
// [0,1] — 1 when fully lit, 0 when fully occluded.
float pcfShadow(vec3 worldPos) {
    vec4 clip = lights.lightSpaceMatrix * vec4(worldPos, 1.0);
    if (clip.w <= 0.0) return 1.0;
    vec3 ndc = clip.xyz / clip.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float fragDepth = ndc.z;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ||
        fragDepth < 0.0 || fragDepth > 1.0) {
        return 1.0;
    }

    float bias = lights.shadowParams.y;
    float texel = lights.shadowParams.w;
    float radius = lights.shadowParams.z;
    float ref = fragDepth - bias;

    float lit = 0.0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            vec2 offset = vec2(float(dx), float(dy)) * texel * radius;
            // sampler2DShadow: third component is the reference depth; hardware
            // performs the comparison and returns 1.0 if ref <= stored, 0.0 else
            // (filtered by VK_FILTER_LINEAR for 2x2 hardware PCF at each tap).
            lit += texture(shadowMap, vec3(uv + offset, ref));
        }
    }
    return lit / 9.0;
}

void main() {
    // Clip planes: discard fragments behind any active plane (signed distance < 0).
    for (int i = 0; i < clipPlanes.count.x; ++i) {
        vec4 eq = clipPlanes.planes[i];
        if (dot(eq.xyz, fragWorldPos) + eq.w < 0.0) {
            discard;
        }
    }

    vec3 n = normalize(fragNormalWorld);
    if (!gl_FrontFacing) n = -n;  // Light back faces correctly (double-sided rendering).

    vec3 lit = hemisphereAmbient(n);

    // Shadow only attenuates the key light; fill and rim stay unshadowed for now.
    vec3 key = lambertContribution(n, lights.keyDirectionIntensity, lights.keyColorEnabled);
    float shadowEnabled = lights.shadowParams.x;
    float visibility = mix(1.0, pcfShadow(fragWorldPos), shadowEnabled);
    lit += key * visibility;

    lit += lambertContribution(n, lights.fillDirectionIntensity, lights.fillColorEnabled);
    lit += lambertContribution(n, lights.rimDirectionIntensity, lights.rimColorEnabled);

    outColor = vec4(fragColor.rgb * lit, fragColor.a);
}
