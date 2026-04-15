#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "renderer/ClipPlane.h"
#include "scene/Slicing.h"

using bimeup::renderer::ClipPlane;
using bimeup::scene::SliceTriangle;
using bimeup::scene::TriangleCut;

namespace {

// Plane y=0 (normal +Y, d=0): front = y>0, back = y<0.
ClipPlane YZero() {
    return ClipPlane::FromPointNormal({0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
}

bool NearEq(const glm::vec3& a, const glm::vec3& b, float eps = 1e-4F) {
    return glm::length(a - b) <= eps;
}

// Triangle has an unordered pair of intersection points equal to {p, q}.
bool HasPoints(const TriangleCut& cut, const glm::vec3& p, const glm::vec3& q) {
    if (cut.pointCount != 2) return false;
    return (NearEq(cut.points[0], p) && NearEq(cut.points[1], q)) ||
           (NearEq(cut.points[0], q) && NearEq(cut.points[1], p));
}

}  // namespace

TEST(SliceTriangle, AllFrontProducesNoCut) {
    const auto cut = SliceTriangle(YZero(), {0, 1, 0}, {1, 1, 0}, {0, 1, 1});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, AllBackProducesNoCut) {
    const auto cut = SliceTriangle(YZero(), {0, -1, 0}, {1, -1, 0}, {0, -1, 1});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, CoplanarProducesNoCut) {
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 0, 0}, {0, 0, 1});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, TwoFrontOneBackCutsBothStraddlingEdges) {
    // A=(0,1,0) front, B=(2,1,0) front, C=(1,-1,0) back.
    // Edge BC crosses at midpoint (1.5, 0, 0). Edge CA crosses at (0.5, 0, 0).
    const auto cut = SliceTriangle(YZero(), {0, 1, 0}, {2, 1, 0}, {1, -1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.5F, 0.0F, 0.0F}, {1.5F, 0.0F, 0.0F}));
}

TEST(SliceTriangle, TwoBackOneFrontCutsBothStraddlingEdges) {
    // A=(0,-1,0) back, B=(2,-1,0) back, C=(1,1,0) front.
    const auto cut = SliceTriangle(YZero(), {0, -1, 0}, {2, -1, 0}, {1, 1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.5F, 0.0F, 0.0F}, {1.5F, 0.0F, 0.0F}));
}

TEST(SliceTriangle, VertexOnPlaneStraddleUsesVertexAndOppositeEdge) {
    // A=(0,0,0) on plane, B=(1,1,0) front, C=(1,-1,0) back.
    // Segment goes from A to mid of BC = (1, 0, 0).
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 1, 0}, {1, -1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}));
}

TEST(SliceTriangle, VertexOnPlaneButNoStraddleDrops) {
    // A=(0,0,0) on plane, rest on the same (front) side → single-point touch.
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 1, 0}, {2, 1, 0});
    EXPECT_EQ(cut.pointCount, 0);
}

TEST(SliceTriangle, EdgeOnPlaneReturnsBothOnPlaneVertices) {
    // A=(0,0,0) and B=(1,0,0) on plane; C=(0,1,0) front. The edge AB lies on
    // the plane — emit it as the section segment.
    const auto cut = SliceTriangle(YZero(), {0, 0, 0}, {1, 0, 0}, {0, 1, 0});
    EXPECT_EQ(cut.pointCount, 2);
    EXPECT_TRUE(HasPoints(cut, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}));
}
