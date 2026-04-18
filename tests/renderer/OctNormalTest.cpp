#include <gtest/gtest.h>

#include <renderer/OctNormal.h>

#include <glm/glm.hpp>

#include <array>
#include <cmath>

namespace {

using bimeup::renderer::OctPackNormal;
using bimeup::renderer::OctUnpackNormal;

constexpr float kEps = 1e-5F;

TEST(OctNormalTest, PackPlusZEncodesToOrigin) {
    glm::vec2 e = OctPackNormal(glm::vec3(0.0F, 0.0F, 1.0F));
    EXPECT_NEAR(e.x, 0.0F, kEps);
    EXPECT_NEAR(e.y, 0.0F, kEps);
}

TEST(OctNormalTest, PackOutputStaysInUnitRange) {
    constexpr int kThetaSteps = 64;
    constexpr int kPhiSteps = 32;
    constexpr float kTwoPi = 6.2831853F;
    for (int i = 0; i < kThetaSteps; ++i) {
        float theta = (static_cast<float>(i) / static_cast<float>(kThetaSteps)) * kTwoPi;
        for (int j = 1; j < kPhiSteps; ++j) {
            float phi = (static_cast<float>(j) / static_cast<float>(kPhiSteps)) * 3.1415927F;
            glm::vec3 n{std::sin(phi) * std::cos(theta),
                        std::sin(phi) * std::sin(theta),
                        std::cos(phi)};
            glm::vec2 e = OctPackNormal(n);
            EXPECT_LE(e.x, 1.0F + kEps);
            EXPECT_GE(e.x, -1.0F - kEps);
            EXPECT_LE(e.y, 1.0F + kEps);
            EXPECT_GE(e.y, -1.0F - kEps);
        }
    }
}

TEST(OctNormalTest, RoundTripCardinalAxes) {
    std::array<glm::vec3, 6> axes = {{
        { 1.0F,  0.0F,  0.0F},
        {-1.0F,  0.0F,  0.0F},
        { 0.0F,  1.0F,  0.0F},
        { 0.0F, -1.0F,  0.0F},
        { 0.0F,  0.0F,  1.0F},
        { 0.0F,  0.0F, -1.0F},
    }};
    for (const auto& n : axes) {
        glm::vec3 r = OctUnpackNormal(OctPackNormal(n));
        EXPECT_NEAR(r.x, n.x, kEps) << "axis " << n.x << "," << n.y << "," << n.z;
        EXPECT_NEAR(r.y, n.y, kEps);
        EXPECT_NEAR(r.z, n.z, kEps);
    }
}

TEST(OctNormalTest, RoundTripFibonacciSphere) {
    // Golden-angle Fibonacci distribution over S^2 — every sample should round-trip
    // with a reconstructed direction dot-product ≥ 1 - 1e-5 and unit length.
    constexpr int kCount = 1024;
    constexpr float kGoldenAngle = 2.3999632F;
    for (int i = 0; i < kCount; ++i) {
        float z = 1.0F - (2.0F * (static_cast<float>(i) + 0.5F) /
                          static_cast<float>(kCount));
        float r = std::sqrt(std::max(0.0F, 1.0F - (z * z)));
        float a = static_cast<float>(i) * kGoldenAngle;
        glm::vec3 n{r * std::cos(a), r * std::sin(a), z};
        glm::vec3 rec = OctUnpackNormal(OctPackNormal(n));
        EXPECT_NEAR(glm::length(rec), 1.0F, kEps);
        EXPECT_GT(glm::dot(n, rec), 0.9999F);
    }
}

TEST(OctNormalTest, RoundTripNegativeZHemisphere) {
    // The fold-branch of the octahedron — verify every corner of the -Z hemisphere.
    std::array<glm::vec3, 4> neg = {{
        glm::normalize(glm::vec3( 1.0F,  1.0F, -1.0F)),
        glm::normalize(glm::vec3(-1.0F,  1.0F, -1.0F)),
        glm::normalize(glm::vec3( 1.0F, -1.0F, -1.0F)),
        glm::normalize(glm::vec3(-1.0F, -1.0F, -1.0F)),
    }};
    for (const auto& n : neg) {
        glm::vec3 rec = OctUnpackNormal(OctPackNormal(n));
        EXPECT_GT(glm::dot(n, rec), 0.9999F);
        EXPECT_NEAR(glm::length(rec), 1.0F, kEps);
    }
}

TEST(OctNormalTest, PackIsDeterministic) {
    glm::vec3 n = glm::normalize(glm::vec3(0.3F, -0.7F, 0.5F));
    glm::vec2 a = OctPackNormal(n);
    glm::vec2 b = OctPackNormal(n);
    EXPECT_EQ(a.x, b.x);
    EXPECT_EQ(a.y, b.y);
}

TEST(OctNormalTest, UnpackReturnsUnitVectorAtCorners) {
    std::array<glm::vec2, 4> corners = {{
        { 1.0F,  0.0F},
        {-1.0F,  0.0F},
        { 0.0F,  1.0F},
        { 0.0F, -1.0F},
    }};
    for (const auto& e : corners) {
        glm::vec3 n = OctUnpackNormal(e);
        EXPECT_NEAR(glm::length(n), 1.0F, kEps);
    }
}

}  // namespace
