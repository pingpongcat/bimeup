#include <gtest/gtest.h>

#include <renderer/ClipPlaneManager.h>
#include <scene/AxisSectionController.h>
#include <ui/AxisSectionPanel.h>

using bimeup::renderer::ClipPlaneManager;
using bimeup::scene::Axis;
using bimeup::scene::AxisSectionController;
using bimeup::scene::AxisSectionSlot;
using bimeup::scene::SectionMode;
using bimeup::ui::AxisSectionPanel;

namespace {

TEST(AxisSectionPanelTest, HasPanelName) {
    AxisSectionPanel panel;
    EXPECT_STREQ(panel.GetName(), "Axis Section");
}

TEST(AxisSectionPanelTest, DefaultPanelIsVisible) {
    AxisSectionPanel panel;
    EXPECT_TRUE(panel.IsVisible());
}

TEST(AxisSectionPanelTest, ControllerNullUntilSet) {
    AxisSectionPanel panel;
    EXPECT_EQ(panel.GetController(), nullptr);

    AxisSectionController ctrl;
    panel.SetController(&ctrl);
    EXPECT_EQ(panel.GetController(), &ctrl);
}

TEST(AxisSectionPanelTest, ActiveAxisDefaultsToEmpty) {
    AxisSectionPanel panel;
    EXPECT_FALSE(panel.ActiveAxis().has_value());
}

TEST(AxisSectionPanelTest, SetActiveAxisPersists) {
    AxisSectionPanel panel;
    panel.SetActiveAxis(Axis::Y);
    ASSERT_TRUE(panel.ActiveAxis().has_value());
    EXPECT_EQ(*panel.ActiveAxis(), Axis::Y);

    panel.SetActiveAxis(std::nullopt);
    EXPECT_FALSE(panel.ActiveAxis().has_value());
}

TEST(AxisSectionPanelTest, ToggleAxisWithoutControllerIsSafe) {
    AxisSectionPanel panel;
    panel.ToggleAxis(Axis::X);  // must not crash or persist anything
    EXPECT_FALSE(panel.ActiveAxis().has_value());
}

TEST(AxisSectionPanelTest, ToggleAxisCreatesSlotAtOffsetZeroCutFront) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.ToggleAxis(Axis::X);

    ASSERT_TRUE(ctrl.HasSlot(Axis::X));
    EXPECT_FLOAT_EQ(ctrl.GetSlot(Axis::X)->offset, 0.0F);
    EXPECT_EQ(ctrl.GetSlot(Axis::X)->mode, SectionMode::CutFront);
}

TEST(AxisSectionPanelTest, ToggleAxisCreatesThenRemoves) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.ToggleAxis(Axis::Z);
    ASSERT_TRUE(ctrl.HasSlot(Axis::Z));

    panel.ToggleAxis(Axis::Z);
    EXPECT_FALSE(ctrl.HasSlot(Axis::Z));
}

TEST(AxisSectionPanelTest, ToggleAxisActivatesNewlyCreatedSlot) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.ToggleAxis(Axis::Y);

    ASSERT_TRUE(panel.ActiveAxis().has_value());
    EXPECT_EQ(*panel.ActiveAxis(), Axis::Y);
}

TEST(AxisSectionPanelTest, TogglingAwayTheActiveAxisClearsActive) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);
    panel.ToggleAxis(Axis::X);
    ASSERT_EQ(panel.ActiveAxis(), Axis::X);

    panel.ToggleAxis(Axis::X);

    EXPECT_FALSE(panel.ActiveAxis().has_value());
}

TEST(AxisSectionPanelTest, TogglingAnInactiveAxisDoesNotStealActive) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);
    panel.ToggleAxis(Axis::X);
    ASSERT_EQ(panel.ActiveAxis(), Axis::X);

    panel.ToggleAxis(Axis::Y);

    // Toggling a new axis on does NOT move the gizmo — user explicitly picks.
    ASSERT_TRUE(panel.ActiveAxis().has_value());
    EXPECT_EQ(*panel.ActiveAxis(), Axis::X);
    EXPECT_TRUE(ctrl.HasSlot(Axis::Y));
}

TEST(AxisSectionPanelTest, SetSlotModeWithoutSlotIsNoop) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.SetSlotMode(Axis::X, SectionMode::SectionOnly);

    EXPECT_FALSE(ctrl.HasSlot(Axis::X));
}

TEST(AxisSectionPanelTest, SetSlotModeUpdatesOnlyMode) {
    AxisSectionController ctrl;
    ctrl.SetSlot(Axis::X, {5.0F, SectionMode::CutFront});
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.SetSlotMode(Axis::X, SectionMode::SectionOnly);

    ASSERT_TRUE(ctrl.HasSlot(Axis::X));
    EXPECT_EQ(ctrl.GetSlot(Axis::X)->mode, SectionMode::SectionOnly);
    EXPECT_FLOAT_EQ(ctrl.GetSlot(Axis::X)->offset, 5.0F);  // preserved
}

TEST(AxisSectionPanelTest, SetSlotOffsetUpdatesOnlyOffset) {
    AxisSectionController ctrl;
    ctrl.SetSlot(Axis::Z, {0.0F, SectionMode::CutBack});
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.SetSlotOffset(Axis::Z, 3.25F);

    ASSERT_TRUE(ctrl.HasSlot(Axis::Z));
    EXPECT_FLOAT_EQ(ctrl.GetSlot(Axis::Z)->offset, 3.25F);
    EXPECT_EQ(ctrl.GetSlot(Axis::Z)->mode, SectionMode::CutBack);  // preserved
}

TEST(AxisSectionPanelTest, SetSlotOffsetWithoutSlotIsNoop) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);

    panel.SetSlotOffset(Axis::Y, 42.0F);

    EXPECT_FALSE(ctrl.HasSlot(Axis::Y));
}

TEST(AxisSectionPanelTest, OffsetRangeDefaultsAndCanBeOverridden) {
    AxisSectionPanel panel;
    EXPECT_FLOAT_EQ(panel.OffsetMin(), -10.0F);
    EXPECT_FLOAT_EQ(panel.OffsetMax(), 10.0F);

    panel.SetOffsetRange(-50.0F, 75.0F);
    EXPECT_FLOAT_EQ(panel.OffsetMin(), -50.0F);
    EXPECT_FLOAT_EQ(panel.OffsetMax(), 75.0F);
}

TEST(AxisSectionPanelTest, PruneActiveIfMissingClearsDanglingAxis) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);
    panel.ToggleAxis(Axis::X);
    ctrl.ClearSlot(Axis::X);  // external removal

    panel.PruneActiveIfMissing();

    EXPECT_FALSE(panel.ActiveAxis().has_value());
}

TEST(AxisSectionPanelTest, PruneActiveIfMissingKeepsLiveAxis) {
    AxisSectionController ctrl;
    AxisSectionPanel panel;
    panel.SetController(&ctrl);
    panel.ToggleAxis(Axis::Y);

    panel.PruneActiveIfMissing();

    ASSERT_TRUE(panel.ActiveAxis().has_value());
    EXPECT_EQ(*panel.ActiveAxis(), Axis::Y);
}

TEST(AxisSectionPanelTest, PruneActiveIfMissingSafeWithoutController) {
    AxisSectionPanel panel;
    panel.SetActiveAxis(Axis::X);

    panel.PruneActiveIfMissing();

    // No controller → cannot verify; leave the selection untouched.
    ASSERT_TRUE(panel.ActiveAxis().has_value());
    EXPECT_EQ(*panel.ActiveAxis(), Axis::X);
}

TEST(AxisSectionPanelTest, ControllerSwitchClearsActiveAxis) {
    AxisSectionController a;
    AxisSectionController b;
    AxisSectionPanel panel;
    panel.SetController(&a);
    panel.ToggleAxis(Axis::X);
    ASSERT_TRUE(panel.ActiveAxis().has_value());

    panel.SetController(&b);

    EXPECT_FALSE(panel.ActiveAxis().has_value());
}

}  // namespace
