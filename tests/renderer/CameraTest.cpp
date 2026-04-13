#include <gtest/gtest.h>
#include <renderer/Camera.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <numbers>

using bimeup::renderer::Camera;

constexpr float kEpsilon = 1e-4f;

static bool Vec3Near(glm::vec3 a, glm::vec3 b, float eps = kEpsilon) {
    return glm::length(a - b) < eps;
}

static bool Mat4Near(const glm::mat4& a, const glm::mat4& b, float eps = kEpsilon) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (std::abs(a[c][r] - b[c][r]) > eps) return false;
    return true;
}

class CameraTest : public ::testing::Test {
protected:
    Camera m_camera;
};

TEST_F(CameraTest, DefaultProjectionIsIdentity) {
    // Before SetPerspective, projection should be identity
    EXPECT_TRUE(Mat4Near(m_camera.GetProjectionMatrix(), glm::mat4(1.0f)));
}

TEST_F(CameraTest, SetPerspectiveProducesCorrectMatrix) {
    m_camera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    auto proj = m_camera.GetProjectionMatrix();
    auto expected = glm::perspective(
        glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    // Vulkan clip space: flip Y
    expected[1][1] *= -1.0f;

    EXPECT_TRUE(Mat4Near(proj, expected));
}

TEST_F(CameraTest, DefaultViewMatrixLooksAtOrigin) {
    // Default camera should be at some position looking at the origin
    auto view = m_camera.GetViewMatrix();
    // View matrix should not be identity (camera has a position offset)
    EXPECT_FALSE(Mat4Near(view, glm::mat4(1.0f)));
}

TEST_F(CameraTest, DefaultPositionIsNotAtOrigin) {
    auto pos = m_camera.GetPosition();
    EXPECT_GT(glm::length(pos), 0.1f);
}

TEST_F(CameraTest, OrbitChangesViewMatrix) {
    auto viewBefore = m_camera.GetViewMatrix();
    m_camera.Orbit(0.5f, 0.0f);
    auto viewAfter = m_camera.GetViewMatrix();

    EXPECT_FALSE(Mat4Near(viewBefore, viewAfter));
}

TEST_F(CameraTest, OrbitYawRotatesAroundTarget) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    auto posBefore = m_camera.GetPosition();

    m_camera.Orbit(std::numbers::pi_v<float> / 2.0f, 0.0f);  // 90 degrees yaw
    auto posAfter = m_camera.GetPosition();

    // Distance to target should stay the same
    float distBefore = glm::length(posBefore);
    float distAfter = glm::length(posAfter);
    EXPECT_NEAR(distBefore, distAfter, kEpsilon);

    // But position should have changed
    EXPECT_FALSE(Vec3Near(posBefore, posAfter));
}

TEST_F(CameraTest, OrbitPitchChangesElevation) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    auto posBefore = m_camera.GetPosition();

    m_camera.Orbit(0.0f, 0.3f);
    auto posAfter = m_camera.GetPosition();

    // Y component should change
    EXPECT_NE(posBefore.y, posAfter.y);
}

TEST_F(CameraTest, OrbitPitchClampsPreventsFlip) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));

    // Orbit pitch by a huge amount — should clamp and not flip
    m_camera.Orbit(0.0f, 100.0f);
    auto pos = m_camera.GetPosition();

    // Position should be above the target (positive Y relative to target)
    EXPECT_GT(pos.y, 0.0f);

    // The view matrix should still be valid (no NaN)
    auto view = m_camera.GetViewMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_FALSE(std::isnan(view[c][r]));
}

TEST_F(CameraTest, ZoomChangesDistance) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    float distBefore = glm::length(m_camera.GetPosition());

    m_camera.Zoom(-1.0f);  // zoom in
    float distAfter = glm::length(m_camera.GetPosition());

    EXPECT_LT(distAfter, distBefore);
}

TEST_F(CameraTest, ZoomClampsMinDistance) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));

    // Zoom in by a huge amount
    m_camera.Zoom(-1000.0f);
    float dist = glm::length(m_camera.GetPosition());

    EXPECT_GT(dist, 0.0f);
}

TEST_F(CameraTest, PanMovesTargetAndCamera) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    auto posBefore = m_camera.GetPosition();

    m_camera.Pan(glm::vec2(1.0f, 0.0f));
    auto posAfter = m_camera.GetPosition();

    EXPECT_FALSE(Vec3Near(posBefore, posAfter));
}

TEST_F(CameraTest, GetForwardPointsTowardTarget) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    auto pos = m_camera.GetPosition();
    auto forward = m_camera.GetForward();

    // Forward should roughly point from camera to target
    auto toTarget = glm::normalize(glm::vec3(0.0f) - pos);
    float dotProduct = glm::dot(forward, toTarget);

    EXPECT_GT(dotProduct, 0.9f);  // Should be close to 1
}

TEST_F(CameraTest, SetOrbitTargetMovesView) {
    auto viewBefore = m_camera.GetViewMatrix();
    m_camera.SetOrbitTarget(glm::vec3(10.0f, 5.0f, 3.0f));
    auto viewAfter = m_camera.GetViewMatrix();

    EXPECT_FALSE(Mat4Near(viewBefore, viewAfter));
}

TEST_F(CameraTest, ViewMatrixIsConsistentWithLookAt) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    auto pos = m_camera.GetPosition();
    auto view = m_camera.GetViewMatrix();

    auto expected = glm::lookAt(pos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    EXPECT_TRUE(Mat4Near(view, expected));
}

TEST_F(CameraTest, SetDistanceControlsOrbitRadius) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(12.5f);

    float dist = glm::length(m_camera.GetPosition());
    EXPECT_NEAR(dist, 12.5f, kEpsilon);
}

TEST_F(CameraTest, SetDistanceClampsToMin) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(-50.0f);

    float dist = glm::length(m_camera.GetPosition());
    EXPECT_GT(dist, 0.0f);
}

TEST_F(CameraTest, DefaultProjectionModeIsPerspective) {
    EXPECT_EQ(m_camera.GetProjectionMode(), bimeup::renderer::ProjectionMode::Perspective);
    EXPECT_FALSE(m_camera.IsOrthographic());
}

TEST_F(CameraTest, SetOrthographicProducesCorrectMatrix) {
    m_camera.SetOrthographic(10.0f, 16.0f / 9.0f, 0.1f, 100.0f);

    float halfH = 5.0f;
    float halfW = halfH * (16.0f / 9.0f);
    auto expected = glm::orthoRH_ZO(-halfW, halfW, -halfH, halfH, 0.1f, 100.0f);
    expected[1][1] *= -1.0f;  // Vulkan Y flip

    EXPECT_TRUE(Mat4Near(m_camera.GetProjectionMatrix(), expected));
}

TEST_F(CameraTest, SetOrthographicSwitchesMode) {
    m_camera.SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    m_camera.SetOrthographic(10.0f, 1.0f, 0.1f, 100.0f);

    EXPECT_EQ(m_camera.GetProjectionMode(), bimeup::renderer::ProjectionMode::Orthographic);
    EXPECT_TRUE(m_camera.IsOrthographic());
}

TEST_F(CameraTest, SetPerspectiveSwitchesMode) {
    m_camera.SetOrthographic(10.0f, 1.0f, 0.1f, 100.0f);
    m_camera.SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    EXPECT_EQ(m_camera.GetProjectionMode(), bimeup::renderer::ProjectionMode::Perspective);
    EXPECT_FALSE(m_camera.IsOrthographic());
}

TEST_F(CameraTest, ToggleProjectionSwapsMode) {
    m_camera.SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);

    m_camera.ToggleProjection();
    EXPECT_TRUE(m_camera.IsOrthographic());

    m_camera.ToggleProjection();
    EXPECT_FALSE(m_camera.IsOrthographic());
}

TEST_F(CameraTest, ToggleProjectionDoesNotChangeViewMatrix) {
    m_camera.SetPerspective(45.0f, 1.0f, 0.1f, 100.0f);
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);

    auto viewBefore = m_camera.GetViewMatrix();
    m_camera.ToggleProjection();
    auto viewAfter = m_camera.GetViewMatrix();

    EXPECT_TRUE(Mat4Near(viewBefore, viewAfter));
}

TEST_F(CameraTest, ToggleProjectionRoundTripRestoresPerspectiveMatrix) {
    m_camera.SetPerspective(45.0f, 1.5f, 0.1f, 100.0f);
    auto projBefore = m_camera.GetProjectionMatrix();

    m_camera.ToggleProjection();  // -> ortho
    m_camera.ToggleProjection();  // -> perspective

    EXPECT_EQ(m_camera.GetProjectionMode(), bimeup::renderer::ProjectionMode::Perspective);
    EXPECT_TRUE(Mat4Near(m_camera.GetProjectionMatrix(), projBefore));
}

TEST_F(CameraTest, FrameBoundsCentersPivotOnBoundsCenter) {
    m_camera.Frame(glm::vec3(-2.0f, 0.0f, 4.0f), glm::vec3(2.0f, 6.0f, 10.0f));

    // Camera orbits around the bounds center; view looks at it.
    glm::vec3 expectedCenter(0.0f, 3.0f, 7.0f);
    glm::vec3 forward = m_camera.GetForward();
    glm::vec3 pos = m_camera.GetPosition();
    // Forward from camera should point toward expected center.
    glm::vec3 toCenter = glm::normalize(expectedCenter - pos);
    EXPECT_GT(glm::dot(forward, toCenter), 0.999f);
}

TEST_F(CameraTest, FrameBoundsDistanceScalesWithSize) {
    // Frame a small bounds then a large one; distance should grow.
    m_camera.Frame(glm::vec3(-1.0f), glm::vec3(1.0f));
    float smallDist = glm::length(m_camera.GetPosition() - glm::vec3(0.0f));

    m_camera.Frame(glm::vec3(-50.0f), glm::vec3(50.0f));
    float largeDist = glm::length(m_camera.GetPosition() - glm::vec3(0.0f));

    EXPECT_GT(largeDist, smallDist);
}

TEST_F(CameraTest, FrameBoundsEnforcesMinimumDistance) {
    // Degenerate bounds (size = 0) should still produce a usable distance.
    m_camera.Frame(glm::vec3(5.0f), glm::vec3(5.0f));
    float dist = glm::length(m_camera.GetPosition() - glm::vec3(5.0f));
    EXPECT_GT(dist, 0.0f);
}

TEST_F(CameraTest, FrameBoundsInvalidMinMaxIsNoOp) {
    // min > max on any axis => invalid bounds; camera must not change.
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    auto posBefore = m_camera.GetPosition();

    m_camera.Frame(glm::vec3(1.0f), glm::vec3(-1.0f));

    EXPECT_TRUE(Vec3Near(m_camera.GetPosition(), posBefore));
}

TEST_F(CameraTest, AxisViewFrontLooksAlongNegativeZ) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Front);

    auto pos = m_camera.GetPosition();
    auto fwd = m_camera.GetForward();
    EXPECT_TRUE(Vec3Near(pos, glm::vec3(0.0f, 0.0f, 5.0f)));
    EXPECT_TRUE(Vec3Near(fwd, glm::vec3(0.0f, 0.0f, -1.0f)));
}

TEST_F(CameraTest, AxisViewBackLooksAlongPositiveZ) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Back);

    auto pos = m_camera.GetPosition();
    auto fwd = m_camera.GetForward();
    EXPECT_TRUE(Vec3Near(pos, glm::vec3(0.0f, 0.0f, -5.0f)));
    EXPECT_TRUE(Vec3Near(fwd, glm::vec3(0.0f, 0.0f, 1.0f)));
}

TEST_F(CameraTest, AxisViewRightLooksAlongNegativeX) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Right);

    auto pos = m_camera.GetPosition();
    auto fwd = m_camera.GetForward();
    EXPECT_TRUE(Vec3Near(pos, glm::vec3(5.0f, 0.0f, 0.0f)));
    EXPECT_TRUE(Vec3Near(fwd, glm::vec3(-1.0f, 0.0f, 0.0f)));
}

TEST_F(CameraTest, AxisViewLeftLooksAlongPositiveX) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Left);

    auto pos = m_camera.GetPosition();
    auto fwd = m_camera.GetForward();
    EXPECT_TRUE(Vec3Near(pos, glm::vec3(-5.0f, 0.0f, 0.0f)));
    EXPECT_TRUE(Vec3Near(fwd, glm::vec3(1.0f, 0.0f, 0.0f)));
}

TEST_F(CameraTest, AxisViewTopLooksDown) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Top);

    auto fwd = m_camera.GetForward();
    auto pos = m_camera.GetPosition();
    // Forward must point predominantly down (negative Y).
    EXPECT_LT(fwd.y, -0.99f);
    // Camera above target.
    EXPECT_GT(pos.y, 0.0f);
    // View matrix must be free of NaN (no gimbal singularity).
    auto view = m_camera.GetViewMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_FALSE(std::isnan(view[c][r]));
}

TEST_F(CameraTest, AxisViewBottomLooksUp) {
    m_camera.SetOrbitTarget(glm::vec3(0.0f));
    m_camera.SetDistance(5.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Bottom);

    auto fwd = m_camera.GetForward();
    auto pos = m_camera.GetPosition();
    EXPECT_GT(fwd.y, 0.99f);
    EXPECT_LT(pos.y, 0.0f);
    auto view = m_camera.GetViewMatrix();
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_FALSE(std::isnan(view[c][r]));
}

TEST_F(CameraTest, AxisViewPreservesDistanceAndTarget) {
    glm::vec3 target(3.0f, 4.0f, -2.0f);
    m_camera.SetOrbitTarget(target);
    m_camera.SetDistance(8.0f);
    m_camera.SetAxisView(bimeup::renderer::AxisView::Right);

    float dist = glm::length(m_camera.GetPosition() - target);
    EXPECT_NEAR(dist, 8.0f, kEpsilon);
    // Right view: camera should be at target + (d, 0, 0).
    EXPECT_TRUE(Vec3Near(m_camera.GetPosition(), target + glm::vec3(8.0f, 0.0f, 0.0f)));
}
