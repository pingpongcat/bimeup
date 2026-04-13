#include <gtest/gtest.h>

#include <renderer/Lighting.h>

#include <glm/glm.hpp>

#include <cmath>

using bimeup::renderer::ComputeLambert;
using bimeup::renderer::DirectionalLight;
using bimeup::renderer::LightingUbo;
using bimeup::renderer::MakeDefaultLighting;
using bimeup::renderer::PackLighting;

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
    EXPECT_EQ(sizeof(LightingUbo), 112U);
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

}  // namespace
