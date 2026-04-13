#include <gtest/gtest.h>
#include <renderer/ClipPlane.h>

#include <glm/gtc/matrix_access.hpp>

using bimeup::renderer::ClipPlane;
using bimeup::renderer::PointSide;
using bimeup::renderer::ClassifyPoint;
using bimeup::renderer::SignedDistance;
using bimeup::renderer::PlaneToTransform;
using bimeup::renderer::TransformToPlane;

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

TEST(ClipPlaneTest, PlaneToTransform_TranslationIsClosestPointToOrigin) {
    // Plane y = 4 → closest point to origin is (0, 4, 0).
    ClipPlane p{{0.0f, 1.0f, 0.0f, -4.0f}, true};
    glm::mat4 m = PlaneToTransform(p);
    glm::vec4 t = glm::column(m, 3);
    EXPECT_NEAR(t.x, 0.0f, 1e-5f);
    EXPECT_NEAR(t.y, 4.0f, 1e-5f);
    EXPECT_NEAR(t.z, 0.0f, 1e-5f);
    EXPECT_FLOAT_EQ(t.w, 1.0f);
}

TEST(ClipPlaneTest, PlaneToTransform_ZAxisAlignsWithNormal) {
    // Normal (1,0,0) → transform's Z column should point +X.
    ClipPlane p{{1.0f, 0.0f, 0.0f, 0.0f}, true};
    glm::mat4 m = PlaneToTransform(p);
    glm::vec3 z = glm::vec3(glm::column(m, 2));
    EXPECT_NEAR(z.x, 1.0f, 1e-5f);
    EXPECT_NEAR(z.y, 0.0f, 1e-5f);
    EXPECT_NEAR(z.z, 0.0f, 1e-5f);
}

TEST(ClipPlaneTest, TransformToPlane_RoundTrip) {
    ClipPlane original{{0.0f, 0.0f, 1.0f, -2.5f}, true};
    glm::mat4 m = PlaneToTransform(original);
    ClipPlane back = TransformToPlane(m);
    EXPECT_NEAR(back.equation.x, original.equation.x, 1e-5f);
    EXPECT_NEAR(back.equation.y, original.equation.y, 1e-5f);
    EXPECT_NEAR(back.equation.z, original.equation.z, 1e-5f);
    EXPECT_NEAR(back.equation.w, original.equation.w, 1e-5f);
}

TEST(ClipPlaneTest, TransformToPlane_RoundTripDiagonal) {
    const float s = 1.0f / 1.41421356f;
    ClipPlane original{{s, s, 0.0f, -3.0f}, true};
    glm::mat4 m = PlaneToTransform(original);
    ClipPlane back = TransformToPlane(m);
    EXPECT_NEAR(back.equation.x, original.equation.x, 1e-5f);
    EXPECT_NEAR(back.equation.y, original.equation.y, 1e-5f);
    EXPECT_NEAR(back.equation.z, original.equation.z, 1e-5f);
    EXPECT_NEAR(back.equation.w, original.equation.w, 1e-5f);
}

TEST(ClipPlaneTest, TransformToPlane_TranslationOffNormalPreservesNormalAndDistance) {
    // Build a plane, then slide the transform's translation along a tangent of the plane.
    // The resulting plane must have the same normal and same signed-distance-to-origin.
    ClipPlane original{{0.0f, 1.0f, 0.0f, -4.0f}, true};
    glm::mat4 m = PlaneToTransform(original);
    // Slide along +X (tangent to the y=4 plane).
    m[3] = glm::vec4(5.0f, 4.0f, 0.0f, 1.0f);
    ClipPlane back = TransformToPlane(m);
    EXPECT_NEAR(back.equation.x, 0.0f, 1e-5f);
    EXPECT_NEAR(back.equation.y, 1.0f, 1e-5f);
    EXPECT_NEAR(back.equation.z, 0.0f, 1e-5f);
    EXPECT_NEAR(back.equation.w, -4.0f, 1e-5f);
}
