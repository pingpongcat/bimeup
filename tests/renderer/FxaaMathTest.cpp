#include <gtest/gtest.h>

#include <renderer/FxaaMath.h>

#include <glm/glm.hpp>

namespace {

using bimeup::renderer::FxaaIsEdge;
using bimeup::renderer::FxaaLocalContrast;
using bimeup::renderer::FxaaLuminance;

constexpr float kEps = 1e-5F;

TEST(FxaaMathTest, LuminanceOfBlackIsZero) {
    // Pure black input — any weighted sum of zeros must be zero. Guards against
    // a stray bias/offset creeping into the weights (e.g. the shader accidentally
    // adding a compile-time constant).
    EXPECT_NEAR(FxaaLuminance(glm::vec3(0.0F)), 0.0F, kEps);
}

TEST(FxaaMathTest, LuminanceOfWhiteIsOne) {
    // Pure white input — the Rec.709 weights (0.2126, 0.7152, 0.0722) sum to 1
    // exactly by construction, so white saturates the 0–1 luma range. Failure
    // here means the weights drifted (e.g. someone swapped to Rec.601).
    EXPECT_NEAR(FxaaLuminance(glm::vec3(1.0F)), 1.0F, kEps);
}

TEST(FxaaMathTest, LuminanceMatchesRec709Weights) {
    // Individual RGB primaries return their Rec.709 coefficients. Pins each
    // channel weight independently — an accidental channel swap (R↔B, common
    // when porting BGR references) would fail one of these three.
    EXPECT_NEAR(FxaaLuminance(glm::vec3(1.0F, 0.0F, 0.0F)), 0.2126F, kEps);
    EXPECT_NEAR(FxaaLuminance(glm::vec3(0.0F, 1.0F, 0.0F)), 0.7152F, kEps);
    EXPECT_NEAR(FxaaLuminance(glm::vec3(0.0F, 0.0F, 1.0F)), 0.0722F, kEps);
}

TEST(FxaaMathTest, LuminanceGrayscaleIsLinear) {
    // rgb(x, x, x) must return x for any x — the weights sum to 1, so grayscale
    // is a fixed point. This is the invariant `fxaa.frag`'s edge detector
    // relies on to keep flat grey regions from being falsely flagged as edges.
    for (float x : {0.1F, 0.25F, 0.5F, 0.75F, 0.9F}) {
        EXPECT_NEAR(FxaaLuminance(glm::vec3(x)), x, kEps) << "x = " << x;
    }
}

TEST(FxaaMathTest, LocalContrastFlatPatchIsZero) {
    // When centre and all four NESW neighbours have identical luma, the local
    // contrast range is 0 — FXAA's early-exit test hinges on this being a
    // sharp zero (no precision slop) so uniform regions skip the AA work.
    EXPECT_NEAR(FxaaLocalContrast(0.5F, 0.5F, 0.5F, 0.5F, 0.5F), 0.0F, kEps);
    EXPECT_NEAR(FxaaLocalContrast(0.0F, 0.0F, 0.0F, 0.0F, 0.0F), 0.0F, kEps);
    EXPECT_NEAR(FxaaLocalContrast(1.0F, 1.0F, 1.0F, 1.0F, 1.0F), 0.0F, kEps);
}

TEST(FxaaMathTest, LocalContrastIsRangeOverFive) {
    // Explicit max-min over the 5-sample NESW+C cross. The centre sample must
    // participate in both the max and the min reductions — a shader that
    // accidentally ran max(N,S,E,W) without including C would miss edges where
    // the centre is the darkest/brightest of the five.
    EXPECT_NEAR(FxaaLocalContrast(0.5F, 0.1F, 0.2F, 0.3F, 0.4F),
                0.5F - 0.1F, kEps);  // centre = max
    EXPECT_NEAR(FxaaLocalContrast(0.05F, 0.1F, 0.2F, 0.3F, 0.4F),
                0.4F - 0.05F, kEps);  // centre = min
    EXPECT_NEAR(FxaaLocalContrast(0.25F, 0.1F, 0.2F, 0.3F, 0.4F),
                0.4F - 0.1F, kEps);  // centre is interior
}

TEST(FxaaMathTest, IsEdgeRejectsFlatPatch) {
    // A perfectly flat luma patch is never an edge, regardless of threshold
    // tuning — the range is zero, which is strictly less than any positive
    // `edgeThresholdMin`. This is the FXAA 3.11 early-exit path that keeps
    // the AA cost off the 90%+ of screen that's interior.
    EXPECT_FALSE(FxaaIsEdge(0.5F, 0.5F, 0.5F, 0.5F, 0.5F, 0.166F, 0.0833F));
    EXPECT_FALSE(FxaaIsEdge(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.166F, 0.0833F));
    EXPECT_FALSE(FxaaIsEdge(1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 0.166F, 0.0833F));
}

TEST(FxaaMathTest, IsEdgeDetectsSharpTransition) {
    // A strong luma step across the cross (centre bright, half the neighbours
    // dark) must trip the edge predicate at default HIGH-quality thresholds.
    // range = 1.0 - 0.0 = 1.0, which dominates both `edgeThresholdMin` (0.0833)
    // and `lumaMax * edgeThreshold` (1.0 * 0.166 = 0.166).
    EXPECT_TRUE(FxaaIsEdge(1.0F, 0.0F, 1.0F, 0.0F, 1.0F, 0.166F, 0.0833F));
}

TEST(FxaaMathTest, IsEdgeRespectsMinThresholdInDarkRegions) {
    // In a dark region (lumaMax ≈ 0.05), the relative `lumaMax * edgeThreshold`
    // term would be ~0.008 — tiny enough that shot noise or quantisation could
    // trigger the edge detector on pure darkness. The absolute
    // `edgeThresholdMin` floor prevents that: a 0.02 range in a dark patch is
    // below the 0.0833 floor and must be classified as flat.
    EXPECT_FALSE(FxaaIsEdge(0.05F, 0.03F, 0.05F, 0.04F, 0.05F, 0.166F, 0.0833F));
}

TEST(FxaaMathTest, IsEdgeScalesWithLumaMaxInBrightRegions) {
    // In a bright region (lumaMax = 1.0), the `lumaMax * edgeThreshold` term
    // dominates at 0.166, which is stricter than the absolute floor (0.0833).
    // A 0.1 range between bright values is visually flat (relative contrast
    // is only 10%), so it must be rejected — this is why FXAA uses a scaled
    // threshold rather than just an absolute floor.
    EXPECT_FALSE(FxaaIsEdge(1.0F, 0.9F, 1.0F, 0.95F, 1.0F, 0.166F, 0.0833F));
    // But a 0.2 range at the same brightness clears `lumaMax * edgeThreshold`
    // (0.166) and is a real edge.
    EXPECT_TRUE(FxaaIsEdge(1.0F, 0.8F, 1.0F, 0.85F, 1.0F, 0.166F, 0.0833F));
}

}  // namespace
