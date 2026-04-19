#include "renderer/SmaaMath.h"

#include <gtest/gtest.h>

namespace bimeup {
namespace {

TEST(SmaaMathTest, LuminanceMatchesRec709Weights) {
    EXPECT_NEAR(renderer::SmaaLuminance({1.0F, 0.0F, 0.0F}), 0.2126F, 1e-5F);
    EXPECT_NEAR(renderer::SmaaLuminance({0.0F, 1.0F, 0.0F}), 0.7152F, 1e-5F);
    EXPECT_NEAR(renderer::SmaaLuminance({0.0F, 0.0F, 1.0F}), 0.0722F, 1e-5F);
    EXPECT_NEAR(renderer::SmaaLuminance({1.0F, 1.0F, 1.0F}), 1.0F, 1e-5F);
    EXPECT_NEAR(renderer::SmaaLuminance({0.0F, 0.0F, 0.0F}), 0.0F, 1e-5F);
}

TEST(SmaaMathTest, EdgeDetectedOnStrongLeftNeighbourDelta) {
    // L = 0.5, Lleft = 0.1 → deltaL = 0.4 >> threshold 0.1; no Y neighbour delta.
    const auto edges = renderer::SmaaDetectEdgeLuma(0.5F, 0.1F, 0.5F, 0.5F, 0.5F);
    EXPECT_TRUE(edges.x);
    EXPECT_FALSE(edges.y);
}

TEST(SmaaMathTest, EdgeDetectedOnStrongTopNeighbourDelta) {
    // L = 0.5, Ltop = 0.1 → deltaT = 0.4 >> threshold 0.1; no X neighbour delta.
    const auto edges = renderer::SmaaDetectEdgeLuma(0.5F, 0.5F, 0.1F, 0.5F, 0.5F);
    EXPECT_FALSE(edges.x);
    EXPECT_TRUE(edges.y);
}

TEST(SmaaMathTest, EdgeRejectedBelowAbsoluteThreshold) {
    // All per-neighbour deltas tiny (~0.005), well below default threshold 0.1.
    const auto edges = renderer::SmaaDetectEdgeLuma(0.5F, 0.495F, 0.503F, 0.498F, 0.5F);
    EXPECT_FALSE(edges.x);
    EXPECT_FALSE(edges.y);
}

TEST(SmaaMathTest, LocalContrastSuppressesWeakEdgeNeighbouringStrongerOne) {
    // Both axes clear the absolute threshold, but the Y-edge is much stronger:
    //   deltaL = 0.15 (passes 0.1), deltaT = 0.5 (dominant), others = 0
    //   maxNeighbourDelta = 0.5
    //   local contrast on X: 0.15 * 2.0 = 0.3 < 0.5 → suppressed
    //   local contrast on Y: 0.5  * 2.0 = 1.0 >= 0.5 → kept
    const auto edges = renderer::SmaaDetectEdgeLuma(0.5F, 0.35F, 0.0F, 0.5F, 0.5F);
    EXPECT_FALSE(edges.x);
    EXPECT_TRUE(edges.y);
}

TEST(SmaaMathTest, LocalContrastPreservesIndependentEdges) {
    // X-edge alone, no competing stronger neighbour → should survive suppression.
    //   deltaL = 0.3, all other neighbour deltas = 0; 0.3 * 2.0 = 0.6 >= 0.3 → keep.
    const auto edges = renderer::SmaaDetectEdgeLuma(0.5F, 0.2F, 0.5F, 0.5F, 0.5F);
    EXPECT_TRUE(edges.x);
    EXPECT_FALSE(edges.y);
}

TEST(SmaaMathTest, CustomThresholdGatesEdges) {
    // deltaL = 0.2 — above default 0.1 but below a stricter 0.25.
    const auto edgesDefault = renderer::SmaaDetectEdgeLuma(0.5F, 0.3F, 0.5F, 0.5F, 0.5F);
    EXPECT_TRUE(edgesDefault.x);
    const auto edgesStrict =
        renderer::SmaaDetectEdgeLuma(0.5F, 0.3F, 0.5F, 0.5F, 0.5F, 0.25F);
    EXPECT_FALSE(edgesStrict.x);
}

TEST(SmaaMathTest, CustomLocalContrastFactorChangesSuppression) {
    // deltaL = 0.15 (passes 0.1), deltaT = 0.5 (dominant).
    // With factor 2.0 (default): 0.15*2 = 0.3 < 0.5 → X suppressed.
    // With factor 4.0:           0.15*4 = 0.6 >= 0.5 → X preserved.
    const auto edgesDefault = renderer::SmaaDetectEdgeLuma(0.5F, 0.35F, 0.0F, 0.5F, 0.5F);
    EXPECT_FALSE(edgesDefault.x);
    const auto edgesRelaxed =
        renderer::SmaaDetectEdgeLuma(0.5F, 0.35F, 0.0F, 0.5F, 0.5F, 0.1F, 4.0F);
    EXPECT_TRUE(edgesRelaxed.x);
}

}  // namespace
}  // namespace bimeup
