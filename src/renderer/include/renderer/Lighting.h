#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>

namespace bimeup::renderer {

struct DirectionalLight {
    glm::vec3 direction{0.0F, -1.0F, 0.0F};  // points FROM the light source
    glm::vec3 color{1.0F};
    float intensity{1.0F};
    bool enabled{true};
};

struct ShadowSettings {
    bool enabled{false};
    float bias{0.005F};                       // depth bias, in light-clip-space units
    float pcfRadius{1.0F};                    // PCF tap offset in texels
    std::uint32_t mapResolution{1024};
    glm::mat4 lightSpaceMatrix{1.0F};         // world → light clip space
};

// 3-tone hemisphere ambient: sampled along dot(normal, +Y world-up). +Y picks
// `zenith`, -Y picks `ground`, horizontal normals pick `horizon`.
struct HemisphereAmbient {
    glm::vec3 zenith{0.55F, 0.60F, 0.70F};
    glm::vec3 horizon{0.60F, 0.60F, 0.60F};
    glm::vec3 ground{0.25F, 0.22F, 0.20F};
};

struct LightingScene {
    DirectionalLight key;
    DirectionalLight fill;
    DirectionalLight rim;
    HemisphereAmbient sky{};
    ShadowSettings shadow{};
};

// std140-packed UBO that mirrors the GLSL LightingUBO.
// Layout: each DirectionalLight → (vec4 directionIntensity, vec4 colorEnabled),
// then vec4 skyZenith, vec4 skyHorizon, vec4 skyGround, mat4 lightSpaceMatrix,
// vec4 shadowParams (x=enabled, y=bias, z=pcfRadius, w=1/resolution).
struct LightingUbo {
    glm::vec4 keyDirectionIntensity;
    glm::vec4 keyColorEnabled;
    glm::vec4 fillDirectionIntensity;
    glm::vec4 fillColorEnabled;
    glm::vec4 rimDirectionIntensity;
    glm::vec4 rimColorEnabled;
    glm::vec4 skyZenith;
    glm::vec4 skyHorizon;
    glm::vec4 skyGround;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 shadowParams;
};

static_assert(sizeof(LightingUbo) == 224, "LightingUbo must match std140 layout");

LightingScene MakeDefaultLighting();
LightingUbo PackLighting(const LightingScene& scene);

// Renderer-local mirror of geographic site info (intentionally not tied to the
// `ifc` module). Lat/lon are geodetic radians; longitude is positive east.
struct SunSite {
    double latitudeRad{0.0};
    double longitudeRad{0.0};
    double elevationM{0.0};
};

// Sun-driven replacement for `LightingScene`. One directional sun in the key
// slot (direction + colour from `ComputeSunDirection` + `ComputeSkyColor`);
// fill/rim slots zeroed (filled by the indoor preset in RP.16.5); hemisphere
// ambient derived from sun elevation. `exposure` is scene metadata routed to
// the tonemap pass by the caller — not consumed by `PackSunLighting`.
struct SunLightingScene {
    double julianDayUtc{2451545.0};  // J2000 epoch — arbitrary fallback
    SunSite siteLocation{};
    double trueNorthRad{0.0};
    bool indoorLightsEnabled{false};
    float exposure{1.0F};
    ShadowSettings shadow{};
};

LightingUbo PackSunLighting(const SunLightingScene& scene);

// Lambertian contribution from a single directional light on a surface with the
// given world-space normal. Mirrors the fragment-shader math so unit tests can
// verify lighting behavior without a GPU.
glm::vec3 ComputeLambert(const DirectionalLight& light, const glm::vec3& normal);

// 3-tone hemisphere ambient sampled by dot(normalize(normal), +Y). Mirrors the
// fragment-shader math so unit tests can verify behaviour on the CPU.
glm::vec3 ComputeHemisphereAmbient(const glm::vec3& normal, const HemisphereAmbient& sky);

// 3x3 PCF shadow visibility in [0, 1]. Returns 1.0 when the shaded point is lit,
// 0.0 when fully occluded, and a fraction in between when only some PCF taps are
// occluded. `sampleDepth(uv)` must return the stored depth at the given shadow-map
// UV; the caller is responsible for out-of-range behavior (the mirror below treats
// UVs outside [0, 1] as fully lit, matching a clamp-to-border sampler with opaque
// white border color).
//
// Mirrors the GLSL code in `assets/shaders/basic.frag` so PCF logic can be
// unit-tested without a GPU.
float ComputePcfShadow(const glm::mat4& lightSpaceMatrix,
                       const glm::vec3& worldPos,
                       float bias,
                       float invMapResolution,
                       const std::function<float(glm::vec2)>& sampleDepth);

}  // namespace bimeup::renderer
