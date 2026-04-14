#include <gtest/gtest.h>

#include <renderer/Camera.h>
#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>
#include <ui/PlanViewPanel.h>

using bimeup::renderer::Camera;
using bimeup::renderer::ClassifyPoint;
using bimeup::renderer::ClipPlane;
using bimeup::renderer::ClipPlaneManager;
using bimeup::renderer::PointSide;
using bimeup::ui::ComputePlanClipEquation;
using bimeup::ui::PlanLevel;
using bimeup::ui::PlanViewPanel;

namespace {

TEST(PlanViewPanelTest, HasPanelName) {
    PlanViewPanel panel;
    EXPECT_STREQ(panel.GetName(), "Plan View");
}

TEST(PlanViewPanelTest, DefaultPanelIsVisible) {
    PlanViewPanel panel;
    EXPECT_TRUE(panel.IsVisible());
}

TEST(PlanViewPanelTest, DefaultActiveLevelIsNone) {
    PlanViewPanel panel;
    EXPECT_EQ(panel.ActiveLevel(), -1);
}

TEST(PlanViewPanelTest, SetLevelsStoresLevels) {
    PlanViewPanel panel;
    panel.SetLevels({{"Ground Floor", 0.0F}, {"Roof", 2.5F}});
    ASSERT_EQ(panel.Levels().size(), 2U);
    EXPECT_EQ(panel.Levels()[0].name, "Ground Floor");
    EXPECT_FLOAT_EQ(panel.Levels()[0].elevation, 0.0F);
    EXPECT_EQ(panel.Levels()[1].name, "Roof");
    EXPECT_FLOAT_EQ(panel.Levels()[1].elevation, 2.5F);
}

TEST(PlanViewPanelTest, ComputePlanClipEquationHasDownwardNormalAndOffset) {
    const auto eq = ComputePlanClipEquation(0.0F, 1.2F);
    EXPECT_FLOAT_EQ(eq.x, 0.0F);
    EXPECT_FLOAT_EQ(eq.y, -1.0F);
    EXPECT_FLOAT_EQ(eq.z, 0.0F);
    EXPECT_FLOAT_EQ(eq.w, 1.2F);
}

TEST(PlanViewPanelTest, ComputePlanClipEquationKeepsBelowCut) {
    const auto eq = ComputePlanClipEquation(2.5F, 1.2F);  // cut at y = 3.7
    ClipPlane plane;
    plane.equation = eq;
    EXPECT_EQ(ClassifyPoint(plane, {0.0F, 3.0F, 0.0F}), PointSide::Front);
    EXPECT_EQ(ClassifyPoint(plane, {0.0F, 4.0F, 0.0F}), PointSide::Back);
}

TEST(PlanViewPanelTest, ActivateLevelCreatesClipPlane) {
    ClipPlaneManager mgr;
    PlanViewPanel panel;
    panel.SetClipPlaneManager(&mgr);
    panel.SetLevels({{"Ground Floor", 0.0F}, {"Roof", 2.5F}});

    ASSERT_TRUE(panel.ActivateLevel(0));
    EXPECT_EQ(panel.ActiveLevel(), 0);
    EXPECT_EQ(mgr.Count(), 1U);
}

TEST(PlanViewPanelTest, ActivateLevelReusesSamePlaneEntry) {
    ClipPlaneManager mgr;
    PlanViewPanel panel;
    panel.SetClipPlaneManager(&mgr);
    panel.SetLevels({{"Ground Floor", 0.0F}, {"Roof", 2.5F}});

    ASSERT_TRUE(panel.ActivateLevel(0));
    ASSERT_TRUE(panel.ActivateLevel(1));
    EXPECT_EQ(panel.ActiveLevel(), 1);
    EXPECT_EQ(mgr.Count(), 1U);
}

TEST(PlanViewPanelTest, DeactivateRemovesManagedPlane) {
    ClipPlaneManager mgr;
    PlanViewPanel panel;
    panel.SetClipPlaneManager(&mgr);
    panel.SetLevels({{"Ground Floor", 0.0F}});

    ASSERT_TRUE(panel.ActivateLevel(0));
    ASSERT_EQ(mgr.Count(), 1U);

    panel.Deactivate();
    EXPECT_EQ(panel.ActiveLevel(), -1);
    EXPECT_EQ(mgr.Count(), 0U);
}

TEST(PlanViewPanelTest, ActivateLevelRejectsOutOfRangeIndex) {
    ClipPlaneManager mgr;
    PlanViewPanel panel;
    panel.SetClipPlaneManager(&mgr);
    panel.SetLevels({{"Ground Floor", 0.0F}});

    EXPECT_FALSE(panel.ActivateLevel(5));
    EXPECT_EQ(panel.ActiveLevel(), -1);
    EXPECT_EQ(mgr.Count(), 0U);
}

TEST(PlanViewPanelTest, ActivateLevelIsSafeWithoutManager) {
    PlanViewPanel panel;
    panel.SetLevels({{"Ground Floor", 0.0F}});
    EXPECT_TRUE(panel.ActivateLevel(0));
    EXPECT_EQ(panel.ActiveLevel(), 0);
}

TEST(PlanViewPanelTest, ActivateLevelSwitchesCameraToOrthographicTop) {
    Camera camera;
    camera.SetPerspective(45.0F, 1.0F, 0.1F, 100.0F);
    ASSERT_FALSE(camera.IsOrthographic());

    PlanViewPanel panel;
    panel.SetCamera(&camera);
    panel.SetSceneBounds({-5.0F, 0.0F, -3.0F}, {5.0F, 3.0F, 3.0F});
    panel.SetLevels({{"Ground Floor", 0.0F}});

    ASSERT_TRUE(panel.ActivateLevel(0));
    EXPECT_TRUE(camera.IsOrthographic());
    // Top view: forward should point nearly along -Y (pitch is clamped just
    // under π/2 to avoid the lookAt up-vector singularity).
    const glm::vec3 fwd = camera.GetForward();
    EXPECT_NEAR(fwd.y, -1.0F, 1e-2F);
    EXPECT_NEAR(fwd.x, 0.0F, 1e-3F);  // yaw must be 0 so the plan isn't rotated
}

TEST(PlanViewPanelTest, ActivateLevelResetsInheritedYawSoPlanIsAxisAligned) {
    // Simulate a Free-3D camera rotated arbitrarily before the user opens the
    // plan view. Without an explicit yaw reset, SetAxisView(Top) would keep
    // the previous yaw and the floor plan would appear rotated on screen.
    Camera camera;
    camera.SetPerspective(45.0F, 1.0F, 0.1F, 100.0F);
    camera.Orbit(1.3F, 0.4F);  // arbitrary yaw + pitch

    PlanViewPanel panel;
    panel.SetCamera(&camera);
    panel.SetSceneBounds({-5.0F, 0.0F, -3.0F}, {5.0F, 3.0F, 3.0F});
    panel.SetLevels({{"Ground Floor", 0.0F}});

    ASSERT_TRUE(panel.ActivateLevel(0));
    // With yaw=0 the top-down forward must have no XZ component besides the
    // tiny residual from pitch being clamped just under π/2.
    const glm::vec3 fwd = camera.GetForward();
    // With yaw=0, fwd.x must be ~0 (fwd.x = -cos(pitch)*sin(yaw)). The
    // previous Orbit-induced yaw of 1.3 rad would have produced fwd.x ~ 0.07.
    EXPECT_NEAR(fwd.x, 0.0F, 1e-3F);
}

}  // namespace
