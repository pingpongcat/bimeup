#include <gtest/gtest.h>

#include <ui/ViewportOverlay.h>

namespace {

using bimeup::ui::ViewportOverlay;

TEST(ViewportOverlayTest, HasPanelName) {
    ViewportOverlay overlay;
    EXPECT_STREQ(overlay.GetName(), "Viewport Overlay");
}

TEST(ViewportOverlayTest, IsAPanel) {
    ViewportOverlay overlay;
    EXPECT_TRUE(overlay.IsVisible());
    overlay.Close();
    EXPECT_FALSE(overlay.IsVisible());
}

}  // namespace
