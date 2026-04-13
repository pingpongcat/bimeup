#include <gtest/gtest.h>
#include <renderer/Msaa.h>

using bimeup::renderer::ClampSampleCount;

TEST(MsaaClampTest, ClampsDownToHighestSupportedBit) {
    // Device supports only 1x / 2x / 4x.
    VkSampleCountFlags supported =
        VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;

    EXPECT_EQ(ClampSampleCount(8, supported), VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(ClampSampleCount(4, supported), VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(ClampSampleCount(2, supported), VK_SAMPLE_COUNT_2_BIT);
    EXPECT_EQ(ClampSampleCount(1, supported), VK_SAMPLE_COUNT_1_BIT);
}

TEST(MsaaClampTest, RoundsNonPowerOfTwoDown) {
    VkSampleCountFlags supported =
        VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_2_BIT | VK_SAMPLE_COUNT_4_BIT;

    EXPECT_EQ(ClampSampleCount(3, supported), VK_SAMPLE_COUNT_2_BIT);
    EXPECT_EQ(ClampSampleCount(5, supported), VK_SAMPLE_COUNT_4_BIT);
    EXPECT_EQ(ClampSampleCount(0, supported), VK_SAMPLE_COUNT_1_BIT);
}

TEST(MsaaClampTest, SkipsUnsupportedCountsInMask) {
    // Device supports 1x and 4x (but not 2x — exotic but legal mask).
    VkSampleCountFlags supported = VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT;

    EXPECT_EQ(ClampSampleCount(2, supported), VK_SAMPLE_COUNT_1_BIT);
    EXPECT_EQ(ClampSampleCount(8, supported), VK_SAMPLE_COUNT_4_BIT);
}

TEST(MsaaClampTest, AlwaysReturnsAtLeast1Bit) {
    // Even a degenerate mask should yield a valid VkSampleCountFlagBits (the 1x fallback).
    EXPECT_EQ(ClampSampleCount(4, 0), VK_SAMPLE_COUNT_1_BIT);
}
