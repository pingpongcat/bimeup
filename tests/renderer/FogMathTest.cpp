#include <gtest/gtest.h>

#include <renderer/FogMath.h>

namespace {

using bimeup::renderer::ComputeFog;

constexpr float kEps = 1e-5F;

TEST(FogMathTest, NoFogBeforeStart) {
    // Any view-space distance shorter than `start` must emit zero fog — the
    // tonemap's `mix(colour, fogColour, factor)` then leaves the pixel
    // untouched. Covers the "near objects stay crisp" contract.
    EXPECT_NEAR(ComputeFog(0.0F, 20.0F, 120.0F), 0.0F, kEps);
    EXPECT_NEAR(ComputeFog(10.0F, 20.0F, 120.0F), 0.0F, kEps);
    EXPECT_NEAR(ComputeFog(19.999F, 20.0F, 120.0F), 0.0F, kEps);
}

TEST(FogMathTest, FogFactorAtStartIsZero) {
    // Exactly at the start distance, factor = 0 — the ramp must begin at the
    // user-tuned distance, not one epsilon short of it. A failing case here
    // would create a visible seam at the near edge of the fog transition.
    EXPECT_NEAR(ComputeFog(20.0F, 20.0F, 120.0F), 0.0F, kEps);
}

TEST(FogMathTest, FogFactorAtEndIsOne) {
    // Exactly at the end distance, factor = 1 — pixels at or beyond `end` are
    // fully replaced by fog colour. Guards against off-by-one precision
    // errors at the far edge of the ramp.
    EXPECT_NEAR(ComputeFog(120.0F, 20.0F, 120.0F), 1.0F, kEps);
}

TEST(FogMathTest, SaturatesBeyondEnd) {
    // Beyond `end`, the factor must clamp at 1, not grow linearly — otherwise
    // the `mix()` would produce over-brightened or negative colours for very
    // distant geometry. This is the "far clamp" half of the saturate contract.
    EXPECT_NEAR(ComputeFog(200.0F, 20.0F, 120.0F), 1.0F, kEps);
    EXPECT_NEAR(ComputeFog(1000.0F, 20.0F, 120.0F), 1.0F, kEps);
}

TEST(FogMathTest, LinearRampMidpoint) {
    // The midpoint of the [start, end] range must return exactly 0.5 — this
    // pins the ramp as linear (not quadratic, not exponential). A future
    // switch to exponential fog would intentionally fail this test.
    EXPECT_NEAR(ComputeFog(70.0F, 20.0F, 120.0F), 0.5F, kEps);
}

TEST(FogMathTest, LinearRampQuarterPoints) {
    // Quarter and three-quarter points pin the ramp slope — a shader that
    // accidentally squared or square-rooted the factor would fail these
    // asymmetrically while the midpoint test still passed.
    EXPECT_NEAR(ComputeFog(45.0F, 20.0F, 120.0F), 0.25F, kEps);
    EXPECT_NEAR(ComputeFog(95.0F, 20.0F, 120.0F), 0.75F, kEps);
}

TEST(FogMathTest, NegativeViewZClampsToZero) {
    // `viewZ` is view-space distance — always positive in practice, but a
    // pixel at exactly the camera origin or a numeric underflow could go
    // slightly negative. The clamp-to-zero behaviour keeps the mix stable
    // rather than producing a negative fog weight.
    EXPECT_NEAR(ComputeFog(-5.0F, 20.0F, 120.0F), 0.0F, kEps);
}

TEST(FogMathTest, MonotonicWithinRange) {
    // Strictly-monotonic ramp inside [start, end] — later distances must be
    // foggier than earlier ones. A bug that swapped `start` and `end` in the
    // subtraction would invert this and fail at any pair.
    float a = ComputeFog(30.0F, 20.0F, 120.0F);
    float b = ComputeFog(60.0F, 20.0F, 120.0F);
    float c = ComputeFog(90.0F, 20.0F, 120.0F);
    EXPECT_LT(a, b);
    EXPECT_LT(b, c);
}

TEST(FogMathTest, DegenerateRangeCollapsesToStep) {
    // When `start == end`, the division-by-zero guard must degrade to a step
    // function at that distance — below returns 0, at-or-above returns 1.
    // This keeps the panel safe when a user drags the two sliders together.
    EXPECT_NEAR(ComputeFog(9.9F, 10.0F, 10.0F), 0.0F, kEps);
    EXPECT_NEAR(ComputeFog(10.0F, 10.0F, 10.0F), 1.0F, kEps);
    EXPECT_NEAR(ComputeFog(50.0F, 10.0F, 10.0F), 1.0F, kEps);
}

}  // namespace
