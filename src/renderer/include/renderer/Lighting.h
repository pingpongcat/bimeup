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

struct LightingScene {
    DirectionalLight key;
    DirectionalLight fill;
    DirectionalLight rim;
    glm::vec3 ambient{0.08F, 0.09F, 0.10F};
    ShadowSettings shadow{};
};

// std140-packed UBO that mirrors the GLSL LightingUBO.
// Layout: each DirectionalLight → (vec4 directionIntensity, vec4 colorEnabled),
// then vec4 ambient, mat4 lightSpaceMatrix, vec4 shadowParams
// (x=enabled, y=bias, z=pcfRadius, w=1/resolution).
struct LightingUbo {
    glm::vec4 keyDirectionIntensity;
    glm::vec4 keyColorEnabled;
    glm::vec4 fillDirectionIntensity;
    glm::vec4 fillColorEnabled;
    glm::vec4 rimDirectionIntensity;
    glm::vec4 rimColorEnabled;
    glm::vec4 ambient;
    glm::mat4 lightSpaceMatrix;
    glm::vec4 shadowParams;
};

static_assert(sizeof(LightingUbo) == 192, "LightingUbo must match std140 layout");

LightingScene MakeDefaultLighting();
LightingUbo PackLighting(const LightingScene& scene);

// Lambertian contribution from a single directional light on a surface with the
// given world-space normal. Mirrors the fragment-shader math so unit tests can
// verify lighting behavior without a GPU.
glm::vec3 ComputeLambert(const DirectionalLight& light, const glm::vec3& normal);

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
