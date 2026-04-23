#version 460
#extension GL_EXT_ray_query : require

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormalWorld;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragNormalView;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outNormal;
// Transparency stencil G-buffer (RP.12b). Bit 2 (value 4) marks fragments
// from the transparent pipeline; XeGTAO uses it to treat glass as
// transparent instead of AO-occluding. Low bits unused post-RP.15
// (selection-outline retirement).
layout(location = 2) out uint outStencilId;

// Fragment-stage push constant. Sits at offset 64 — the byte after the
// 64-byte vertex-stage model matrix range pushed by basic.vert. Shaded /
// wireframe / transparent pipelines all share this layout; overlay pipelines
// (section-fill, disk-marker) keep their layouts untouched and never touch
// the range. `transparentBit` is 0 for opaque draws and 4 for the
// transparent pipeline — the fragment writes it into the stencil G-buffer.
// Stage 9.8.b.1: `useRtSunPath` is 0 for the classical raster path (sun
// term baked here with PCF + transmission, bit-compatible with pre-9.8
// output) and 1 when Hybrid RT is active and the downstream sun composite
// pass (Stage 9.8.b.2) will re-apply the key term using RT visibility —
// this fragment then skips the sun block so the composite doesn't double
// up. Stage 9.8.c.1: `useRtIndoorPath` plays the same role for the indoor
// fill-light lambert — 0 bakes it here (bit-compatible raster path), 1
// skips it so the Stage 9.8.c.2 composite can re-apply it with RT wall
// occlusion. Transparent draws stay on path 0 for both flags in every
// mode (composites only cover opaque surfaces). Stage 9.Q.3:
// `useRayQueryShadow` is 0 for raster mode (PCF shadow-map sample) and 1
// for ray-query mode — the inner branch swaps the PCF tap for an inline
// `rayQueryEXT` trace against the TLAS. Glass transmission (RP.18 tint)
// composes the same way in either branch: visibility is the only thing
// that swaps. Mutually exclusive with `useRtSunPath` in practice (only
// one mode can claim the sun term at a time).
layout(push_constant) uniform StencilPush {
    layout(offset = 64) uint transparentBit;
    layout(offset = 68) uint useRtSunPath;
    layout(offset = 72) uint useRtIndoorPath;
    layout(offset = 76) uint useRayQueryShadow;
} push;

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

// RP.18.4 — tinted sun attenuation written from the light's POV by the
// transparent-shadow pass. Cleared to opaque white (= no attenuation); glass
// writes `surfaceColor * (1 - alpha)` min-blended so overlapping panes keep
// the darkest tap. Sampler border is opaque white so out-of-frustum reads
// behave as "no glass".
layout(set = 0, binding = 4) uniform sampler2D shadowTransmissionMap;

// Stage 9.Q.3 — TLAS for inline ray-query shadow traces. Bound only when
// the device exposes ray-query (Stage 9.Q.1 probe) and the scene's TLAS
// has been built (`SceneUploader::WriteTlasToDescriptor`). The fragment
// only samples it when `push.useRayQueryShadow == 1` — raster mode keeps
// the PCF path and never touches this binding.
layout(set = 0, binding = 5) uniform accelerationStructureEXT tlas;

vec3 lambertContribution(vec3 n, vec4 dirI, vec4 colE) {
    float enabled = colE.w;
    vec3 toLight = normalize(-dirI.xyz);
    float ndotl = max(dot(n, toLight), 0.0);
    return colE.rgb * (dirI.w * ndotl * enabled);
}

// Octahedron-encode a unit normal into [-1, 1]^2. Byte-for-byte mirror of
// bimeup::renderer::OctPackNormal (Cigolle et al. JCGT 2014 signed encoding).
vec2 octPackNormal(vec3 n) {
    float l1 = abs(n.x) + abs(n.y) + abs(n.z);
    vec3 nn = n / l1;
    vec2 e = nn.xy;
    if (nn.z < 0.0) {
        vec2 folded = vec2(1.0 - abs(e.y), 1.0 - abs(e.x));
        vec2 signE = vec2(e.x >= 0.0 ? 1.0 : -1.0, e.y >= 0.0 ? 1.0 : -1.0);
        e = folded * signE;
    }
    return e;
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

// RP.18.4 / RP.18.7 — mirror of bimeup::renderer::ComputeTransmittedSun.
// `transmit.rgb` = nearest glass tint at this lightUV (min-blended
// `vec3(1 - alpha)`, cleared to 1 = "no glass"); `transmit.a` = nearest glass's
// light-space Z (min-blended `gl_FragCoord.z`, cleared to 1 = "far"). Tint
// applies only when glass is strictly in front of the fragment — otherwise the
// 2D transmission map would leak sunlight into rooms that share a shadow-map
// texel with a window they can't see. Visibility (PCF) still multiplies, so an
// opaque wall between glass and fragment blocks regardless of glass presence.
vec3 computeTransmittedSun(float visibility, float fragLightZ, float bias,
                           vec3 sunColor, vec4 transmit) {
    bool glassAhead = transmit.a < fragLightZ - bias;
    vec3 tint = glassAhead ? transmit.rgb : vec3(1.0);
    return sunColor * (visibility * tint);
}

// Stage 9.Q.3 — single inline ray-query trace toward the sun. Returns
// visibility in [0,1] — 1 when the ray reaches the sky unobstructed, 0
// when something is in the way. Geometric-truth alternative to PCF: no
// shadow-map bias artefacts, no resolution-bound contact-shadow gap. The
// `OpaqueEXT | TerminateOnFirstHit` flag pair plus the
// `SkipClosestHitShader` keep the trace allocation-free — we don't need
// hit attributes, just a binary occluded/lit answer. Surface-normal bias
// (epsilon along `n`) keeps the ray from self-intersecting the surface
// it was launched from. Pulls the sun direction from `LightingUBO` —
// `keyDirectionIntensity.xyz` is the direction sun light *travels*, so
// `toLight = -keyDirectionIntensity.xyz`. Glass instances with the
// 9.6.a `FORCE_NO_OPAQUE` flag don't attenuate this trace (any-hit
// shaders aren't invoked by ray-query proceed in opaque mode); window
// transmission is handled separately via the RP.18 transmission map.
float rayQueryShadow(vec3 worldPos, vec3 worldNormal) {
    vec3 toSun = normalize(-lights.keyDirectionIntensity.xyz);
    vec3 origin = worldPos + worldNormal * 0.001;
    rayQueryEXT rq;
    rayQueryInitializeEXT(rq, tlas,
                          gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
                          0xFFu, origin, 0.001, toSun, 1.0e6);
    rayQueryProceedEXT(rq);
    return rayQueryGetIntersectionTypeEXT(rq, true) ==
           gl_RayQueryCommittedIntersectionNoneEXT ? 1.0 : 0.0;
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
    vec3 nView = normalize(fragNormalView);
    if (!gl_FrontFacing) {  // Double-sided rendering: flip so lighting + G-buffer
        n = -n;              // see the visible face.
        nView = -nView;
    }

    // MRT normal G-buffer for SSAO. R16G16_SNORM target.
    outNormal = octPackNormal(nView);
    outStencilId = push.transparentBit;

    vec3 lit = hemisphereAmbient(n);

    // Stage 9.8.b.1 — skip the sun block when the Hybrid RT composite
    // pass will re-apply it with RT visibility. `useRtSunPath == 0`
    // keeps the raster path (PCF shadow + glass transmission) fully
    // baked here and bit-compatible with pre-9.8 output.
    if (push.useRtSunPath == 0U) {
        // Shadow only attenuates the key light; fill and rim stay unshadowed for now.
        vec3 key = lambertContribution(n, lights.keyDirectionIntensity, lights.keyColorEnabled);
        float shadowEnabled = lights.shadowParams.x;
        // Stage 9.Q.3 — pick visibility source. Ray-query mode (push set
        // by the panel via the new RenderMode::RayQuery in 9.Q.4) traces
        // a single shadow ray; raster mode keeps the PCF tap. Glass tint
        // composes on top of either via the unchanged
        // `computeTransmittedSun` path below.
        float rawShadow = (push.useRayQueryShadow == 1U)
            ? rayQueryShadow(fragWorldPos, n)
            : pcfShadow(fragWorldPos);
        float visibility = mix(1.0, rawShadow, shadowEnabled);
        // RP.18.4 / RP.18.7 — sample the window-transmission map at the fragment's
        // lightUV. Need the fragment's own light-space Z too (not just the UV) so
        // `computeTransmittedSun` can gate the tint on "glass is in front of this
        // fragment". Compute the light-space projection once here; out-of-frustum
        // clips use fragLightZ = 0 so `glassAhead = (transmit.a < 0 - bias)` is
        // always false. Shadow-disabled path keeps the old bit-compatible output
        // by passing transmit = opaque white (glassAhead still false).
        vec4 lightClip = lights.lightSpaceMatrix * vec4(fragWorldPos, 1.0);
        float fragLightZ = 0.0;
        vec4 transmit = vec4(1.0);
        if (lightClip.w > 0.0) {
            vec3 lightNdc = lightClip.xyz / lightClip.w;
            vec2 lightUv = lightNdc.xy * 0.5 + 0.5;
            fragLightZ = lightNdc.z;
            vec4 sampled = texture(shadowTransmissionMap, lightUv);
            transmit = mix(vec4(1.0), sampled, shadowEnabled);
        }
        float bias = lights.shadowParams.y;
        lit += computeTransmittedSun(visibility, fragLightZ, bias, key, transmit);
    }

    // Stage 9.8.c.1 — skip the indoor fill-light lambert when the Hybrid
    // RT composite pass will re-apply it with wall-occlusion visibility.
    // `useRtIndoorPath == 0` keeps the unshadowed directional fill baked
    // here (bit-compatible with pre-9.8 output).
    if (push.useRtIndoorPath == 0U) {
        lit += lambertContribution(n, lights.fillDirectionIntensity, lights.fillColorEnabled);
    }
    lit += lambertContribution(n, lights.rimDirectionIntensity, lights.rimColorEnabled);

    outColor = vec4(fragColor.rgb * lit, fragColor.a);
}
