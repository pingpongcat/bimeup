#include <gtest/gtest.h>

#include <renderer/SsaoKernel.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace {

using bimeup::renderer::GenerateHemisphereKernel;
using bimeup::renderer::PackEdges;
using bimeup::renderer::UnpackEdges;

constexpr float kEps = 1e-5F;

TEST(SsaoKernelTest, HemisphereKernelReturnsRequestedSize) {
    auto k16 = GenerateHemisphereKernel(16, 1);
    auto k64 = GenerateHemisphereKernel(64, 1);
    EXPECT_EQ(k16.size(), 16U);
    EXPECT_EQ(k64.size(), 64U);
}

TEST(SsaoKernelTest, HemisphereKernelAllSamplesInPlusZHemisphere) {
    // SSAO kernel is a +z hemisphere; the shader rotates each sample into the
    // fragment's TBN basis. A sample with z < 0 would point behind the surface
    // and produce bogus AO.
    auto samples = GenerateHemisphereKernel(128, 7);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        EXPECT_GE(samples[i].z, 0.0F) << "sample " << i;
    }
}

TEST(SsaoKernelTest, HemisphereKernelSamplesInsideUnitSphere) {
    // Quadratic falloff rescales samples by a factor in [0.1, 1.0], and the
    // raw direction is inside the unit sphere → every sample's length must be
    // ≤ 1 (tiny epsilon for float rounding).
    auto samples = GenerateHemisphereKernel(128, 42);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        EXPECT_LE(glm::length(samples[i]), 1.0F + kEps) << "sample " << i;
    }
}

TEST(SsaoKernelTest, HemisphereKernelDeterministicForSameSeed) {
    auto a = GenerateHemisphereKernel(32, 1234);
    auto b = GenerateHemisphereKernel(32, 1234);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].x, b[i].x) << "sample " << i;
        EXPECT_EQ(a[i].y, b[i].y);
        EXPECT_EQ(a[i].z, b[i].z);
    }
}

TEST(SsaoKernelTest, HemisphereKernelDiffersWithDifferentSeed) {
    auto a = GenerateHemisphereKernel(32, 1);
    auto b = GenerateHemisphereKernel(32, 2);
    ASSERT_EQ(a.size(), b.size());
    // At least one sample must differ between seeds (catches a stub that
    // ignores the seed).
    bool anyDiff = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            anyDiff = true;
            break;
        }
    }
    EXPECT_TRUE(anyDiff);
}

TEST(SsaoKernelTest, HemisphereKernelBiasedTowardOrigin) {
    // Quadratic weighting lerp(0.1, 1.0, i^2/N^2) pushes later samples outward
    // and clusters earlier samples near the origin. Average length of the
    // first quarter must be strictly less than the last quarter.
    constexpr std::size_t kCount = 64;
    auto samples = GenerateHemisphereKernel(kCount, 99);
    float firstAvg = 0.0F;
    float lastAvg = 0.0F;
    constexpr std::size_t kQuarter = kCount / 4;
    for (std::size_t i = 0; i < kQuarter; ++i) {
        firstAvg += glm::length(samples[i]);
        lastAvg += glm::length(samples[kCount - 1 - i]);
    }
    firstAvg /= static_cast<float>(kQuarter);
    lastAvg /= static_cast<float>(kQuarter);
    EXPECT_LT(firstAvg, lastAvg) << "first=" << firstAvg << " last=" << lastAvg;
}

TEST(SsaoKernelTest, PackEdgesAllZeroIsZero) {
    EXPECT_EQ(PackEdges(0.0F, 0.0F, 0.0F, 0.0F), 0U);
}

TEST(SsaoKernelTest, PackEdgesAllOneIsFullByte) {
    EXPECT_EQ(PackEdges(1.0F, 1.0F, 1.0F, 1.0F), 0xFFU);
}

TEST(SsaoKernelTest, PackEdgesIsolatedChannelOccupiesExpectedBits) {
    // Channel layout: L → bits 0-1, R → bits 2-3, T → bits 4-5, B → bits 6-7.
    // Driving one channel to saturation while the rest are 0 pins down the
    // bit-pack contract so the GLSL mirror (RP.5b) can't drift.
    EXPECT_EQ(PackEdges(1.0F, 0.0F, 0.0F, 0.0F), 0x03U);
    EXPECT_EQ(PackEdges(0.0F, 1.0F, 0.0F, 0.0F), 0x0CU);
    EXPECT_EQ(PackEdges(0.0F, 0.0F, 1.0F, 0.0F), 0x30U);
    EXPECT_EQ(PackEdges(0.0F, 0.0F, 0.0F, 1.0F), 0xC0U);
}

TEST(SsaoKernelTest, PackEdgesSaturatesOutOfRangeInputs) {
    // Negative inputs clamp to 0, inputs > 1 clamp to saturation. The AO
    // shader feeds raw edge deltas so the helper has to be robust.
    EXPECT_EQ(PackEdges(-0.5F, 0.0F, 0.0F, 0.0F), 0x00U);
    EXPECT_EQ(PackEdges(5.0F, 0.0F, 0.0F, 0.0F), 0x03U);
    EXPECT_EQ(PackEdges(-1.0F, -1.0F, 2.0F, 2.0F), 0xF0U);
}

TEST(SsaoKernelTest, PackUnpackRoundTripsAtLatticePoints) {
    // 2-bit quantisation → 4 levels {0, 1/3, 2/3, 1}. Values placed exactly on
    // the lattice must round-trip exactly.
    std::array<float, 4> lattice = {0.0F, 1.0F / 3.0F, 2.0F / 3.0F, 1.0F};
    for (float l : lattice) {
        for (float r : lattice) {
            for (float t : lattice) {
                for (float b : lattice) {
                    std::uint8_t p = PackEdges(l, r, t, b);
                    glm::vec4 u = UnpackEdges(p);
                    EXPECT_NEAR(u.x, l, kEps);
                    EXPECT_NEAR(u.y, r, kEps);
                    EXPECT_NEAR(u.z, t, kEps);
                    EXPECT_NEAR(u.w, b, kEps);
                }
            }
        }
    }
}

TEST(SsaoKernelTest, UnpackEdgesReturnsValuesInUnitRange) {
    // Every byte value → four lattice-quantised weights in [0, 1].
    for (int i = 0; i < 256; ++i) {
        glm::vec4 u = UnpackEdges(static_cast<std::uint8_t>(i));
        EXPECT_GE(u.x, 0.0F);
        EXPECT_LE(u.x, 1.0F + kEps);
        EXPECT_GE(u.y, 0.0F);
        EXPECT_LE(u.y, 1.0F + kEps);
        EXPECT_GE(u.z, 0.0F);
        EXPECT_LE(u.z, 1.0F + kEps);
        EXPECT_GE(u.w, 0.0F);
        EXPECT_LE(u.w, 1.0F + kEps);
    }
}

}  // namespace
