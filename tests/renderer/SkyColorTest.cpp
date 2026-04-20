#include <gtest/gtest.h>

#include <renderer/SkyColor.h>

#include <glm/glm.hpp>

#include <string>

namespace {

using bimeup::renderer::ComputeSkyColor;
using bimeup::renderer::kSkyColorKeyCount;
using bimeup::renderer::kSkyColorKeyElevations;
using bimeup::renderer::SkyColor;
using bimeup::renderer::SkyColorKeyAt;

constexpr float kEps = 1e-5F;

void ExpectVec3Near(const glm::vec3& a, const glm::vec3& b, float eps) {
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
}

void ExpectSkyNear(const SkyColor& a, const SkyColor& b, float eps) {
    ExpectVec3Near(a.zenith, b.zenith, eps);
    ExpectVec3Near(a.horizon, b.horizon, eps);
    ExpectVec3Near(a.ground, b.ground, eps);
    ExpectVec3Near(a.sunColor, b.sunColor, eps);
}

TEST(SkyColorTest, KeyAccessorReturnsAllFiveKeys) {
    // Five keys exposed; the LUT order in the header is the source of truth.
    EXPECT_EQ(kSkyColorKeyCount, 5);
    for (int i = 0; i < kSkyColorKeyCount; ++i) {
        // Just verify each key is a sensible value (not all-zero unless
        // night). Real values are pinned through ComputeSkyColor below.
        SkyColor k = SkyColorKeyAt(i);
        EXPECT_GE(k.zenith.b, 0.0F) << "i=" << i;
    }
}

TEST(SkyColorTest, KeyAccessorClampsOutOfRange) {
    // Defensive: panel could feed a stale index.
    ExpectSkyNear(SkyColorKeyAt(-5), SkyColorKeyAt(0), kEps);
    ExpectSkyNear(SkyColorKeyAt(99), SkyColorKeyAt(kSkyColorKeyCount - 1),
                  kEps);
}

TEST(SkyColorTest, SamplingAtKeyElevationReturnsKeyExactly) {
    // C⁰ continuity: sampling exactly at a key elevation must return that
    // key bit-for-bit (modulo float precision in the lerp). Otherwise the
    // panel preview wouldn't agree with the live frame.
    for (int i = 0; i < kSkyColorKeyCount; ++i) {
        SCOPED_TRACE("key " + std::to_string(i));
        SkyColor sampled = ComputeSkyColor(kSkyColorKeyElevations[i]);
        SkyColor key = SkyColorKeyAt(i);
        ExpectSkyNear(sampled, key, kEps);
    }
}

TEST(SkyColorTest, BelowFirstKeyClampsToNight) {
    // Sun deep below the horizon: night LUT, no extrapolation.
    SkyColor below = ComputeSkyColor(-1.0F);  // -57°
    ExpectSkyNear(below, SkyColorKeyAt(0), kEps);
}

TEST(SkyColorTest, AboveLastKeyClampsToZenith) {
    // Sun directly overhead (e.g. tropics noon): zenith LUT, no
    // extrapolation past the table.
    SkyColor above = ComputeSkyColor(1.5F);  // ~86°
    ExpectSkyNear(above, SkyColorKeyAt(kSkyColorKeyCount - 1), kEps);
}

TEST(SkyColorTest, MidpointBetweenKeysIsLinearLerp) {
    // Verifies the interpolation is genuinely piecewise-linear. Picks every
    // adjacent pair and samples the midpoint.
    for (int i = 0; i + 1 < kSkyColorKeyCount; ++i) {
        SCOPED_TRACE("between key " + std::to_string(i) + " and " +
                     std::to_string(i + 1));
        const float lo = kSkyColorKeyElevations[i];
        const float hi = kSkyColorKeyElevations[i + 1];
        const float mid = 0.5F * (lo + hi);

        SkyColor sampled = ComputeSkyColor(mid);
        SkyColor a = SkyColorKeyAt(i);
        SkyColor b = SkyColorKeyAt(i + 1);
        SkyColor expected = {
            0.5F * (a.zenith + b.zenith),
            0.5F * (a.horizon + b.horizon),
            0.5F * (a.ground + b.ground),
            0.5F * (a.sunColor + b.sunColor),
        };
        ExpectSkyNear(sampled, expected, kEps);
    }
}

TEST(SkyColorTest, NightSunIsBlack) {
    // No direct sun contribution at night.
    SkyColor night = SkyColorKeyAt(0);
    EXPECT_EQ(night.sunColor.r, 0.0F);
    EXPECT_EQ(night.sunColor.g, 0.0F);
    EXPECT_EQ(night.sunColor.b, 0.0F);
}

TEST(SkyColorTest, GoldenHourSunIsWarm) {
    // Plan contract: "sunColor warm at low elevation". At the golden-hour
    // key the red channel must dominate the blue channel (warmth).
    SkyColor golden = SkyColorKeyAt(2);
    EXPECT_GT(golden.sunColor.r, golden.sunColor.b);
    EXPECT_GT(golden.sunColor.r, golden.sunColor.g);
}

TEST(SkyColorTest, ZenithSunIsNeutralWhite) {
    // Plan contract: "white at high elevation". At the zenith key all three
    // channels match within a tight tolerance.
    SkyColor z = SkyColorKeyAt(kSkyColorKeyCount - 1);
    EXPECT_NEAR(z.sunColor.r, z.sunColor.g, 0.01F);
    EXPECT_NEAR(z.sunColor.g, z.sunColor.b, 0.01F);
    EXPECT_NEAR(z.sunColor.r, 1.0F, 0.05F);
}

TEST(SkyColorTest, SunColourLuminanceMonotonicBetweenNightAndDay) {
    // Climbing from night through day, the sun's brightness must strictly
    // increase across the four ascending key boundaries.
    auto luma = [](glm::vec3 c) {
        return (0.2126F * c.r) + (0.7152F * c.g) + (0.0722F * c.b);
    };
    for (int i = 0; i + 1 < kSkyColorKeyCount; ++i) {
        EXPECT_LT(luma(SkyColorKeyAt(i).sunColor),
                  luma(SkyColorKeyAt(i + 1).sunColor))
            << "between key " << i << " and " << (i + 1);
    }
}

TEST(SkyColorTest, KeyElevationsStrictlyAscending) {
    // The lerp loop assumes ascending order. Defensive: if a future tweak
    // re-orders keys, this fires immediately rather than producing silent
    // garbage.
    for (int i = 0; i + 1 < kSkyColorKeyCount; ++i) {
        EXPECT_LT(kSkyColorKeyElevations[i], kSkyColorKeyElevations[i + 1])
            << "i=" << i;
    }
}

TEST(SkyColorTest, Deterministic) {
    SkyColor a = ComputeSkyColor(0.3F);
    SkyColor b = ComputeSkyColor(0.3F);
    EXPECT_EQ(a.zenith, b.zenith);
    EXPECT_EQ(a.horizon, b.horizon);
    EXPECT_EQ(a.ground, b.ground);
    EXPECT_EQ(a.sunColor, b.sunColor);
}

}  // namespace
