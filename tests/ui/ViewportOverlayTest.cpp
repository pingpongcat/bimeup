#include <gtest/gtest.h>

#include <ui/ViewportOverlay.h>

#include <glm/vec3.hpp>

namespace {

using bimeup::ui::ViewportOverlay;

TEST(ViewportOverlayTest, HasPanelName) {
    ViewportOverlay overlay;
    EXPECT_STREQ(overlay.GetName(), "Viewport Overlay");
}

TEST(ViewportOverlayTest, DefaultFpsIsZero) {
    ViewportOverlay overlay;
    EXPECT_FLOAT_EQ(overlay.GetFps(), 0.0F);
}

TEST(ViewportOverlayTest, SetFpsUpdatesValue) {
    ViewportOverlay overlay;
    overlay.SetFps(60.0F);
    EXPECT_FLOAT_EQ(overlay.GetFps(), 60.0F);
}

TEST(ViewportOverlayTest, SetCameraPositionStoresValue) {
    ViewportOverlay overlay;
    overlay.SetCameraPosition({1.0F, 2.0F, 3.0F});
    const glm::vec3 p = overlay.GetCameraPosition();
    EXPECT_FLOAT_EQ(p.x, 1.0F);
    EXPECT_FLOAT_EQ(p.y, 2.0F);
    EXPECT_FLOAT_EQ(p.z, 3.0F);
}

TEST(ViewportOverlayTest, SetCameraForwardStoresValue) {
    ViewportOverlay overlay;
    overlay.SetCameraForward({0.0F, 0.0F, -1.0F});
    const glm::vec3 f = overlay.GetCameraForward();
    EXPECT_FLOAT_EQ(f.x, 0.0F);
    EXPECT_FLOAT_EQ(f.y, 0.0F);
    EXPECT_FLOAT_EQ(f.z, -1.0F);
}

TEST(ViewportOverlayTest, AxesGizmoVisibleByDefault) {
    ViewportOverlay overlay;
    EXPECT_TRUE(overlay.IsAxesGizmoVisible());
}

TEST(ViewportOverlayTest, SetAxesGizmoVisibleUpdatesValue) {
    ViewportOverlay overlay;
    overlay.SetAxesGizmoVisible(false);
    EXPECT_FALSE(overlay.IsAxesGizmoVisible());
}

TEST(ViewportOverlayTest, FpsCounterVisibleByDefault) {
    ViewportOverlay overlay;
    EXPECT_TRUE(overlay.IsFpsCounterVisible());
}

TEST(ViewportOverlayTest, CameraInfoVisibleByDefault) {
    ViewportOverlay overlay;
    EXPECT_TRUE(overlay.IsCameraInfoVisible());
}

TEST(ViewportOverlayTest, IsAPanel) {
    ViewportOverlay overlay;
    EXPECT_TRUE(overlay.IsVisible());
    overlay.Close();
    EXPECT_FALSE(overlay.IsVisible());
}

}  // namespace
