#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

struct DirectionalLight {
    glm::vec3 direction{0.0F, -1.0F, 0.0F};  // points FROM the light source
    glm::vec3 color{1.0F};
    float intensity{1.0F};
    bool enabled{true};
};

struct LightingScene {
    DirectionalLight key;
    DirectionalLight fill;
    DirectionalLight rim;
    glm::vec3 ambient{0.08F, 0.09F, 0.10F};
};

// std140-packed UBO that mirrors the GLSL LightingUBO.
// Layout: each DirectionalLight → (vec4 directionIntensity, vec4 colorEnabled)
// ambient packed in the w=0 channel's .xyz.
struct LightingUbo {
    glm::vec4 keyDirectionIntensity;
    glm::vec4 keyColorEnabled;
    glm::vec4 fillDirectionIntensity;
    glm::vec4 fillColorEnabled;
    glm::vec4 rimDirectionIntensity;
    glm::vec4 rimColorEnabled;
    glm::vec4 ambient;  // xyz=color, w=unused
};

static_assert(sizeof(LightingUbo) == 112, "LightingUbo must match std140 layout");

LightingScene MakeDefaultLighting();
LightingUbo PackLighting(const LightingScene& scene);

// Lambertian contribution from a single directional light on a surface with the
// given world-space normal. Mirrors the fragment-shader math so unit tests can
// verify lighting behavior without a GPU.
glm::vec3 ComputeLambert(const DirectionalLight& light, const glm::vec3& normal);

}  // namespace bimeup::renderer
