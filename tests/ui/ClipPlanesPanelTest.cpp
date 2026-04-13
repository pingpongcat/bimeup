#include <gtest/gtest.h>

#include <renderer/ClipPlaneManager.h>
#include <ui/ClipPlanesPanel.h>

using bimeup::renderer::ClipPlaneManager;
using bimeup::ui::AxisPreset;
using bimeup::ui::ClipPlanesPanel;
using bimeup::ui::MakeAxisPlaneEquation;

namespace {

TEST(ClipPlanesPanelTest, HasPanelName) {
    ClipPlanesPanel panel;
    EXPECT_STREQ(panel.GetName(), "Clipping Planes");
}

TEST(ClipPlanesPanelTest, DefaultPanelIsVisible) {
    ClipPlanesPanel panel;
    EXPECT_TRUE(panel.IsVisible());
}

TEST(ClipPlanesPanelTest, ManagerIsNullUntilSet) {
    ClipPlanesPanel panel;
    EXPECT_EQ(panel.GetManager(), nullptr);
    ClipPlaneManager mgr;
    panel.SetManager(&mgr);
    EXPECT_EQ(panel.GetManager(), &mgr);
}

TEST(ClipPlanesPanelTest, AxisPresetXPositiveNormalIsPlusX) {
    const auto eq = MakeAxisPlaneEquation(AxisPreset::XPositive);
    EXPECT_FLOAT_EQ(eq.x, 1.0F);
    EXPECT_FLOAT_EQ(eq.y, 0.0F);
    EXPECT_FLOAT_EQ(eq.z, 0.0F);
    EXPECT_FLOAT_EQ(eq.w, 0.0F);
}

TEST(ClipPlanesPanelTest, AxisPresetYNegativeNormalIsMinusY) {
    const auto eq = MakeAxisPlaneEquation(AxisPreset::YNegative);
    EXPECT_FLOAT_EQ(eq.x, 0.0F);
    EXPECT_FLOAT_EQ(eq.y, -1.0F);
    EXPECT_FLOAT_EQ(eq.z, 0.0F);
    EXPECT_FLOAT_EQ(eq.w, 0.0F);
}

TEST(ClipPlanesPanelTest, AxisPresetZPositiveNormalIsPlusZ) {
    const auto eq = MakeAxisPlaneEquation(AxisPreset::ZPositive);
    EXPECT_FLOAT_EQ(eq.x, 0.0F);
    EXPECT_FLOAT_EQ(eq.y, 0.0F);
    EXPECT_FLOAT_EQ(eq.z, 1.0F);
    EXPECT_FLOAT_EQ(eq.w, 0.0F);
}

TEST(ClipPlanesPanelTest, ActivePlaneIdDefaultsToEmpty) {
    ClipPlanesPanel panel;
    EXPECT_FALSE(panel.ActivePlaneId().has_value());
}

TEST(ClipPlanesPanelTest, SetActivePlaneIdIsPersisted) {
    ClipPlanesPanel panel;
    panel.SetActivePlaneId(7U);
    ASSERT_TRUE(panel.ActivePlaneId().has_value());
    EXPECT_EQ(*panel.ActivePlaneId(), 7U);
    panel.SetActivePlaneId(std::nullopt);
    EXPECT_FALSE(panel.ActivePlaneId().has_value());
}

TEST(ClipPlanesPanelTest, ActiveGizmoModeDefaultsToTranslate) {
    ClipPlanesPanel panel;
    EXPECT_EQ(panel.ActiveGizmoMode(), bimeup::ui::GizmoMode::Translate);
}

}  // namespace
