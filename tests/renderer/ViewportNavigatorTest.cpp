#include <gtest/gtest.h>
#include <renderer/ViewportNavigator.h>

using bimeup::renderer::ClassifyDrag;
using bimeup::renderer::NavAction;
using bimeup::renderer::NavButton;
using bimeup::renderer::NavModifiers;

TEST(ViewportNavigatorTest, MiddleButtonNoMods_Orbits) {
    EXPECT_EQ(ClassifyDrag(NavButton::Middle, {}), NavAction::Orbit);
}

TEST(ViewportNavigatorTest, ShiftMiddle_Pans) {
    NavModifiers m;
    m.shift = true;
    EXPECT_EQ(ClassifyDrag(NavButton::Middle, m), NavAction::Pan);
}

TEST(ViewportNavigatorTest, CtrlMiddle_Dollies) {
    NavModifiers m;
    m.ctrl = true;
    EXPECT_EQ(ClassifyDrag(NavButton::Middle, m), NavAction::Dolly);
}

TEST(ViewportNavigatorTest, ShiftAndCtrlMiddle_PrefersPan) {
    NavModifiers m;
    m.shift = true;
    m.ctrl = true;
    EXPECT_EQ(ClassifyDrag(NavButton::Middle, m), NavAction::Pan);
}

TEST(ViewportNavigatorTest, LeftButton_NoNavigation) {
    EXPECT_EQ(ClassifyDrag(NavButton::Left, {}), NavAction::None);
}

TEST(ViewportNavigatorTest, RightButton_NoNavigation) {
    // Blender-style: RMB is not used for viewport navigation.
    EXPECT_EQ(ClassifyDrag(NavButton::Right, {}), NavAction::None);
    NavModifiers m;
    m.shift = true;
    EXPECT_EQ(ClassifyDrag(NavButton::Right, m), NavAction::None);
}
