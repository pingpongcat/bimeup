#include <gtest/gtest.h>

#include <ui/FirstPersonExitPanel.h>

namespace {

using bimeup::ui::FirstPersonExitPanel;

TEST(FirstPersonExitPanelTest, HasPanelName) {
    FirstPersonExitPanel panel;
    EXPECT_STREQ(panel.GetName(), "First Person");
}

TEST(FirstPersonExitPanelTest, HiddenByDefault) {
    FirstPersonExitPanel panel;
    EXPECT_FALSE(panel.IsVisible());
}

TEST(FirstPersonExitPanelTest, CanBeOpenedAndClosed) {
    FirstPersonExitPanel panel;
    panel.Open();
    EXPECT_TRUE(panel.IsVisible());
    panel.Close();
    EXPECT_FALSE(panel.IsVisible());
}

TEST(FirstPersonExitPanelTest, TriggerExitInvokesCallback) {
    FirstPersonExitPanel panel;
    int calls = 0;
    panel.SetOnExit([&calls] { ++calls; });
    panel.TriggerExit();
    panel.TriggerExit();
    EXPECT_EQ(calls, 2);
}

TEST(FirstPersonExitPanelTest, TriggerExitNoCallbackIsNoop) {
    FirstPersonExitPanel panel;
    panel.TriggerExit();  // must not crash
    SUCCEED();
}

}  // namespace
