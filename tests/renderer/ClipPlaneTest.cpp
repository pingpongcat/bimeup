#include <gtest/gtest.h>
#include <renderer/ClipPlane.h>

using bimeup::renderer::ClipPlane;
using bimeup::renderer::PointSide;
using bimeup::renderer::ClassifyPoint;
using bimeup::renderer::SignedDistance;

TEST(ClipPlaneTest, SignedDistance_UnitXPlaneAtOrigin) {
    // Plane: x = 0, normal = (1,0,0), d = 0 → equation (1,0,0,0).
    ClipPlane p{{1.0f, 0.0f, 0.0f, 0.0f}, true};
    EXPECT_FLOAT_EQ(SignedDistance(p, {2.0f, 5.0f, 7.0f}), 2.0f);
    EXPECT_FLOAT_EQ(SignedDistance(p, {-3.0f, 0.0f, 0.0f}), -3.0f);
    EXPECT_FLOAT_EQ(SignedDistance(p, {0.0f, 1.0f, 2.0f}), 0.0f);
}

TEST(ClipPlaneTest, SignedDistance_OffsetPlane) {
    // Plane: y = 4, normal = (0,1,0), so 0*x + 1*y + 0*z + (-4) = 0 → equation (0,1,0,-4).
    ClipPlane p{{0.0f, 1.0f, 0.0f, -4.0f}, true};
    EXPECT_FLOAT_EQ(SignedDistance(p, {0.0f, 10.0f, 0.0f}), 6.0f);
    EXPECT_FLOAT_EQ(SignedDistance(p, {9.0f, 1.0f, 9.0f}), -3.0f);
    EXPECT_FLOAT_EQ(SignedDistance(p, {0.0f, 4.0f, 0.0f}), 0.0f);
}

TEST(ClipPlaneTest, ClassifyPoint_FrontBackOnPlane) {
    ClipPlane p{{1.0f, 0.0f, 0.0f, 0.0f}, true};
    EXPECT_EQ(ClassifyPoint(p, {1.0f, 0.0f, 0.0f}), PointSide::Front);
    EXPECT_EQ(ClassifyPoint(p, {-1.0f, 0.0f, 0.0f}), PointSide::Back);
    EXPECT_EQ(ClassifyPoint(p, {0.0f, 0.0f, 0.0f}), PointSide::OnPlane);
}

TEST(ClipPlaneTest, ClassifyPoint_WithinEpsilonIsOnPlane) {
    ClipPlane p{{1.0f, 0.0f, 0.0f, 0.0f}, true};
    // Default epsilon (1e-4) treats tiny offsets as on-plane.
    EXPECT_EQ(ClassifyPoint(p, {1e-6f, 0.0f, 0.0f}), PointSide::OnPlane);
    EXPECT_EQ(ClassifyPoint(p, {-1e-6f, 0.0f, 0.0f}), PointSide::OnPlane);
    // Outside epsilon resolves to a side.
    EXPECT_EQ(ClassifyPoint(p, {1e-2f, 0.0f, 0.0f}), PointSide::Front);
    EXPECT_EQ(ClassifyPoint(p, {-1e-2f, 0.0f, 0.0f}), PointSide::Back);
}

TEST(ClipPlaneTest, ClassifyPoint_DiagonalPlane) {
    // Normal (1,1,0)/sqrt(2), passes through origin.
    const float s = 1.0f / 1.41421356f;
    ClipPlane p{{s, s, 0.0f, 0.0f}, true};
    EXPECT_EQ(ClassifyPoint(p, {1.0f, 1.0f, 0.0f}), PointSide::Front);
    EXPECT_EQ(ClassifyPoint(p, {-1.0f, -1.0f, 0.0f}), PointSide::Back);
    // A point on the plane: (1,-1,0) → dot = 0.
    EXPECT_EQ(ClassifyPoint(p, {1.0f, -1.0f, 5.0f}), PointSide::OnPlane);
}

TEST(ClipPlaneTest, PlaneFromPointAndNormal) {
    // Constructor helper: build equation from a point on the plane and a normal.
    const glm::vec3 point{0.0f, 4.0f, 0.0f};
    const glm::vec3 normal{0.0f, 1.0f, 0.0f};
    ClipPlane p = ClipPlane::FromPointNormal(point, normal);
    EXPECT_FLOAT_EQ(p.equation.x, 0.0f);
    EXPECT_FLOAT_EQ(p.equation.y, 1.0f);
    EXPECT_FLOAT_EQ(p.equation.z, 0.0f);
    EXPECT_FLOAT_EQ(p.equation.w, -4.0f);
    EXPECT_EQ(ClassifyPoint(p, {0.0f, 4.0f, 0.0f}), PointSide::OnPlane);
    EXPECT_EQ(ClassifyPoint(p, {0.0f, 5.0f, 0.0f}), PointSide::Front);
}

TEST(ClipPlaneTest, FromPointNormal_NormalizesNonUnitNormal) {
    // Non-unit normal should be normalized so SignedDistance returns real distance.
    ClipPlane p = ClipPlane::FromPointNormal({0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(SignedDistance(p, {3.0f, 0.0f, 0.0f}), 3.0f);
}
