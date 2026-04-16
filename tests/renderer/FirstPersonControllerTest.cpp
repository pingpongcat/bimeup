#include <gtest/gtest.h>
#include <renderer/Camera.h>
#include <renderer/FirstPersonController.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

using bimeup::renderer::Camera;
using bimeup::renderer::FirstPersonController;

namespace {

constexpr float kEps = 1e-4F;

bool ApproxEq(const glm::vec3& a, const glm::vec3& b, float eps = kEps) {
    return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
}

}  // namespace

TEST(FirstPersonControllerTest, DefaultsAtOriginLookingMinusZ) {
    FirstPersonController fpc;
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(0.0F)));
    EXPECT_FLOAT_EQ(fpc.GetYaw(), 0.0F);
    EXPECT_FLOAT_EQ(fpc.GetPitch(), 0.0F);
    // Camera convention: yaw=0,pitch=0 → forward = -Z.
    EXPECT_TRUE(ApproxEq(fpc.GetForward(), glm::vec3(0.0F, 0.0F, -1.0F)));
}

TEST(FirstPersonControllerTest, SetPositionRoundTrip) {
    FirstPersonController fpc;
    fpc.SetPosition({1.5F, 2.0F, -3.5F});
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(1.5F, 2.0F, -3.5F)));
}

TEST(FirstPersonControllerTest, SetYawPitchRoundTrip) {
    FirstPersonController fpc;
    fpc.SetYawPitch(0.7F, -0.3F);
    EXPECT_NEAR(fpc.GetYaw(), 0.7F, kEps);
    EXPECT_NEAR(fpc.GetPitch(), -0.3F, kEps);
}

TEST(FirstPersonControllerTest, GazeAlongPlusZ_YawIsPi) {
    // PROGRESS.md spec: PoV teleport puts camera "gaze along +Z".
    FirstPersonController fpc;
    fpc.SetYawPitch(glm::pi<float>(), 0.0F);
    EXPECT_TRUE(ApproxEq(fpc.GetForward(), glm::vec3(0.0F, 0.0F, 1.0F)));
}

TEST(FirstPersonControllerTest, LookAccumulatesYawAndInvertsPitchSign) {
    FirstPersonController fpc;
    // Mouse delta: x grows to the right (yaw should turn right = +yaw),
    // y grows downward (pitch should look down = -pitch in our +Y-up convention).
    fpc.Look({100.0F, 50.0F}, 0.01F);
    EXPECT_NEAR(fpc.GetYaw(), 1.0F, kEps);
    EXPECT_NEAR(fpc.GetPitch(), -0.5F, kEps);
}

TEST(FirstPersonControllerTest, PitchClampedToHalfPi) {
    FirstPersonController fpc;
    fpc.Look({0.0F, -100000.0F}, 0.01F);  // huge upward push
    EXPECT_LE(fpc.GetPitch(), glm::half_pi<float>());
    EXPECT_GE(fpc.GetPitch(), glm::half_pi<float>() - 0.01F);

    fpc.Look({0.0F, 100000.0F}, 0.01F);  // huge downward push
    EXPECT_GE(fpc.GetPitch(), -glm::half_pi<float>());
    EXPECT_LE(fpc.GetPitch(), -glm::half_pi<float>() + 0.01F);
}

TEST(FirstPersonControllerTest, MoveForward_FacingMinusZ_AdvancesAlongMinusZ) {
    FirstPersonController fpc;  // yaw=0 → forward = -Z
    fpc.Move({0.0F, 0.0F, 1.0F}, /*dt=*/1.0F, /*speed=*/2.0F);
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(0.0F, 0.0F, -2.0F)));
}

TEST(FirstPersonControllerTest, MoveForward_GazingPlusZ_AdvancesAlongPlusZ) {
    FirstPersonController fpc;
    fpc.SetYawPitch(glm::pi<float>(), 0.0F);  // gaze +Z
    fpc.Move({0.0F, 0.0F, 1.0F}, /*dt=*/1.0F, /*speed=*/3.0F);
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(0.0F, 0.0F, 3.0F)));
}

TEST(FirstPersonControllerTest, MoveStrafe_FacingMinusZ_GoesAlongPlusX) {
    // Right-handed: right vector = forward × world_up. With forward=-Z, up=+Y,
    // right = (-Z) × (+Y) = +X. So strafe right (+x input) → +X world motion.
    FirstPersonController fpc;
    fpc.Move({1.0F, 0.0F, 0.0F}, 1.0F, 4.0F);
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(4.0F, 0.0F, 0.0F)));
}

TEST(FirstPersonControllerTest, MoveVertical_AlongWorldY) {
    FirstPersonController fpc;
    fpc.Move({0.0F, 1.0F, 0.0F}, 1.0F, 5.0F);
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(0.0F, 5.0F, 0.0F)));
}

TEST(FirstPersonControllerTest, MoveForward_PitchedDown_StaysHorizontal) {
    // First-person walking shouldn't change altitude when looking down — only yaw
    // affects horizontal direction.
    FirstPersonController fpc;
    fpc.SetYawPitch(0.0F, -1.0F);  // looking down
    fpc.Move({0.0F, 0.0F, 1.0F}, 1.0F, 2.0F);
    EXPECT_NEAR(fpc.GetPosition().y, 0.0F, kEps);
    // Magnitude in XZ plane equals speed * dt.
    const float xz = std::sqrt(fpc.GetPosition().x * fpc.GetPosition().x +
                               fpc.GetPosition().z * fpc.GetPosition().z);
    EXPECT_NEAR(xz, 2.0F, kEps);
}

TEST(FirstPersonControllerTest, MoveScalesByDtAndSpeed) {
    FirstPersonController fpc;
    fpc.Move({0.0F, 0.0F, 1.0F}, 0.25F, 8.0F);  // 0.25 * 8 = 2 units forward
    EXPECT_TRUE(ApproxEq(fpc.GetPosition(), glm::vec3(0.0F, 0.0F, -2.0F)));
}

TEST(FirstPersonControllerTest, ApplyToCamera_PositionMatches) {
    FirstPersonController fpc;
    fpc.SetPosition({3.0F, 2.0F, 5.0F});
    fpc.SetYawPitch(glm::pi<float>(), 0.0F);  // gaze +Z

    Camera camera;
    camera.SetPerspective(60.0F, 1.0F, 0.1F, 100.0F);
    fpc.ApplyTo(camera);

    EXPECT_TRUE(ApproxEq(camera.GetPosition(), glm::vec3(3.0F, 2.0F, 5.0F), 1e-3F));
    EXPECT_TRUE(ApproxEq(camera.GetForward(), glm::vec3(0.0F, 0.0F, 1.0F), 1e-3F));
}

TEST(FirstPersonControllerTest, ApplyToCamera_PreservesYawPitchInForward) {
    FirstPersonController fpc;
    fpc.SetPosition({0.0F, 1.7F, 0.0F});
    fpc.SetYawPitch(0.5F, -0.2F);

    Camera camera;
    camera.SetPerspective(60.0F, 1.0F, 0.1F, 100.0F);
    fpc.ApplyTo(camera);

    // Camera forward should match the controller's forward direction.
    EXPECT_TRUE(ApproxEq(camera.GetForward(), fpc.GetForward(), 1e-3F));
    EXPECT_TRUE(ApproxEq(camera.GetPosition(), glm::vec3(0.0F, 1.7F, 0.0F), 1e-3F));
}
