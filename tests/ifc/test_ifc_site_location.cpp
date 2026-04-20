#include <gtest/gtest.h>

#include <ifc/IfcModel.h>
#include <ifc/IfcSiteLocation.h>

#include <cmath>
#include <numbers>

namespace {

using bimeup::ifc::DmsToRadians;
using bimeup::ifc::ExtractSiteLocation;
using bimeup::ifc::IfcModel;
using bimeup::ifc::SiteLocation;
using bimeup::ifc::TrueNorthAngleFromDirection;

constexpr double kPi = std::numbers::pi_v<double>;
constexpr double kHalfPi = kPi * 0.5;
constexpr double kDegToRad = kPi / 180.0;

constexpr const char* kFullSampleIfc = TEST_DATA_DIR "/example.ifc";
constexpr const char* kNoSiteIfc = TEST_DATA_DIR "/site_no_site.ifc";
constexpr const char* kNoLatIfc = TEST_DATA_DIR "/site_no_lat.ifc";

TEST(DmsToRadiansTest, AllZerosIsZero) {
    EXPECT_EQ(DmsToRadians(0, 0, 0, 0), 0.0);
}

TEST(DmsToRadiansTest, IntegerDegreesOnly) {
    // 90° → π/2.
    EXPECT_NEAR(DmsToRadians(90, 0, 0, 0), kHalfPi, 1e-12);
}

TEST(DmsToRadiansTest, MinutesAreSixtieths) {
    // 0°30'00" → 0.5° → 0.5 · π/180.
    EXPECT_NEAR(DmsToRadians(0, 30, 0, 0), 0.5 * kDegToRad, 1e-12);
}

TEST(DmsToRadiansTest, SecondsAreThirtySixHundredths) {
    // 0°00'36" → 0.01° → 0.01 · π/180.
    EXPECT_NEAR(DmsToRadians(0, 0, 36, 0), 0.01 * kDegToRad, 1e-12);
}

TEST(DmsToRadiansTest, MicroSecondsScaleByMillion) {
    // 0°00'00.000001" → (1/(3600·1e6))° → tiny but exact.
    const double expected = (1.0 / (3600.0 * 1.0e6)) * kDegToRad;
    EXPECT_NEAR(DmsToRadians(0, 0, 0, 1), expected, 1e-20);
}

TEST(DmsToRadiansTest, NegativeShareSignAcrossComponents) {
    // example.ifc longitude (-71, -1, -58, -789672)
    //   = -(71 + 1/60 + 58.789672/3600)° = -71.0330° (approx).
    const double rad = DmsToRadians(-71, -1, -58, -789672);
    const double expectedDeg = -(71.0 + (1.0 / 60.0) +
                                 (58.789672 / 3600.0));
    EXPECT_NEAR(rad, expectedDeg * kDegToRad, 1e-9);
    EXPECT_LT(rad, 0.0);
}

TEST(DmsToRadiansTest, FullExampleLatitudeMatchesReference) {
    // example.ifc latitude (42, 12, 46, 804504) ≈ 42.21300°.
    const double rad = DmsToRadians(42, 12, 46, 804504);
    const double expectedDeg = 42.0 + (12.0 / 60.0) +
                               (46.804504 / 3600.0);
    EXPECT_NEAR(rad, expectedDeg * kDegToRad, 1e-9);
}

TEST(TrueNorthAngleFromDirectionTest, PlusYIsZero) {
    EXPECT_NEAR(TrueNorthAngleFromDirection(0.0, 1.0), 0.0, 1e-12);
}

TEST(TrueNorthAngleFromDirectionTest, PlusXIsMinusHalfPi) {
    // True north along local +X (east) means the model is rotated 90° CW
    // from true north — angle CCW from local +Y is −π/2.
    EXPECT_NEAR(TrueNorthAngleFromDirection(1.0, 0.0), -kHalfPi, 1e-12);
}

TEST(TrueNorthAngleFromDirectionTest, MinusXIsPlusHalfPi) {
    EXPECT_NEAR(TrueNorthAngleFromDirection(-1.0, 0.0), kHalfPi, 1e-12);
}

TEST(TrueNorthAngleFromDirectionTest, MinusYIsPi) {
    EXPECT_NEAR(std::abs(TrueNorthAngleFromDirection(0.0, -1.0)), kPi,
                1e-12);
}

TEST(TrueNorthAngleFromDirectionTest, NearZeroXTreatedAsAlignedWithY) {
    // example.ifc TrueNorth = (6.12e-17, 1) — floating-point grit, treat as
    // aligned with +Y.
    EXPECT_NEAR(TrueNorthAngleFromDirection(6.12303176911189e-17, 1.0), 0.0,
                1e-15);
}

TEST(ExtractSiteLocationTest, FullSampleReturnsExpectedSite) {
    // example.ifc has IFCSITE(..., (42,12,46,804504), (-71,-1,-58,-789672),
    //                         0., ..., #11=context with TrueNorth ≈ (0,1)).
    IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kFullSampleIfc));

    auto loc = ExtractSiteLocation(model);
    ASSERT_TRUE(loc.has_value());

    const double latDeg = 42.0 + (12.0 / 60.0) + (46.804504 / 3600.0);
    const double lonDeg = -(71.0 + (1.0 / 60.0) + (58.789672 / 3600.0));
    EXPECT_NEAR(loc->latitudeRad, latDeg * kDegToRad, 1e-9);
    EXPECT_NEAR(loc->longitudeRad, lonDeg * kDegToRad, 1e-9);
    EXPECT_EQ(loc->elevationM, 0.0);
    EXPECT_NEAR(loc->trueNorthRad, 0.0, 1e-15);
}

TEST(ExtractSiteLocationTest, NoSiteReturnsNullopt) {
    IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kNoSiteIfc));
    EXPECT_FALSE(ExtractSiteLocation(model).has_value());
}

TEST(ExtractSiteLocationTest, MissingLatitudeReturnsNullopt) {
    IfcModel model;
    ASSERT_TRUE(model.LoadFromFile(kNoLatIfc));
    EXPECT_FALSE(ExtractSiteLocation(model).has_value());
}

TEST(ExtractSiteLocationTest, UnloadedModelReturnsNullopt) {
    IfcModel model;
    EXPECT_FALSE(ExtractSiteLocation(model).has_value());
}

}  // namespace
