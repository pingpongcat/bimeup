#include <renderer/Lighting.h>

#include <renderer/SkyColor.h>
#include <renderer/SunPosition.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>

namespace bimeup::renderer {

LightingUbo PackSunLighting(const SunLightingScene& scene) {
    const SunPosition sun = ComputeSunDirection(scene.julianDayUtc,
                                                scene.siteLocation.latitudeRad,
                                                scene.siteLocation.longitudeRad,
                                                scene.trueNorthRad);
    const SkyColor sky = ComputeSkyColor(sun.elevation);

    LightingUbo ubo{};
    ubo.keyDirectionIntensity = glm::vec4(sun.dirWorld, 1.0F);
    ubo.keyColorEnabled = glm::vec4(sky.sunColor, 1.0F);
    // Rim is retired in the sun-driven model.
    ubo.rimDirectionIntensity = glm::vec4(0.0F);
    ubo.rimColorEnabled = glm::vec4(0.0F);

    glm::vec3 zenith = sky.zenith;
    glm::vec3 horizon = sky.horizon;
    glm::vec3 ground = sky.ground;

    if (scene.indoorLightsEnabled) {
        const glm::vec3 indoorDir = glm::normalize(glm::vec3(0.2F, -1.0F, 0.3F));
        const glm::vec3 indoorColor(1.0F, 0.95F, 0.85F);  // warm soft white
        ubo.fillDirectionIntensity = glm::vec4(indoorDir, 0.5F);
        ubo.fillColorEnabled = glm::vec4(indoorColor, 1.0F);
        // Approximate bounce light from interior surfaces: sky drops a notch,
        // horizon trims slightly, ground lifts (floor bounce dominates indoors).
        zenith *= 0.7F;
        horizon *= 0.9F;
        ground *= 1.2F;
    } else {
        ubo.fillDirectionIntensity = glm::vec4(0.0F);
        ubo.fillColorEnabled = glm::vec4(0.0F);
    }

    ubo.skyZenith = glm::vec4(zenith, 0.0F);
    ubo.skyHorizon = glm::vec4(horizon, 0.0F);
    ubo.skyGround = glm::vec4(ground, 0.0F);

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

glm::vec3 ComputeHemisphereAmbient(const glm::vec3& normal, const HemisphereAmbient& sky) {
    const float t = glm::normalize(normal).y;
    if (t >= 0.0F) {
        return glm::mix(sky.horizon, sky.zenith, t);
    }
    return glm::mix(sky.horizon, sky.ground, -t);
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
