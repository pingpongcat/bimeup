#include <gtest/gtest.h>

#include <renderer/Lighting.h>
#include <renderer/SkyColor.h>
#include <renderer/SunPosition.h>

#include <glm/glm.hpp>

#include <cmath>
#include <cstring>
#include <numbers>

namespace {

using bimeup::renderer::ComputeSkyColor;
using bimeup::renderer::ComputeSunDirection;
using bimeup::renderer::LightingUbo;
using bimeup::renderer::PackSunLighting;
using bimeup::renderer::ShadowSettings;
using bimeup::renderer::SkyColor;
using bimeup::renderer::SunLightingScene;
using bimeup::renderer::SunPosition;
using bimeup::renderer::SunSite;

constexpr float kEps = 1e-4F;
constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kDegToRad = kPi / 180.0;

// Two reference pairs cover daytime and night for the full packer contract.
//   Greenwich vernal equinox noon (UTC): sun high, colourful LUT day key.
//   Warsaw local midnight (equinox): sun well below horizon, night key.
constexpr double kJdGreenwichEquinoxNoon = 2460390.0;
constexpr double kJdEquinoxMidnight = 2460390.5;

SunLightingScene MakeGreenwichNoonScene() {
    SunLightingScene s;
    s.julianDayUtc = kJdGreenwichEquinoxNoon;
    s.siteLocation = SunSite{51.4779 * kDegToRad, 0.0, 0.0};
    s.trueNorthRad = 0.0;
    return s;
}

SunLightingScene MakeWarsawMidnightScene() {
    SunLightingScene s;
    s.julianDayUtc = kJdEquinoxMidnight;
    s.siteLocation = SunSite{52.2297 * kDegToRad, 21.0122 * kDegToRad, 0.0};
    s.trueNorthRad = 0.0;
    return s;
}

TEST(SunLightingTest, KeyDirectionMatchesComputeSunDirection) {
    const auto scene = MakeGreenwichNoonScene();
    const SunPosition sun = ComputeSunDirection(scene.julianDayUtc,
                                                scene.siteLocation.latitudeRad,
                                                scene.siteLocation.longitudeRad,
                                                scene.trueNorthRad);
    const LightingUbo ubo = PackSunLighting(scene);

    EXPECT_NEAR(ubo.keyDirectionIntensity.x, sun.dirWorld.x, kEps);
    EXPECT_NEAR(ubo.keyDirectionIntensity.y, sun.dirWorld.y, kEps);
    EXPECT_NEAR(ubo.keyDirectionIntensity.z, sun.dirWorld.z, kEps);
    EXPECT_NEAR(ubo.keyDirectionIntensity.w, 1.0F, kEps);
}

TEST(SunLightingTest, KeyColorMatchesSkyLutSunColor) {
    const auto scene = MakeGreenwichNoonScene();
    const SunPosition sun = ComputeSunDirection(scene.julianDayUtc,
                                                scene.siteLocation.latitudeRad,
                                                scene.siteLocation.longitudeRad,
                                                scene.trueNorthRad);
    const SkyColor sky = ComputeSkyColor(sun.elevation);
    const LightingUbo ubo = PackSunLighting(scene);

    EXPECT_NEAR(ubo.keyColorEnabled.r, sky.sunColor.r, kEps);
    EXPECT_NEAR(ubo.keyColorEnabled.g, sky.sunColor.g, kEps);
    EXPECT_NEAR(ubo.keyColorEnabled.b, sky.sunColor.b, kEps);
    EXPECT_NEAR(ubo.keyColorEnabled.w, 1.0F, kEps);
}

TEST(SunLightingTest, FillAndRimZeroed) {
    const auto scene = MakeGreenwichNoonScene();
    const LightingUbo ubo = PackSunLighting(scene);

    EXPECT_NEAR(ubo.fillDirectionIntensity.x, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillDirectionIntensity.y, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillDirectionIntensity.z, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillDirectionIntensity.w, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillColorEnabled.r, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillColorEnabled.g, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillColorEnabled.b, 0.0F, kEps);
    EXPECT_NEAR(ubo.fillColorEnabled.w, 0.0F, kEps);

    EXPECT_NEAR(ubo.rimDirectionIntensity.x, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimDirectionIntensity.y, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimDirectionIntensity.z, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimDirectionIntensity.w, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimColorEnabled.r, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimColorEnabled.g, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimColorEnabled.b, 0.0F, kEps);
    EXPECT_NEAR(ubo.rimColorEnabled.w, 0.0F, kEps);
}

TEST(SunLightingTest, AmbientTripleMatchesSkyLut) {
    const auto scene = MakeGreenwichNoonScene();
    const SunPosition sun = ComputeSunDirection(scene.julianDayUtc,
                                                scene.siteLocation.latitudeRad,
                                                scene.siteLocation.longitudeRad,
                                                scene.trueNorthRad);
    const SkyColor sky = ComputeSkyColor(sun.elevation);
    const LightingUbo ubo = PackSunLighting(scene);

    EXPECT_NEAR(ubo.skyZenith.r, sky.zenith.r, kEps);
    EXPECT_NEAR(ubo.skyZenith.g, sky.zenith.g, kEps);
    EXPECT_NEAR(ubo.skyZenith.b, sky.zenith.b, kEps);
    EXPECT_NEAR(ubo.skyHorizon.r, sky.horizon.r, kEps);
    EXPECT_NEAR(ubo.skyHorizon.g, sky.horizon.g, kEps);
    EXPECT_NEAR(ubo.skyHorizon.b, sky.horizon.b, kEps);
    EXPECT_NEAR(ubo.skyGround.r, sky.ground.r, kEps);
    EXPECT_NEAR(ubo.skyGround.g, sky.ground.g, kEps);
    EXPECT_NEAR(ubo.skyGround.b, sky.ground.b, kEps);
}

TEST(SunLightingTest, NightSceneSunColorIsBlack) {
    // At equinox local midnight in Warsaw the sun is well below the horizon;
    // ComputeSkyColor clamps to the night key whose sunColor is (0,0,0).
    const auto scene = MakeWarsawMidnightScene();
    const LightingUbo ubo = PackSunLighting(scene);

    EXPECT_NEAR(ubo.keyColorEnabled.r, 0.0F, kEps);
    EXPECT_NEAR(ubo.keyColorEnabled.g, 0.0F, kEps);
    EXPECT_NEAR(ubo.keyColorEnabled.b, 0.0F, kEps);
}

TEST(SunLightingTest, ShadowParamsPassedThrough) {
    auto scene = MakeGreenwichNoonScene();
    scene.shadow.enabled = true;
    scene.shadow.bias = 0.02F;
    scene.shadow.pcfRadius = 1.75F;
    scene.shadow.mapResolution = 2048U;
    scene.shadow.lightSpaceMatrix = glm::mat4(3.0F);

    const LightingUbo ubo = PackSunLighting(scene);

    EXPECT_NEAR(ubo.shadowParams.x, 1.0F, kEps);
    EXPECT_NEAR(ubo.shadowParams.y, 0.02F, kEps);
    EXPECT_NEAR(ubo.shadowParams.z, 1.75F, kEps);
    EXPECT_NEAR(ubo.shadowParams.w, 1.0F / 2048.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[0][0], 3.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[1][1], 3.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[2][2], 3.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[3][3], 3.0F, kEps);
}

TEST(SunLightingTest, PackIsBitExactIdempotent) {
    auto scene = MakeGreenwichNoonScene();
    scene.shadow.enabled = true;
    scene.shadow.bias = 0.01F;
    scene.shadow.pcfRadius = 1.5F;
    scene.shadow.mapResolution = 1024U;

    const LightingUbo a = PackSunLighting(scene);
    const LightingUbo b = PackSunLighting(scene);
    EXPECT_EQ(0, std::memcmp(&a, &b, sizeof(LightingUbo)));
}

TEST(SunLightingTest, TrueNorthRotatesKeyDirection) {
    auto base = MakeGreenwichNoonScene();
    auto rotated = base;
    rotated.trueNorthRad = kPi * 0.5;  // 90° CCW model-vs-true-north

    const LightingUbo a = PackSunLighting(base);
    const LightingUbo b = PackSunLighting(rotated);

    // Y component (vertical) must be identical — elevation is unaffected by
    // TrueNorth; XZ must differ — azimuth rotates.
    EXPECT_NEAR(a.keyDirectionIntensity.y, b.keyDirectionIntensity.y, kEps);
    const float dxz =
        std::abs(a.keyDirectionIntensity.x - b.keyDirectionIntensity.x) +
        std::abs(a.keyDirectionIntensity.z - b.keyDirectionIntensity.z);
    EXPECT_GT(dxz, 0.1F);
}

TEST(SunLightingTest, IndoorToggleDoesNotAffectKeyInPhase16_4_a) {
    // RP.16.5 will mutate the fill slot + ambient when indoor is enabled.
    // At 16.4.a the key (sun) slot must remain identical either way — pin it.
    auto off = MakeGreenwichNoonScene();
    auto on = off;
    on.indoorLightsEnabled = true;

    const LightingUbo a = PackSunLighting(off);
    const LightingUbo b = PackSunLighting(on);
    EXPECT_NEAR(a.keyDirectionIntensity.x, b.keyDirectionIntensity.x, kEps);
    EXPECT_NEAR(a.keyDirectionIntensity.y, b.keyDirectionIntensity.y, kEps);
    EXPECT_NEAR(a.keyDirectionIntensity.z, b.keyDirectionIntensity.z, kEps);
    EXPECT_NEAR(a.keyColorEnabled.r, b.keyColorEnabled.r, kEps);
    EXPECT_NEAR(a.keyColorEnabled.g, b.keyColorEnabled.g, kEps);
    EXPECT_NEAR(a.keyColorEnabled.b, b.keyColorEnabled.b, kEps);
}

}  // namespace
