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

    ubo.lightSpaceMatrix = scene.shadow.lightSpaceMatrix;
    const float invRes = scene.shadow.mapResolution > 0
                             ? 1.0F / static_cast<float>(scene.shadow.mapResolution)
                             : 0.0F;
    ubo.shadowParams = glm::vec4(scene.shadow.enabled ? 1.0F : 0.0F, scene.shadow.bias,
                                 scene.shadow.pcfRadius, invRes);
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

float ComputePcfShadow(const glm::mat4& lightSpaceMatrix,
                       const glm::vec3& worldPos,
                       float bias,
                       float invMapResolution,
                       const std::function<float(glm::vec2)>& sampleDepth) {
    const glm::vec4 clip = lightSpaceMatrix * glm::vec4(worldPos, 1.0F);
    if (clip.w <= 0.0F) {
        return 1.0F;
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const glm::vec2 uv = glm::vec2(ndc) * 0.5F + 0.5F;
    const float fragDepth = ndc.z;

    // Outside the shadow map → treat as lit (matches CLAMP_TO_BORDER + opaque white).
    if (uv.x < 0.0F || uv.x > 1.0F || uv.y < 0.0F || uv.y > 1.0F ||
        fragDepth < 0.0F || fragDepth > 1.0F) {
        return 1.0F;
    }

    float lit = 0.0F;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const glm::vec2 offset(static_cast<float>(dx) * invMapResolution,
                                   static_cast<float>(dy) * invMapResolution);
            const float stored = sampleDepth(uv + offset);
            lit += ((fragDepth - bias) <= stored) ? 1.0F : 0.0F;
        }
    }
    return lit / 9.0F;
}

}  // namespace bimeup::renderer
