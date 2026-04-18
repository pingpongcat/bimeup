#include <gtest/gtest.h>

#include <renderer/Tonemap.h>

#include <glm/glm.hpp>

namespace {

using bimeup::renderer::AcesTonemap;

constexpr float kEps = 1e-4F;

// Reference values for the Narkowicz ACES-fitted curve
// f(x) = clamp((x*(2.51*x + 0.03)) / (x*(2.43*x + 0.59) + 0.14), 0, 1)
//   f(0.00) = 0.0
//   f(0.18) = 0.266904...
//   f(1.00) = 0.803797...
//   f(10.0) = 1.00907... -> clamps to 1.0

TEST(TonemapTest, AcesAtZeroIsZero) {
    glm::vec3 out = AcesTonemap(glm::vec3(0.0F));
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(TonemapTest, AcesAtMiddleGrayMatchesReference) {
    glm::vec3 out = AcesTonemap(glm::vec3(0.18F));
    EXPECT_NEAR(out.r, 0.266904F, kEps);
    EXPECT_NEAR(out.g, 0.266904F, kEps);
    EXPECT_NEAR(out.b, 0.266904F, kEps);
}

TEST(TonemapTest, AcesAtWhiteMatchesReference) {
    glm::vec3 out = AcesTonemap(glm::vec3(1.0F));
    EXPECT_NEAR(out.r, 0.803797F, kEps);
    EXPECT_NEAR(out.g, 0.803797F, kEps);
    EXPECT_NEAR(out.b, 0.803797F, kEps);
}

TEST(TonemapTest, AcesAtTenClampsToOne) {
    glm::vec3 out = AcesTonemap(glm::vec3(10.0F));
    EXPECT_NEAR(out.r, 1.0F, kEps);
    EXPECT_NEAR(out.g, 1.0F, kEps);
    EXPECT_NEAR(out.b, 1.0F, kEps);
}

TEST(TonemapTest, AcesClampsNegativeInputToZero) {
    glm::vec3 out = AcesTonemap(glm::vec3(-1.0F));
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(TonemapTest, AcesChannelsAreIndependent) {
    // Per-channel curve: r/g/b should map through the scalar curve independently.
    glm::vec3 out = AcesTonemap(glm::vec3(0.0F, 0.18F, 1.0F));
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.266904F, kEps);
    EXPECT_NEAR(out.b, 0.803797F, kEps);
}

TEST(TonemapTest, AcesIsMonotonicOnPositives) {
    glm::vec3 a = AcesTonemap(glm::vec3(0.05F));
    glm::vec3 b = AcesTonemap(glm::vec3(0.5F));
    glm::vec3 c = AcesTonemap(glm::vec3(2.0F));
    EXPECT_LT(a.r, b.r);
    EXPECT_LT(b.r, c.r);
}

}  // namespace
