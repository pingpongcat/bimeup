#include <gtest/gtest.h>

#include <renderer/Lighting.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>

using bimeup::renderer::ComputeLambert;
using bimeup::renderer::ComputePcfShadow;
using bimeup::renderer::DirectionalLight;
using bimeup::renderer::LightingUbo;
using bimeup::renderer::MakeDefaultLighting;
using bimeup::renderer::PackLighting;
using bimeup::renderer::ShadowSettings;

namespace {

constexpr float kEps = 1e-4F;

TEST(LightingTest, DefaultKeyFillRimEnabled) {
    auto lighting = MakeDefaultLighting();
    EXPECT_TRUE(lighting.key.enabled);
    EXPECT_TRUE(lighting.fill.enabled);
    EXPECT_TRUE(lighting.rim.enabled);
}

TEST(LightingTest, DefaultKeyIsBrightestLight) {
    auto lighting = MakeDefaultLighting();
    EXPECT_GT(lighting.key.intensity, lighting.fill.intensity);
    EXPECT_GT(lighting.key.intensity, lighting.rim.intensity);
}

TEST(LightingTest, LambertHeadOnLightIsFullIntensity) {
    DirectionalLight light{};
    light.direction = glm::vec3(0.0F, 0.0F, -1.0F);  // light points into -Z
    light.color = glm::vec3(1.0F);
    light.intensity = 1.0F;
    light.enabled = true;

    glm::vec3 normal(0.0F, 0.0F, 1.0F);  // surface faces +Z (toward light source)
    glm::vec3 contribution = ComputeLambert(light, normal);

    EXPECT_NEAR(contribution.r, 1.0F, kEps);
    EXPECT_NEAR(contribution.g, 1.0F, kEps);
    EXPECT_NEAR(contribution.b, 1.0F, kEps);
}

TEST(LightingTest, LambertBackFacingIsZero) {
    DirectionalLight light{};
    light.direction = glm::vec3(0.0F, 0.0F, -1.0F);
    light.color = glm::vec3(1.0F);
    light.intensity = 1.0F;
    light.enabled = true;

    glm::vec3 normal(0.0F, 0.0F, -1.0F);  // facing away
    glm::vec3 contribution = ComputeLambert(light, normal);

    EXPECT_NEAR(contribution.r, 0.0F, kEps);
    EXPECT_NEAR(contribution.g, 0.0F, kEps);
    EXPECT_NEAR(contribution.b, 0.0F, kEps);
}

TEST(LightingTest, LambertObliqueIsCosAngle) {
    DirectionalLight light{};
    light.direction = glm::normalize(glm::vec3(0.0F, 0.0F, -1.0F));
    light.color = glm::vec3(1.0F, 1.0F, 1.0F);
    light.intensity = 1.0F;
    light.enabled = true;

    // 60° off-axis — cos(60°) = 0.5
    glm::vec3 normal = glm::normalize(glm::vec3(std::sqrt(3.0F) / 2.0F, 0.0F, 0.5F));
    glm::vec3 contribution = ComputeLambert(light, normal);

    EXPECT_NEAR(contribution.r, 0.5F, kEps);
}

TEST(LightingTest, LambertDisabledLightContributesNothing) {
    DirectionalLight light{};
    light.direction = glm::vec3(0.0F, 0.0F, -1.0F);
    light.color = glm::vec3(1.0F);
    light.intensity = 1.0F;
    light.enabled = false;

    glm::vec3 normal(0.0F, 0.0F, 1.0F);
    glm::vec3 contribution = ComputeLambert(light, normal);

    EXPECT_NEAR(contribution.r, 0.0F, kEps);
}

TEST(LightingTest, LambertScalesWithIntensity) {
    DirectionalLight light{};
    light.direction = glm::vec3(0.0F, 0.0F, -1.0F);
    light.color = glm::vec3(1.0F);
    light.intensity = 2.5F;
    light.enabled = true;

    glm::vec3 normal(0.0F, 0.0F, 1.0F);
    glm::vec3 contribution = ComputeLambert(light, normal);

    EXPECT_NEAR(contribution.r, 2.5F, kEps);
}

TEST(LightingTest, PackedUboMatchesStd140Size) {
    // std140: 3 × (vec4 dirIntensity + vec4 colorEnabled) + vec4 ambient = 112 bytes
    // + mat4 lightSpaceMatrix (64) + vec4 shadowParams (16) = 192 bytes
    EXPECT_EQ(sizeof(LightingUbo), 192U);
}

TEST(LightingTest, PackLightingPreservesDirectionAndIntensity) {
    auto scene = MakeDefaultLighting();
    auto ubo = PackLighting(scene);

    EXPECT_NEAR(ubo.keyDirectionIntensity.x, scene.key.direction.x, kEps);
    EXPECT_NEAR(ubo.keyDirectionIntensity.y, scene.key.direction.y, kEps);
    EXPECT_NEAR(ubo.keyDirectionIntensity.z, scene.key.direction.z, kEps);
    EXPECT_NEAR(ubo.keyDirectionIntensity.w, scene.key.intensity, kEps);
}

TEST(LightingTest, PackLightingEncodesEnabledFlagInColorW) {
    auto scene = MakeDefaultLighting();
    scene.fill.enabled = false;
    auto ubo = PackLighting(scene);

    EXPECT_NEAR(ubo.fillColorEnabled.w, 0.0F, kEps);
    EXPECT_NEAR(ubo.keyColorEnabled.w, 1.0F, kEps);
}

TEST(LightingTest, PackLightingEncodesShadowParams) {
    auto scene = MakeDefaultLighting();
    scene.shadow.enabled = true;
    scene.shadow.bias = 0.01F;
    scene.shadow.pcfRadius = 1.5F;
    scene.shadow.mapResolution = 2048U;

    auto ubo = PackLighting(scene);

    EXPECT_NEAR(ubo.shadowParams.x, 1.0F, kEps);
    EXPECT_NEAR(ubo.shadowParams.y, 0.01F, kEps);
    EXPECT_NEAR(ubo.shadowParams.z, 1.5F, kEps);
    EXPECT_NEAR(ubo.shadowParams.w, 1.0F / 2048.0F, kEps);
}

TEST(LightingTest, PackLightingDefaultShadowsDisabled) {
    auto scene = MakeDefaultLighting();
    auto ubo = PackLighting(scene);

    EXPECT_NEAR(ubo.shadowParams.x, 0.0F, kEps);
}

TEST(LightingTest, PackLightingPreservesLightSpaceMatrix) {
    auto scene = MakeDefaultLighting();
    scene.shadow.lightSpaceMatrix = glm::mat4(2.0F);  // scale-2 identity-ish
    auto ubo = PackLighting(scene);

    EXPECT_NEAR(ubo.lightSpaceMatrix[0][0], 2.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[1][1], 2.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[2][2], 2.0F, kEps);
    EXPECT_NEAR(ubo.lightSpaceMatrix[3][3], 2.0F, kEps);
}

// --- ComputePcfShadow: 3x3 PCF visibility [0,1]. 1=lit, 0=fully in shadow. -----

TEST(PcfShadowTest, FullyLitWhenStoredDepthGreaterThanFragDepth) {
    // Light-space identity matrix maps world→light trivially; world point (0,0,0)
    // projects to UV=(0.5, 0.5) and depth=0.5 after the standard NDC→UV remap.
    glm::mat4 ls(1.0F);
    ls = glm::translate(ls, glm::vec3(0.0F, 0.0F, 0.5F));  // shift so clip.z = 0.5
    auto stored = [](glm::vec2 /*uv*/) { return 0.9F; };   // well behind

    float vis = ComputePcfShadow(ls, glm::vec3(0.0F), 0.001F, 1.0F / 1024.0F, stored);
    EXPECT_NEAR(vis, 1.0F, 1e-5F);
}

TEST(PcfShadowTest, FullyShadowedWhenStoredDepthLessThanFragDepth) {
    glm::mat4 ls(1.0F);
    ls = glm::translate(ls, glm::vec3(0.0F, 0.0F, 0.5F));
    auto stored = [](glm::vec2 /*uv*/) { return 0.1F; };  // in front — occluder

    float vis = ComputePcfShadow(ls, glm::vec3(0.0F), 0.001F, 1.0F / 1024.0F, stored);
    EXPECT_NEAR(vis, 0.0F, 1e-5F);
}

TEST(PcfShadowTest, PartialOcclusionReturnsFraction) {
    // 5 of 9 taps occlude, 4 lit → visibility = 4/9.
    glm::mat4 ls(1.0F);
    ls = glm::translate(ls, glm::vec3(0.0F, 0.0F, 0.5F));

    int call = 0;
    auto stored = [&](glm::vec2 /*uv*/) {
        ++call;
        return (call <= 5) ? 0.1F : 0.9F;  // first 5 samples occlude
    };

    float vis = ComputePcfShadow(ls, glm::vec3(0.0F), 0.001F, 1.0F / 1024.0F, stored);
    EXPECT_NEAR(vis, 4.0F / 9.0F, 1e-5F);
}

TEST(PcfShadowTest, OutsideShadowMapTreatedAsLit) {
    // Push the fragment well outside the light-space [-1,1] NDC cube.
    glm::mat4 ls(1.0F);  // identity
    auto stored = [](glm::vec2 /*uv*/) { return 0.0F; };  // anything in range — irrelevant

    // World point at x=100 → clip.x=100 → uv=50.5, way outside [0,1].
    float vis = ComputePcfShadow(ls, glm::vec3(100.0F, 0.0F, 0.0F), 0.001F,
                                 1.0F / 1024.0F, stored);
    EXPECT_NEAR(vis, 1.0F, 1e-5F);
}

TEST(PcfShadowTest, BiasPreventsSelfShadowing) {
    // fragDepth == storedDepth exactly — without bias, floating-point noise would
    // flicker. With a positive bias, the shaded point should register as lit.
    glm::mat4 ls(1.0F);
    ls = glm::translate(ls, glm::vec3(0.0F, 0.0F, 0.5F));
    auto stored = [](glm::vec2 /*uv*/) { return 0.5F; };

    float vis = ComputePcfShadow(ls, glm::vec3(0.0F), 0.01F, 1.0F / 1024.0F, stored);
    EXPECT_NEAR(vis, 1.0F, 1e-5F);
}

}  // namespace
