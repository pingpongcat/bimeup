#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <renderer/BloomMath.h>

namespace {

using bimeup::renderer::BloomDownsample;
using bimeup::renderer::BloomPrefilter;
using bimeup::renderer::BloomUpsample;

constexpr float kEps = 1e-4F;

// ---------------------------------------------------------------------------
// BloomPrefilter — soft-knee threshold applied before bloom downsample.
// ---------------------------------------------------------------------------

TEST(BloomMathTest, PrefilterBlackStaysBlack) {
    // Black input has zero max-channel luma — the contribution factor must
    // collapse to 0 for any finite threshold/knee. Guards against the `luma`
    // epsilon in the divisor leaking a non-zero term through a true-black
    // pixel and turning the scene backdrop into a bloom source.
    glm::vec3 out = BloomPrefilter(glm::vec3{0.0F}, 1.0F, 0.5F);
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(BloomMathTest, PrefilterBelowKneeReturnsZero) {
    // A pixel whose max-channel luma sits further below the threshold than
    // the knee width must be fully rejected — both the soft-knee branch and
    // the hard `luma - threshold` branch are ≤ 0, so the final max() stays
    // at 0. Pins the "no bloom from sub-threshold scene light" contract.
    glm::vec3 out = BloomPrefilter(glm::vec3{0.2F, 0.1F, 0.05F}, 1.0F, 0.3F);
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(BloomMathTest, PrefilterFarAboveThresholdApproachesHardThreshold) {
    // Far above threshold (luma ≫ threshold + knee), the soft branch is
    // dominated by the hard `luma - threshold` branch, so contribution ≈
    // (luma - threshold) / luma. At luma=4.0, threshold=1.0 → factor 0.75;
    // the RGB vector premultiplies by that. Pins the HDR-tail behaviour —
    // a very bright light keeps 75% of its energy through the prefilter.
    glm::vec3 color{4.0F, 4.0F, 4.0F};
    glm::vec3 out = BloomPrefilter(color, 1.0F, 0.5F);
    float expected = 4.0F * (3.0F / 4.0F);  // luma=4, (luma-thresh)/luma = 0.75
    EXPECT_NEAR(out.r, expected, kEps);
    EXPECT_NEAR(out.g, expected, kEps);
    EXPECT_NEAR(out.b, expected, kEps);
}

TEST(BloomMathTest, PrefilterAtThresholdIsZero) {
    // Exactly at threshold with knee=0, both branches resolve to 0 — the
    // "below threshold" fast path and the "hard threshold" branch agree
    // at the boundary. Guards against a seam where the soft-knee smoothing
    // accidentally bumps contribution above 0 at the threshold itself.
    glm::vec3 out = BloomPrefilter(glm::vec3{1.0F, 1.0F, 1.0F}, 1.0F, 0.0F);
    EXPECT_NEAR(out.r, 0.0F, kEps);
    EXPECT_NEAR(out.g, 0.0F, kEps);
    EXPECT_NEAR(out.b, 0.0F, kEps);
}

TEST(BloomMathTest, PrefilterInsideKneeBleedsAboveHardThreshold) {
    // Inside the knee region (luma between threshold-knee and threshold+knee),
    // the soft branch dominates and the prefilter contributes *more* than a
    // hard threshold would — this is the whole point of the knee: smoothly
    // ramp contribution from 0 rather than popping on once luma crosses the
    // threshold. At luma=1.1, threshold=1.0, knee=0.5: hard branch gives
    // factor (0.1/1.1) ≈ 0.091, soft branch gives (0.36/2) / 1.1 ≈ 0.164 —
    // strictly greater. A regression that swapped `max` for `min` in the
    // soft-vs-hard selection would fail this.
    glm::vec3 color{1.1F, 1.1F, 1.1F};
    glm::vec3 out = BloomPrefilter(color, 1.0F, 0.5F);
    float hardOut = color.r * (0.1F / 1.1F);
    EXPECT_GT(out.r, hardOut);
    // And still below the input itself — contribution never exceeds 1.
    EXPECT_LT(out.r, color.r);
}

TEST(BloomMathTest, PrefilterAtThresholdPlusKneeMatchesHardBranch) {
    // At luma = threshold + knee exactly, the soft branch saturates (softness
    // clamps to 2*knee) and equals the hard branch — the knee ramp joins
    // the linear regime smoothly with matching values. Pins the knee's
    // upper seam so a regression in the `4*knee + eps` denominator or the
    // clamp bound would show up as a discontinuity here, not downstream in
    // the shader composite.
    float threshold = 1.0F;
    float knee = 0.5F;
    float luma = threshold + knee;  // 1.5
    glm::vec3 color{luma, luma, luma};
    glm::vec3 out = BloomPrefilter(color, threshold, knee);
    // Hard branch contribution at this luma = (luma-threshold)/luma = 0.5/1.5.
    float hardFactor = (luma - threshold) / luma;
    EXPECT_NEAR(out.r, color.r * hardFactor, 1e-3F);
}

TEST(BloomMathTest, PrefilterUsesMaxChannelLuma) {
    // Prefilter luma is max-channel (brightest colour component) rather than
    // Rec.709 — this is the HDR-bloom convention because a saturated red
    // light at (2,0,0) should bloom even though its Rec.709 luma (≈0.43)
    // would reject it. A saturated pixel whose max channel is above
    // threshold must bloom; a pixel whose all channels sit below must not.
    glm::vec3 saturatedRed{2.0F, 0.0F, 0.0F};
    glm::vec3 outRed = BloomPrefilter(saturatedRed, 1.0F, 0.0F);
    EXPECT_GT(outRed.r, 0.0F);

    glm::vec3 dimAllChannels{0.8F, 0.8F, 0.8F};
    glm::vec3 outDim = BloomPrefilter(dimAllChannels, 1.0F, 0.0F);
    EXPECT_NEAR(outDim.r, 0.0F, kEps);
    EXPECT_NEAR(outDim.g, 0.0F, kEps);
    EXPECT_NEAR(outDim.b, 0.0F, kEps);
}

// ---------------------------------------------------------------------------
// BloomDownsample — dual-filter 5-tap (centre + 4 diagonals) weighting.
// ---------------------------------------------------------------------------

TEST(BloomDownsampleTest, FlatFieldReturnsSameColor) {
    // Weights 4/8 + 1/8 * 4 sum to 1 — a flat colour across all 5 samples
    // must downsample to itself. Fails if any weight drifts (e.g. 4/9,
    // missing divisor, swapped centre/diagonal weight).
    glm::vec3 c{0.3F, 0.6F, 0.9F};
    glm::vec3 out = BloomDownsample(c, c, c, c, c);
    EXPECT_NEAR(out.r, c.r, kEps);
    EXPECT_NEAR(out.g, c.g, kEps);
    EXPECT_NEAR(out.b, c.b, kEps);
}

TEST(BloomDownsampleTest, CenterContributesFourEighths) {
    // Centre-only (others black) must emit 4/8 * centre = 0.5 * centre.
    // Pins the centre weight explicitly — a kernel that gave centre weight
    // 1 (treating it as a regular tap) or weight 8 (summing everything)
    // would fail this.
    glm::vec3 c{1.0F, 1.0F, 1.0F};
    glm::vec3 zero{0.0F};
    glm::vec3 out = BloomDownsample(c, zero, zero, zero, zero);
    EXPECT_NEAR(out.r, 0.5F, kEps);
    EXPECT_NEAR(out.g, 0.5F, kEps);
    EXPECT_NEAR(out.b, 0.5F, kEps);
}

TEST(BloomDownsampleTest, EachDiagonalContributesOneEighth) {
    // Single-diagonal-only (others black) must emit 1/8 * diagonal. All
    // four corners share the same weight — pin that by running each in
    // turn. A bug that weighted one corner differently (e.g. a copy-paste
    // swap) fails here.
    glm::vec3 c{1.0F, 1.0F, 1.0F};
    glm::vec3 zero{0.0F};
    glm::vec3 outTL = BloomDownsample(zero, c, zero, zero, zero);
    glm::vec3 outTR = BloomDownsample(zero, zero, c, zero, zero);
    glm::vec3 outBL = BloomDownsample(zero, zero, zero, c, zero);
    glm::vec3 outBR = BloomDownsample(zero, zero, zero, zero, c);
    constexpr float kOneEighth = 0.125F;
    EXPECT_NEAR(outTL.r, kOneEighth, kEps);
    EXPECT_NEAR(outTR.r, kOneEighth, kEps);
    EXPECT_NEAR(outBL.r, kOneEighth, kEps);
    EXPECT_NEAR(outBR.r, kOneEighth, kEps);
}

TEST(BloomDownsampleTest, LinearInInputs) {
    // The kernel is a linear combination — downsample(a) + downsample(b)
    // must equal downsample(a+b) for the same tap positions. Catches a
    // bug that introduced a non-linearity (e.g. a sneaky max() or saturate)
    // somewhere in the weighting, which would break the correct behaviour
    // when bloom composites multiple mips.
    glm::vec3 a1{0.1F, 0.2F, 0.3F};
    glm::vec3 a2{0.4F, 0.5F, 0.6F};
    glm::vec3 a3{0.7F, 0.8F, 0.9F};
    glm::vec3 a4{0.2F, 0.3F, 0.4F};
    glm::vec3 a5{0.5F, 0.6F, 0.7F};
    glm::vec3 b1{0.3F, 0.1F, 0.2F};
    glm::vec3 b2{0.6F, 0.4F, 0.5F};
    glm::vec3 b3{0.9F, 0.7F, 0.8F};
    glm::vec3 b4{0.4F, 0.2F, 0.3F};
    glm::vec3 b5{0.7F, 0.5F, 0.6F};
    glm::vec3 sumOfDs = BloomDownsample(a1, a2, a3, a4, a5) +
                       BloomDownsample(b1, b2, b3, b4, b5);
    glm::vec3 dOfSum = BloomDownsample(a1 + b1, a2 + b2, a3 + b3, a4 + b4, a5 + b5);
    EXPECT_NEAR(sumOfDs.r, dOfSum.r, kEps);
    EXPECT_NEAR(sumOfDs.g, dOfSum.g, kEps);
    EXPECT_NEAR(sumOfDs.b, dOfSum.b, kEps);
}

// ---------------------------------------------------------------------------
// BloomUpsample — dual-filter 8-tap tent (4 cardinals + 4 diagonals).
// ---------------------------------------------------------------------------

TEST(BloomUpsampleTest, FlatFieldReturnsSameColor) {
    // Cardinal weight 1 * 4 + diagonal weight 2 * 4 = 12, divided by 12 → 1.
    // A flat colour across all 8 taps must upsample to itself. The tent has
    // no centre tap (unlike downsample) — this is the classic Bjørge form:
    // a centre term would double-count since the upsample writes back into
    // a mip that will later be added with the higher mip during composite.
    glm::vec3 c{0.4F, 0.7F, 0.2F};
    glm::vec3 out = BloomUpsample(c, c, c, c, c, c, c, c);
    EXPECT_NEAR(out.r, c.r, kEps);
    EXPECT_NEAR(out.g, c.g, kEps);
    EXPECT_NEAR(out.b, c.b, kEps);
}

TEST(BloomUpsampleTest, EachCardinalContributesOneTwelfth) {
    // Cardinal-only taps (top/bottom/left/right) have weight 1/12 each.
    // Running each through in turn pins all four weights — a swap of one
    // weight (e.g. accidentally giving `top` weight 2) would fail exactly
    // one of these four without affecting the others.
    glm::vec3 c{1.0F, 1.0F, 1.0F};
    glm::vec3 zero{0.0F};
    constexpr float kOneTwelfth = 1.0F / 12.0F;
    glm::vec3 outT = BloomUpsample(c, zero, zero, zero, zero, zero, zero, zero);
    glm::vec3 outB = BloomUpsample(zero, c, zero, zero, zero, zero, zero, zero);
    glm::vec3 outL = BloomUpsample(zero, zero, c, zero, zero, zero, zero, zero);
    glm::vec3 outR = BloomUpsample(zero, zero, zero, c, zero, zero, zero, zero);
    EXPECT_NEAR(outT.r, kOneTwelfth, kEps);
    EXPECT_NEAR(outB.r, kOneTwelfth, kEps);
    EXPECT_NEAR(outL.r, kOneTwelfth, kEps);
    EXPECT_NEAR(outR.r, kOneTwelfth, kEps);
}

TEST(BloomUpsampleTest, EachDiagonalContributesTwoTwelfths) {
    // Diagonal taps weigh twice the cardinals — 2/12 each. The GDC 2015
    // talk derives this from the target being a 3×3 tent centred on the
    // target pixel, and diagonals lie on the denser corner sums. A kernel
    // that flattened all 8 weights to 1/8 would fail here but still pass
    // FlatFieldReturnsSameColor (sum-to-1), so this test is load-bearing.
    glm::vec3 c{1.0F, 1.0F, 1.0F};
    glm::vec3 zero{0.0F};
    constexpr float kTwoTwelfths = 2.0F / 12.0F;
    glm::vec3 outTL = BloomUpsample(zero, zero, zero, zero, c, zero, zero, zero);
    glm::vec3 outTR = BloomUpsample(zero, zero, zero, zero, zero, c, zero, zero);
    glm::vec3 outBL = BloomUpsample(zero, zero, zero, zero, zero, zero, c, zero);
    glm::vec3 outBR = BloomUpsample(zero, zero, zero, zero, zero, zero, zero, c);
    EXPECT_NEAR(outTL.r, kTwoTwelfths, kEps);
    EXPECT_NEAR(outTR.r, kTwoTwelfths, kEps);
    EXPECT_NEAR(outBL.r, kTwoTwelfths, kEps);
    EXPECT_NEAR(outBR.r, kTwoTwelfths, kEps);
}

TEST(BloomUpsampleTest, LinearInInputs) {
    // Like downsample, the upsample kernel must be linear — bloom composite
    // adds progressively-upsampled mips, so any hidden non-linearity (a
    // max(), a saturate(), a per-channel tonemap) corrupts the accumulated
    // result. Catches such a bug at the helper level before it leaks into
    // the shader port.
    glm::vec3 a1{0.1F, 0.2F, 0.3F};
    glm::vec3 a2{0.4F, 0.5F, 0.6F};
    glm::vec3 a3{0.7F, 0.8F, 0.9F};
    glm::vec3 a4{0.2F, 0.3F, 0.4F};
    glm::vec3 a5{0.5F, 0.6F, 0.7F};
    glm::vec3 a6{0.3F, 0.4F, 0.5F};
    glm::vec3 a7{0.6F, 0.7F, 0.8F};
    glm::vec3 a8{0.8F, 0.1F, 0.2F};
    glm::vec3 b1{0.2F, 0.1F, 0.0F};
    glm::vec3 b2{0.5F, 0.4F, 0.3F};
    glm::vec3 b3{0.8F, 0.7F, 0.6F};
    glm::vec3 b4{0.3F, 0.2F, 0.1F};
    glm::vec3 b5{0.6F, 0.5F, 0.4F};
    glm::vec3 b6{0.4F, 0.3F, 0.2F};
    glm::vec3 b7{0.7F, 0.6F, 0.5F};
    glm::vec3 b8{0.9F, 0.8F, 0.7F};
    glm::vec3 sumOfUs = BloomUpsample(a1, a2, a3, a4, a5, a6, a7, a8) +
                       BloomUpsample(b1, b2, b3, b4, b5, b6, b7, b8);
    glm::vec3 uOfSum = BloomUpsample(a1 + b1, a2 + b2, a3 + b3, a4 + b4,
                                     a5 + b5, a6 + b6, a7 + b7, a8 + b8);
    EXPECT_NEAR(sumOfUs.r, uOfSum.r, kEps);
    EXPECT_NEAR(sumOfUs.g, uOfSum.g, kEps);
    EXPECT_NEAR(sumOfUs.b, uOfSum.b, kEps);
}

}  // namespace
