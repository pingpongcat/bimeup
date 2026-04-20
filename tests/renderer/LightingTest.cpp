#include <gtest/gtest.h>

#include <renderer/Lighting.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>

using bimeup::renderer::ComputeHemisphereAmbient;
using bimeup::renderer::ComputeLambert;
using bimeup::renderer::ComputePcfShadow;
using bimeup::renderer::ComputeTransmittedSun;
using bimeup::renderer::DirectionalLight;
using bimeup::renderer::HemisphereAmbient;
using bimeup::renderer::LightingUbo;

namespace {

constexpr float kEps = 1e-4F;

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
    // std140: 3 × (vec4 dirIntensity + vec4 colorEnabled) = 96 bytes
    // + 3 × vec4 (skyZenith, skyHorizon, skyGround) = 48 bytes
    // + mat4 lightSpaceMatrix (64) + vec4 shadowParams (16) = 224 bytes
    EXPECT_EQ(sizeof(LightingUbo), 224U);
}

// --- ComputeHemisphereAmbient: cardinal-normal sanity + lerp behaviour. -------

TEST(HemisphereAmbientTest, ZenithWhenNormalPointsUp) {
    HemisphereAmbient sky{glm::vec3(0.2F, 0.4F, 0.9F), glm::vec3(0.5F, 0.5F, 0.5F),
                          glm::vec3(0.1F, 0.08F, 0.06F)};
    glm::vec3 a = ComputeHemisphereAmbient(glm::vec3(0.0F, 1.0F, 0.0F), sky);
    EXPECT_NEAR(a.r, sky.zenith.r, kEps);
    EXPECT_NEAR(a.g, sky.zenith.g, kEps);
    EXPECT_NEAR(a.b, sky.zenith.b, kEps);
}

TEST(HemisphereAmbientTest, GroundWhenNormalPointsDown) {
    HemisphereAmbient sky{glm::vec3(0.2F, 0.4F, 0.9F), glm::vec3(0.5F, 0.5F, 0.5F),
                          glm::vec3(0.1F, 0.08F, 0.06F)};
    glm::vec3 a = ComputeHemisphereAmbient(glm::vec3(0.0F, -1.0F, 0.0F), sky);
    EXPECT_NEAR(a.r, sky.ground.r, kEps);
    EXPECT_NEAR(a.g, sky.ground.g, kEps);
    EXPECT_NEAR(a.b, sky.ground.b, kEps);
}

TEST(HemisphereAmbientTest, HorizonForCardinalHorizontalNormals) {
    HemisphereAmbient sky{glm::vec3(0.2F, 0.4F, 0.9F), glm::vec3(0.5F, 0.55F, 0.6F),
                          glm::vec3(0.1F, 0.08F, 0.06F)};
    const std::array<glm::vec3, 4> horiz{glm::vec3(1.0F, 0.0F, 0.0F),
                                         glm::vec3(-1.0F, 0.0F, 0.0F),
                                         glm::vec3(0.0F, 0.0F, 1.0F),
                                         glm::vec3(0.0F, 0.0F, -1.0F)};
    for (const glm::vec3& n : horiz) {
        glm::vec3 a = ComputeHemisphereAmbient(n, sky);
        EXPECT_NEAR(a.r, sky.horizon.r, kEps);
        EXPECT_NEAR(a.g, sky.horizon.g, kEps);
        EXPECT_NEAR(a.b, sky.horizon.b, kEps);
    }
}

TEST(HemisphereAmbientTest, MidwayBetweenHorizonAndZenith) {
    HemisphereAmbient sky{glm::vec3(1.0F), glm::vec3(0.0F), glm::vec3(0.5F)};
    // 45° upward: n.y = sqrt(2)/2 ≈ 0.707 → lerp horizon→zenith by 0.707.
    glm::vec3 n = glm::normalize(glm::vec3(1.0F, 1.0F, 0.0F));
    glm::vec3 a = ComputeHemisphereAmbient(n, sky);
    const float t = n.y;
    EXPECT_NEAR(a.r, t, kEps);
    EXPECT_NEAR(a.g, t, kEps);
    EXPECT_NEAR(a.b, t, kEps);
}

TEST(HemisphereAmbientTest, NormalizesInput) {
    HemisphereAmbient sky{glm::vec3(1.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F),
                          glm::vec3(0.0F, 0.0F, 1.0F)};
    glm::vec3 a1 = ComputeHemisphereAmbient(glm::vec3(0.0F, 1.0F, 0.0F), sky);
    glm::vec3 a2 = ComputeHemisphereAmbient(glm::vec3(0.0F, 7.3F, 0.0F), sky);
    EXPECT_NEAR(a1.r, a2.r, kEps);
    EXPECT_NEAR(a1.g, a2.g, kEps);
    EXPECT_NEAR(a1.b, a2.b, kEps);
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

// --- ComputeTransmittedSun: raster approximation of sun through IfcWindow glass. ---
//
// RP.18.7 — `transmit.a` holds the nearest glass's light-space Z (cleared to 1
// = "far / no glass"). `fragLightZ` is this fragment's light-space Z. Glass
// tints the sun only when glass is strictly in front of the fragment
// (`glassAhead = transmit.a < fragLightZ - bias`); opaque visibility still
// multiplies the result so a wall between the glass and the fragment blocks.

constexpr float kShadowBias = 0.0005F;

TEST(TransmittedSunTest, FullVisibilityNoGlassIsFullSun) {
    // No glass in the light path (transmission cleared to opaque white, alpha
    // = 1 = "far") + PCF fully lit → sun is unattenuated.
    const glm::vec3 sun(1.0F, 0.95F, 0.8F);
    const glm::vec4 transmit(1.0F, 1.0F, 1.0F, 1.0F);
    const float fragLightZ = 0.5F;
    glm::vec3 out = ComputeTransmittedSun(1.0F, fragLightZ, kShadowBias, sun, transmit);
    EXPECT_NEAR(out.r, sun.r, kEps);
    EXPECT_NEAR(out.g, sun.g, kEps);
    EXPECT_NEAR(out.b, sun.b, kEps);
}

TEST(TransmittedSunTest, ZeroVisibilityNoGlassIsBlack) {
    // Wall shadow, no glass at lightUV → fragment fully dark.
    const glm::vec3 sun(1.0F, 0.95F, 0.8F);
    const glm::vec4 transmit(1.0F, 1.0F, 1.0F, 1.0F);
    glm::vec3 out = ComputeTransmittedSun(0.0F, 0.5F, kShadowBias, sun, transmit);
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(TransmittedSunTest, ZeroVisibilityGlassAheadStillBlack) {
    // RP.18.7 bug-fix case: the transmission map has glass recorded at this
    // UV (room-A window), and glassZ is in front of the fragment's light-space
    // depth. But the opaque PCF says the fragment is shadowed — a wall
    // (between room A and room B) blocks the light. Must stay black; pre-RP.18.7
    // code would have lit this fragment with the tint.
    const glm::vec3 sun(1.0F, 1.0F, 1.0F);
    const glm::vec4 transmit(0.4F, 0.5F, 0.6F, 0.2F);  // glassZ = 0.2
    const float fragLightZ = 0.5F;                      // glass is in front
    glm::vec3 out = ComputeTransmittedSun(0.0F, fragLightZ, kShadowBias, sun, transmit);
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(TransmittedSunTest, FullVisibilityGlassAheadReturnsSunTimesTint) {
    // Floor directly under a window: PCF = 1 (floor is its own opaque occluder),
    // glassZ = 0.2 is in front of the floor's light-space Z. Result = sun × tint.
    const glm::vec3 sun(1.0F, 1.0F, 1.0F);
    const glm::vec4 transmit(0.4F, 0.5F, 0.6F, 0.2F);
    const float fragLightZ = 0.5F;
    glm::vec3 out = ComputeTransmittedSun(1.0F, fragLightZ, kShadowBias, sun, transmit);
    EXPECT_NEAR(out.r, sun.r * transmit.r, kEps);
    EXPECT_NEAR(out.g, sun.g * transmit.g, kEps);
    EXPECT_NEAR(out.b, sun.b * transmit.b, kEps);
}

TEST(TransmittedSunTest, FullVisibilityGlassBehindFragmentIsUnchangedSun) {
    // Glass is recorded at this UV but its depth is behind the fragment (e.g.
    // the window is further from the sun than this lit surface). Tint must not
    // apply — fragment reads as full sun.
    const glm::vec3 sun(1.0F, 1.0F, 1.0F);
    const glm::vec4 transmit(0.4F, 0.5F, 0.6F, 0.8F);  // glassZ = 0.8
    const float fragLightZ = 0.5F;                      // glass is behind
    glm::vec3 out = ComputeTransmittedSun(1.0F, fragLightZ, kShadowBias, sun, transmit);
    EXPECT_NEAR(out.r, sun.r, kEps);
    EXPECT_NEAR(out.g, sun.g, kEps);
    EXPECT_NEAR(out.b, sun.b, kEps);
}

TEST(TransmittedSunTest, PartialVisibilityGlassAheadScalesTint) {
    // Penumbra through glass: PCF tap gives 0.4 visibility (edge of shadow),
    // glass is in front → result = 0.4 × sun × tint.
    const glm::vec3 sun(1.0F, 1.0F, 1.0F);
    const glm::vec4 transmit(0.4F, 0.5F, 0.6F, 0.2F);
    const float fragLightZ = 0.5F;
    const float visibility = 0.4F;
    glm::vec3 out = ComputeTransmittedSun(visibility, fragLightZ, kShadowBias, sun, transmit);
    EXPECT_NEAR(out.r, visibility * sun.r * transmit.r, kEps);
    EXPECT_NEAR(out.g, visibility * sun.g * transmit.g, kEps);
    EXPECT_NEAR(out.b, visibility * sun.b * transmit.b, kEps);
}

}  // namespace
