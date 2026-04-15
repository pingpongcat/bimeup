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

TEST(ClipPlanesPanelTest, PruneActiveIfMissingKeepsExistingId) {
    ClipPlaneManager mgr;
    const std::uint32_t id = mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F});

    ClipPlanesPanel panel;
    panel.SetManager(&mgr);
    panel.SetActivePlaneId(id);

    panel.PruneActiveIfMissing();

    ASSERT_TRUE(panel.ActivePlaneId().has_value());
    EXPECT_EQ(*panel.ActivePlaneId(), id);
}

TEST(ClipPlanesPanelTest, PruneActiveIfMissingClearsDanglingId) {
    ClipPlaneManager mgr;
    const std::uint32_t id = mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F});

    ClipPlanesPanel panel;
    panel.SetManager(&mgr);
    panel.SetActivePlaneId(id);
    mgr.RemovePlane(id);

    panel.PruneActiveIfMissing();

    EXPECT_FALSE(panel.ActivePlaneId().has_value());
}

TEST(ClipPlanesPanelTest, PruneActiveIfMissingIsSafeWithoutManager) {
    ClipPlanesPanel panel;
    panel.SetActivePlaneId(42U);
    panel.PruneActiveIfMissing();
    // Without a manager we cannot verify membership; leave the id untouched.
    ASSERT_TRUE(panel.ActivePlaneId().has_value());
    EXPECT_EQ(*panel.ActivePlaneId(), 42U);
}

// 7.5b: the panel's "Section fill" checkbox + ColorEdit4 call SetSectionFill
// and SetFillColor on the active plane. Verify that the wiring target (the
// manager setters used from per-plane UI rows) edits exactly one plane and
// leaves other planes and the panel's active-plane selection intact.
TEST(ClipPlanesPanelTest, SectionFillEditsScopedToTargetPlane) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    const std::uint32_t b = mgr.AddPlane({0.0F, 1.0F, 0.0F, -2.0F});

    ClipPlanesPanel panel;
    panel.SetManager(&mgr);
    panel.SetActivePlaneId(a);

    ASSERT_TRUE(mgr.SetSectionFill(b, true));
    ASSERT_TRUE(mgr.SetFillColor(b, {0.1F, 0.2F, 0.3F, 0.5F}));

    // Plane a must not be touched.
    const auto* pa = mgr.Find(a);
    ASSERT_NE(pa, nullptr);
    EXPECT_FALSE(pa->sectionFill);
    EXPECT_FLOAT_EQ(pa->fillColor.r, 0.6F);
    EXPECT_FLOAT_EQ(pa->fillColor.a, 1.0F);

    const auto* pb = mgr.Find(b);
    ASSERT_NE(pb, nullptr);
    EXPECT_TRUE(pb->sectionFill);
    EXPECT_FLOAT_EQ(pb->fillColor.r, 0.1F);
    EXPECT_FLOAT_EQ(pb->fillColor.a, 0.5F);
    // Equation preserved — section setters must not clobber the plane equation.
    EXPECT_FLOAT_EQ(pb->equation.y, 1.0F);
    EXPECT_FLOAT_EQ(pb->equation.w, -2.0F);

    // Active-plane selection unaffected by unrelated plane edits.
    ASSERT_TRUE(panel.ActivePlaneId().has_value());
    EXPECT_EQ(*panel.ActivePlaneId(), a);
}

}  // namespace
