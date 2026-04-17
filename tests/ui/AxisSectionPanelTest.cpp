#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <renderer/ClipPlaneManager.h>
#include <scene/AxisSectionController.h>
#include <ui/AxisSectionPanel.h>

using bimeup::renderer::ClipPlaneManager;
using bimeup::scene::Axis;
using bimeup::scene::AxisSectionController;
using bimeup::scene::AxisSectionSlot;
using bimeup::scene::SectionMode;
using bimeup::ui::AxisSectionPanel;
using bimeup::ui::ExtractAxisOffset;
using bimeup::ui::MakeAxisGizmoTransform;

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

TEST(AxisSectionPanelTest, MakeAxisGizmoTransformPlacesOffsetOnXOnly) {
    const glm::mat4 m = MakeAxisGizmoTransform(Axis::X, 3.5F);
    EXPECT_FLOAT_EQ(m[3][0], 3.5F);
    EXPECT_FLOAT_EQ(m[3][1], 0.0F);
    EXPECT_FLOAT_EQ(m[3][2], 0.0F);
    // Upper-left 3x3 is identity.
    EXPECT_FLOAT_EQ(m[0][0], 1.0F);
    EXPECT_FLOAT_EQ(m[1][1], 1.0F);
    EXPECT_FLOAT_EQ(m[2][2], 1.0F);
    EXPECT_FLOAT_EQ(m[3][3], 1.0F);
}

TEST(AxisSectionPanelTest, MakeAxisGizmoTransformPlacesOffsetOnYOnly) {
    const glm::mat4 m = MakeAxisGizmoTransform(Axis::Y, -2.25F);
    EXPECT_FLOAT_EQ(m[3][0], 0.0F);
    EXPECT_FLOAT_EQ(m[3][1], -2.25F);
    EXPECT_FLOAT_EQ(m[3][2], 0.0F);
}

TEST(AxisSectionPanelTest, MakeAxisGizmoTransformPlacesOffsetOnZOnly) {
    const glm::mat4 m = MakeAxisGizmoTransform(Axis::Z, 7.0F);
    EXPECT_FLOAT_EQ(m[3][0], 0.0F);
    EXPECT_FLOAT_EQ(m[3][1], 0.0F);
    EXPECT_FLOAT_EQ(m[3][2], 7.0F);
}

TEST(AxisSectionPanelTest, ExtractAxisOffsetReadsTranslationOnAxis) {
    glm::mat4 m{1.0F};
    m[3] = glm::vec4{11.0F, 22.0F, 33.0F, 1.0F};
    EXPECT_FLOAT_EQ(ExtractAxisOffset(m, Axis::X), 11.0F);
    EXPECT_FLOAT_EQ(ExtractAxisOffset(m, Axis::Y), 22.0F);
    EXPECT_FLOAT_EQ(ExtractAxisOffset(m, Axis::Z), 33.0F);
}

TEST(AxisSectionPanelTest, ExtractAxisOffsetRoundTripsMakeAxisGizmoTransform) {
    for (auto axis : {Axis::X, Axis::Y, Axis::Z}) {
        for (float off : {-9.5F, -0.1F, 0.0F, 0.25F, 100.0F}) {
            const glm::mat4 m = MakeAxisGizmoTransform(axis, off);
            EXPECT_FLOAT_EQ(ExtractAxisOffset(m, axis), off);
        }
    }
}

TEST(AxisSectionPanelTest, ExtractAxisOffsetIgnoresOtherAxisTranslations) {
    // Gizmo with OPERATION::TRANSLATE_X should never actually move Y/Z, but
    // if it does (numerical drift or different op), we still report only the
    // active-axis component so the slot offset doesn't absorb unrelated motion.
    glm::mat4 m{1.0F};
    m[3] = glm::vec4{4.0F, 99.0F, -88.0F, 1.0F};
    EXPECT_FLOAT_EQ(ExtractAxisOffset(m, Axis::X), 4.0F);
}

TEST(AxisSectionPanelTest, SetCameraMatricesRoundTrips) {
    AxisSectionPanel panel;
    glm::mat4 view{1.0F};
    view[3] = glm::vec4{1.0F, 2.0F, 3.0F, 1.0F};
    glm::mat4 proj{1.0F};
    proj[0][0] = 2.5F;
    panel.SetCameraMatrices(view, proj);
    EXPECT_EQ(panel.GetViewMatrix(), view);
    EXPECT_EQ(panel.GetProjectionMatrix(), proj);
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
