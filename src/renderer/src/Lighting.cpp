#include <renderer/Lighting.h>

#include <glm/geometric.hpp>

#include <algorithm>

namespace bimeup::renderer {

LightingScene MakeDefaultLighting() {
    LightingScene s;

    // Key: warm, above-and-in-front from the upper-right.
    s.key.direction = glm::normalize(glm::vec3(-0.6F, -1.0F, -0.5F));
    s.key.color = glm::vec3(1.0F, 0.96F, 0.88F);
    s.key.intensity = 1.0F;
    s.key.enabled = true;

    // Fill: cooler, softer, from the opposite side.
    s.fill.direction = glm::normalize(glm::vec3(0.8F, -0.3F, -0.4F));
    s.fill.color = glm::vec3(0.75F, 0.82F, 1.0F);
    s.fill.intensity = 0.45F;
    s.fill.enabled = true;

    // Rim: from behind, catches silhouettes.
    s.rim.direction = glm::normalize(glm::vec3(0.2F, -0.3F, 1.0F));
    s.rim.color = glm::vec3(1.0F);
    s.rim.intensity = 0.35F;
    s.rim.enabled = true;

    s.ambient = glm::vec3(0.08F, 0.09F, 0.10F);
    return s;
}

LightingUbo PackLighting(const LightingScene& scene) {
    auto packLight = [](const DirectionalLight& l, glm::vec4& dirI, glm::vec4& colE) {
        dirI = glm::vec4(l.direction, l.intensity);
        colE = glm::vec4(l.color, l.enabled ? 1.0F : 0.0F);
    };

    LightingUbo ubo{};
    packLight(scene.key, ubo.keyDirectionIntensity, ubo.keyColorEnabled);
    packLight(scene.fill, ubo.fillDirectionIntensity, ubo.fillColorEnabled);
    packLight(scene.rim, ubo.rimDirectionIntensity, ubo.rimColorEnabled);
    ubo.ambient = glm::vec4(scene.ambient, 0.0F);
    return ubo;
}

glm::vec3 ComputeLambert(const DirectionalLight& light, const glm::vec3& normal) {
    if (!light.enabled) {
        return glm::vec3(0.0F);
    }
    // Direction points FROM the light, so the "to-light" vector is -direction.
    glm::vec3 toLight = -light.direction;
    float ndotl = std::max(0.0F, glm::dot(glm::normalize(normal), glm::normalize(toLight)));
    return light.color * (light.intensity * ndotl);
}

}  // namespace bimeup::renderer
