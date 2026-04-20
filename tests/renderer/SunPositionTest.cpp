#include <gtest/gtest.h>

#include <renderer/SunPosition.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <numbers>

namespace {

using bimeup::renderer::ComputeSunDirection;
using bimeup::renderer::SunPosition;

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kDegToRad = kPi / 180.0;

// JD reference epoch values (UTC). Computed via the standard
// Meeus/Calendrical-Calculations conversion; cross-checked against the NOAA
// online solar calculator.
//   2024-03-20 12:00:00 UTC → 2460390.0 (Greenwich vernal-equinox local noon)
//   2024-06-21 10:37:30 UTC → 2460482.94271 (Warsaw summer-solstice solar noon)
//   2024-12-21 10:38:00 UTC → 2460665.94306 (Warsaw winter-solstice solar noon)
//   2024-03-21 00:00:00 UTC → 2460390.5 (Warsaw local midnight, sun below)
constexpr double kJdGreenwichEquinoxNoon = 2460390.0;
constexpr double kJdWarsawSummerSolarNoon = 2460482.94271;
constexpr double kJdWarsawWinterSolarNoon = 2460665.94306;
constexpr double kJdEquinoxMidnight = 2460390.5;

constexpr double kLatGreenwich = 51.4779 * kDegToRad;
constexpr double kLonGreenwich = 0.0;
constexpr double kLatWarsaw = 52.2297 * kDegToRad;
constexpr double kLonWarsaw = 21.0122 * kDegToRad;

TEST(SunPositionTest, GreenwichEquinoxNoonElevationMatchesColatitude) {
    // At a true equinox noon at Greenwich the sun sits on the local meridian
    // with declination ≈ 0, so elevation ≈ 90° − latitude. UTC noon on
    // 2024-03-20 sits ~7 min before solar noon (EoT ≈ −7 min), pulling the
    // elevation a fraction lower; ±1° tolerance covers it.
    SunPosition s = ComputeSunDirection(kJdGreenwichEquinoxNoon, kLatGreenwich,
                                        kLonGreenwich, 0.0);
    float expected = static_cast<float>((90.0 * kDegToRad) - kLatGreenwich);
    EXPECT_NEAR(s.elevation, expected, 1.0F * static_cast<float>(kDegToRad));
}

TEST(SunPositionTest, WarsawSummerSolarNoonElevationApproxSixtyOne) {
    // Local solar noon at the summer solstice: elevation = 90° − |lat − δ|
    // = 90° − (52.23° − 23.44°) ≈ 61.21°.
    SunPosition s = ComputeSunDirection(kJdWarsawSummerSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.0);
    float expected = static_cast<float>(61.21 * kDegToRad);
    EXPECT_NEAR(s.elevation, expected, 1.0F * static_cast<float>(kDegToRad));
}

TEST(SunPositionTest, WarsawWinterSolarNoonElevationApproxFourteen) {
    // Local solar noon at the winter solstice: elevation
    // = 90° − (52.23° + 23.44°) ≈ 14.33°. Low winter sun.
    SunPosition s = ComputeSunDirection(kJdWarsawWinterSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.0);
    float expected = static_cast<float>(14.33 * kDegToRad);
    EXPECT_NEAR(s.elevation, expected, 1.0F * static_cast<float>(kDegToRad));
}

TEST(SunPositionTest, WarsawSummerSolarNoonAzimuthIsSouth) {
    // At local solar noon in the northern hemisphere the sun is due south,
    // i.e. compass azimuth ≈ 180° (CW from north). Tolerance accommodates
    // the small offset between our pinned JD and the exact solar-noon
    // instant.
    SunPosition s = ComputeSunDirection(kJdWarsawSummerSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.0);
    float expected = static_cast<float>(180.0 * kDegToRad);
    EXPECT_NEAR(s.azimuth, expected, 5.0F * static_cast<float>(kDegToRad));
}

TEST(SunPositionTest, MidnightSunBelowHorizon) {
    // Equinox local midnight at Warsaw: sun is on the opposite side of the
    // Earth, elevation must be negative.
    SunPosition s = ComputeSunDirection(kJdEquinoxMidnight, kLatWarsaw,
                                        kLonWarsaw, 0.0);
    EXPECT_LT(s.elevation, 0.0F);
}

TEST(SunPositionTest, DirWorldIsUnitLength) {
    // Direction must be unit (downstream Lambert math assumes it).
    SunPosition s = ComputeSunDirection(kJdWarsawSummerSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.0);
    EXPECT_NEAR(glm::length(s.dirWorld), 1.0F, 1e-5F);
}

TEST(SunPositionTest, DirWorldPointsDownwardWhenSunIsUp) {
    // `dirWorld` points FROM the sun toward the scene, matching
    // `DirectionalLight.direction`. Sun above horizon ⇒ y-component < 0.
    SunPosition s = ComputeSunDirection(kJdWarsawSummerSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.0);
    ASSERT_GT(s.elevation, 0.0F);
    EXPECT_LT(s.dirWorld.y, 0.0F);
}

TEST(SunPositionTest, AzimuthInZeroToTwoPi) {
    // Azimuth is the normalised compass bearing.
    for (double jdOffset : {0.0, 0.1, 0.25, 0.4, 0.6, 0.85}) {
        SunPosition s = ComputeSunDirection(
            kJdWarsawSummerSolarNoon + jdOffset, kLatWarsaw, kLonWarsaw, 0.0);
        EXPECT_GE(s.azimuth, 0.0F) << "jdOffset=" << jdOffset;
        EXPECT_LT(s.azimuth, static_cast<float>(2.0 * kPi))
            << "jdOffset=" << jdOffset;
    }
}

TEST(SunPositionTest, TrueNorthOffsetRotatesAzimuthOneToOne) {
    // Rotating the model frame by +θ around +Y must shift the reported
    // azimuth by +θ (mod 2π) and rotate `dirWorld` by the same Y-axis
    // rotation. The sun in the sky doesn't move when only the model
    // orientation changes — the model frame does.
    constexpr double kTheta = kPi / 4.0;  // 45°

    SunPosition baseS = ComputeSunDirection(kJdWarsawSummerSolarNoon,
                                            kLatWarsaw, kLonWarsaw, 0.0);
    SunPosition rotS = ComputeSunDirection(kJdWarsawSummerSolarNoon,
                                           kLatWarsaw, kLonWarsaw, kTheta);

    float expectedAz = baseS.azimuth + static_cast<float>(kTheta);
    if (expectedAz >= static_cast<float>(2.0 * kPi)) {
        expectedAz -= static_cast<float>(2.0 * kPi);
    }
    EXPECT_NEAR(rotS.azimuth, expectedAz, 1e-4F);

    // CW rotation around +Y by +θ in our convention (azimuth measured CW
    // from +Z): (x, z) → (x·cos θ + z·sin θ, −x·sin θ + z·cos θ). y is
    // untouched.
    float c = std::cos(static_cast<float>(kTheta));
    float s = std::sin(static_cast<float>(kTheta));
    glm::vec3 expectedDir(baseS.dirWorld.x * c + baseS.dirWorld.z * s,
                          baseS.dirWorld.y,
                          -baseS.dirWorld.x * s + baseS.dirWorld.z * c);
    EXPECT_NEAR(rotS.dirWorld.x, expectedDir.x, 1e-5F);
    EXPECT_NEAR(rotS.dirWorld.y, expectedDir.y, 1e-5F);
    EXPECT_NEAR(rotS.dirWorld.z, expectedDir.z, 1e-5F);
}

TEST(SunPositionTest, AzimuthMatchesDirWorldHorizontalProjection) {
    // Internal consistency: the horizontal projection of `dirWorld` (FROM
    // sun) negated gives the TO-sun horizontal direction, which by our
    // azimuth convention equals (sin az, _, cos az). Catches mismatches
    // between the azimuth and dirWorld assembly paths.
    SunPosition state = ComputeSunDirection(kJdWarsawSummerSolarNoon,
                                            kLatWarsaw, kLonWarsaw, 0.0);
    float az = state.azimuth;
    float cosEl = std::cos(state.elevation);
    EXPECT_NEAR(-state.dirWorld.x, std::sin(az) * cosEl, 1e-5F);
    EXPECT_NEAR(-state.dirWorld.z, std::cos(az) * cosEl, 1e-5F);
}

TEST(SunPositionTest, Deterministic) {
    // Pure function — identical inputs yield bit-identical output. The
    // panel re-packs every frame, so any non-determinism would re-upload
    // the lighting UBO unnecessarily.
    SunPosition a = ComputeSunDirection(kJdWarsawSummerSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.123);
    SunPosition b = ComputeSunDirection(kJdWarsawSummerSolarNoon, kLatWarsaw,
                                        kLonWarsaw, 0.123);
    EXPECT_EQ(a.elevation, b.elevation);
    EXPECT_EQ(a.azimuth, b.azimuth);
    EXPECT_EQ(a.dirWorld.x, b.dirWorld.x);
    EXPECT_EQ(a.dirWorld.y, b.dirWorld.y);
    EXPECT_EQ(a.dirWorld.z, b.dirWorld.z);
}

}  // namespace
