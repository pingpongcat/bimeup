#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <scene/AxisSectionController.h>
#include <ui/AxisSectionPanel.h>

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

TEST(AxisSectionPanelTest, ToggleAxisWithoutControllerIsSafe) {
    AxisSectionPanel panel;
    panel.ToggleAxis(Axis::X);  // must not crash
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

}  // namespace
