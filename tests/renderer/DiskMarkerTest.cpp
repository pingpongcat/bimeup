#include <gtest/gtest.h>

#include <renderer/DiskMarker.h>

#include <cmath>

#include <glm/glm.hpp>

using bimeup::renderer::BuildDiskVertices;
using bimeup::renderer::DiskVertex;

namespace {

constexpr float kCoplanarEpsilon = 1e-4F;

}  // namespace

TEST(DiskMarkerTest, VertexCountIsThreeTimesSegments) {
    const auto verts =
        BuildDiskVertices(glm::vec3(0.0F), glm::vec3(0.0F, 1.0F, 0.0F), 1.0F,
                          glm::vec4(1.0F), 8);
    EXPECT_EQ(verts.size(), 8U * 3U);
}

TEST(DiskMarkerTest, AllVerticesAreCoplanarWithPlane) {
    const glm::vec3 center(2.0F, 3.0F, 5.0F);
    const glm::vec3 normal = glm::normalize(glm::vec3(1.0F, 2.0F, 3.0F));
    const auto verts = BuildDiskVertices(center, normal, 0.5F,
                                         glm::vec4(1.0F), 16);
    for (const auto& v : verts) {
        const float d = glm::dot(v.position - center, normal);
        EXPECT_NEAR(d, 0.0F, kCoplanarEpsilon);
    }
}

TEST(DiskMarkerTest, TriangleFanTopology) {
    // With a triangle fan, every triangle shares the center vertex at index 0
    // within the triplet. Convention: triplet = {center, ring[i], ring[i+1]}.
    const glm::vec3 center(0.0F);
    const auto verts = BuildDiskVertices(center, glm::vec3(0.0F, 1.0F, 0.0F),
                                         2.0F, glm::vec4(1.0F), 6);
    ASSERT_EQ(verts.size(), 18U);
    for (std::size_t t = 0; t < 6; ++t) {
        const auto& c = verts[3 * t + 0];
        const auto& a = verts[3 * t + 1];
        const auto& b = verts[3 * t + 2];
        EXPECT_NEAR(glm::distance(c.position, center), 0.0F, 1e-5F);
        EXPECT_NEAR(glm::distance(a.position, center), 2.0F, 1e-4F);
        EXPECT_NEAR(glm::distance(b.position, center), 2.0F, 1e-4F);
    }
}

TEST(DiskMarkerTest, AlphaFadesCenterOpaqueOuterTransparent) {
    const auto verts = BuildDiskVertices(glm::vec3(0.0F),
                                         glm::vec3(0.0F, 1.0F, 0.0F), 1.0F,
                                         glm::vec4(0.2F, 0.8F, 1.0F, 1.0F), 8);
    for (std::size_t t = 0; t < 8; ++t) {
        const auto& c = verts[3 * t + 0];
        const auto& a = verts[3 * t + 1];
        const auto& b = verts[3 * t + 2];
        EXPECT_FLOAT_EQ(c.color.a, 1.0F);
        EXPECT_FLOAT_EQ(a.color.a, 0.0F);
        EXPECT_FLOAT_EQ(b.color.a, 0.0F);
        // RGB preserved from input.
        EXPECT_FLOAT_EQ(c.color.r, 0.2F);
        EXPECT_FLOAT_EQ(a.color.g, 0.8F);
        EXPECT_FLOAT_EQ(b.color.b, 1.0F);
    }
}

TEST(DiskMarkerTest, SegmentsBelowThreeClampsToThree) {
    const auto verts = BuildDiskVertices(glm::vec3(0.0F),
                                         glm::vec3(0.0F, 1.0F, 0.0F), 1.0F,
                                         glm::vec4(1.0F), 2);
    EXPECT_EQ(verts.size(), 3U * 3U);
}

TEST(DiskMarkerTest, ZeroOrNegativeRadiusReturnsEmpty) {
    EXPECT_TRUE(
        BuildDiskVertices(glm::vec3(0.0F), glm::vec3(0.0F, 1.0F, 0.0F), 0.0F,
                          glm::vec4(1.0F), 8)
            .empty());
    EXPECT_TRUE(
        BuildDiskVertices(glm::vec3(0.0F), glm::vec3(0.0F, 1.0F, 0.0F), -1.0F,
                          glm::vec4(1.0F), 8)
            .empty());
}

TEST(DiskMarkerTest, DegenerateNormalReturnsEmpty) {
    EXPECT_TRUE(
        BuildDiskVertices(glm::vec3(0.0F), glm::vec3(0.0F), 1.0F,
                          glm::vec4(1.0F), 8)
            .empty());
}

TEST(DiskMarkerTest, RingSweepsFullCircle) {
    // Outer ring positions should trace a full 2π: summing them gives ~center.
    const glm::vec3 center(1.0F, 2.0F, 3.0F);
    const auto verts = BuildDiskVertices(center, glm::vec3(0.0F, 1.0F, 0.0F),
                                         1.0F, glm::vec4(1.0F), 32);
    glm::vec3 sum(0.0F);
    for (std::size_t t = 0; t < 32; ++t) {
        sum += verts[3 * t + 1].position;
    }
    sum /= 32.0F;
    EXPECT_NEAR(glm::distance(sum, center), 0.0F, 1e-3F);
}
