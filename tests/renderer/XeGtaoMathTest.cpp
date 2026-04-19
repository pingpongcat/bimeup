#include <gtest/gtest.h>

#include <renderer/XeGtaoMath.h>

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <numbers>

namespace {

using bimeup::renderer::XeGtaoSliceDirection;
using bimeup::renderer::XeGtaoSliceVisibility;

constexpr float kEps = 1e-5F;
constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kHalfPi = 0.5F * kPi;

TEST(XeGtaoMathTest, SliceDirectionUnitLength) {
    // Slice direction is a unit vector; the shader uses it verbatim to step
    // along screen space, so a non-unit result would bias tap spacing.
    constexpr std::uint32_t kN = 16;
    for (std::uint32_t i = 0; i < kN; ++i) {
        glm::vec2 d = XeGtaoSliceDirection(i, kN, 0.0F);
        EXPECT_NEAR(glm::length(d), 1.0F, kEps) << "i=" << i;
    }
}

TEST(XeGtaoMathTest, SliceDirectionFirstIsHorizontal) {
    // index 0, jitter 0 → (1, 0). Pins the origin of the slice sweep.
    glm::vec2 d = XeGtaoSliceDirection(0, 4, 0.0F);
    EXPECT_NEAR(d.x, 1.0F, kEps);
    EXPECT_NEAR(d.y, 0.0F, kEps);
}

TEST(XeGtaoMathTest, SliceDirectionCoversHalfCircle) {
    // N slices evenly cover [0, π): angle = π · i / N.
    constexpr std::uint32_t kN = 8;
    for (std::uint32_t i = 0; i < kN; ++i) {
        glm::vec2 d = XeGtaoSliceDirection(i, kN, 0.0F);
        float expected = kPi * static_cast<float>(i) / static_cast<float>(kN);
        EXPECT_NEAR(std::atan2(d.y, d.x), expected, kEps) << "i=" << i;
    }
}

TEST(XeGtaoMathTest, SliceDirectionJitterRotatesUniformly) {
    // jitter = 0.5 shifts every slice by π / (2N) relative to jitter = 0.
    constexpr std::uint32_t kN = 4;
    float rot = kPi * 0.5F / static_cast<float>(kN);
    for (std::uint32_t i = 0; i < kN; ++i) {
        glm::vec2 base = XeGtaoSliceDirection(i, kN, 0.0F);
        glm::vec2 jit = XeGtaoSliceDirection(i, kN, 0.5F);
        float aBase = std::atan2(base.y, base.x);
        float aJit = std::atan2(jit.y, jit.x);
        EXPECT_NEAR(aJit - aBase, rot, kEps) << "i=" << i;
    }
}

TEST(XeGtaoMathTest, SliceVisibilityFullyOpenIsOne) {
    // n = 0, horizons at ±π/2, projLen = 1: full hemisphere → 1.
    // 0.25 · (−cos(−π) + 1 + 0 − cos(π) + 1 + 0) = 0.25 · 4 = 1.
    float v = XeGtaoSliceVisibility(-kHalfPi, kHalfPi, 0.0F, 1.0F);
    EXPECT_NEAR(v, 1.0F, kEps);
}

TEST(XeGtaoMathTest, SliceVisibilityFullyOccludedIsZero) {
    // Horizons coincident with the view direction pinch the hemisphere shut.
    float v = XeGtaoSliceVisibility(0.0F, 0.0F, 0.0F, 1.0F);
    EXPECT_NEAR(v, 0.0F, kEps);
}

TEST(XeGtaoMathTest, SliceVisibilityProjectedLengthZeroIsZero) {
    // Normal perpendicular to slice plane: slice contributes 0 regardless
    // of horizon geometry. Guards the `projLen · visArea` factor.
    float v = XeGtaoSliceVisibility(-kPi * 0.25F, kPi * 0.25F, 0.0F, 0.0F);
    EXPECT_NEAR(v, 0.0F, kEps);
}

TEST(XeGtaoMathTest, SliceVisibilityScalesLinearlyWithProjectedLength) {
    // Per-slice output is projLen · visArea.
    float full = XeGtaoSliceVisibility(-kPi * 0.25F, kPi * 0.25F, 0.0F, 1.0F);
    float half = XeGtaoSliceVisibility(-kPi * 0.25F, kPi * 0.25F, 0.0F, 0.5F);
    EXPECT_NEAR(half, 0.5F * full, kEps);
}

TEST(XeGtaoMathTest, SliceVisibilityAnalyticalSymmetricUprightNormal) {
    // For n = 0, h1 = −θ, h2 = +θ, projLen = 1 the integral collapses to
    //   visArea = 0.25 · 2 · (1 − cos(2θ)) = sin²(θ).
    // Reference derivation: the sin(0) terms vanish, cos(±2θ) = cos(2θ).
    const float kThetas[] = {kPi / 12.0F, kPi / 6.0F, kPi / 4.0F, kPi / 3.0F,
                             5.0F * kPi / 12.0F};
    for (float theta : kThetas) {
        float v = XeGtaoSliceVisibility(-theta, theta, 0.0F, 1.0F);
        float ref = std::sin(theta) * std::sin(theta);
        EXPECT_NEAR(v, ref, kEps) << "theta=" << theta;
    }
}

TEST(XeGtaoMathTest, SliceVisibilityMonotonicInHorizonOpening) {
    // Opening the horizon can't decrease visibility — guards against a
    // sign-flip in the integrand.
    float tight = XeGtaoSliceVisibility(-0.2F, 0.2F, 0.0F, 1.0F);
    float mid = XeGtaoSliceVisibility(-0.5F, 0.5F, 0.0F, 1.0F);
    float wide = XeGtaoSliceVisibility(-1.0F, 1.0F, 0.0F, 1.0F);
    EXPECT_LT(tight, mid);
    EXPECT_LT(mid, wide);
}

TEST(XeGtaoMathTest, SliceVisibilityClampsHorizonsToNormalHemisphere) {
    // Horizon inputs past n ± π/2 reach behind the surface — both bounds
    // should clamp, matching the GLSL clamp in `ssao_xegtao.comp`.
    float n = 0.3F;
    float atBoundary = XeGtaoSliceVisibility(n - kHalfPi, n + kHalfPi, n, 1.0F);
    float wellPast = XeGtaoSliceVisibility(n - kPi, n + kPi, n, 1.0F);
    EXPECT_NEAR(atBoundary, wellPast, kEps);
}

TEST(XeGtaoMathTest, SliceVisibilityDeterministic) {
    // Pure function — identical inputs return identical outputs bit-for-bit.
    float a = XeGtaoSliceVisibility(-0.4F, 0.6F, 0.1F, 0.8F);
    float b = XeGtaoSliceVisibility(-0.4F, 0.6F, 0.1F, 0.8F);
    EXPECT_EQ(a, b);
}

}  // namespace
