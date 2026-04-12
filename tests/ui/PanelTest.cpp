#include <gtest/gtest.h>

#include <ui/Panel.h>
#include <ui/UIManager.h>

#include <vulkan/vulkan.h>

#include <memory>

namespace {

class TestPanel : public bimeup::ui::Panel {
public:
    [[nodiscard]] const char* GetName() const override { return "Test"; }
    void OnDraw() override { ++drawCount; }
    int drawCount = 0;
};

TEST(PanelTest, DefaultsToVisible) {
    TestPanel panel;
    EXPECT_TRUE(panel.IsVisible());
}

TEST(PanelTest, ClosedPanelIsNotVisible) {
    TestPanel panel;
    panel.Close();
    EXPECT_FALSE(panel.IsVisible());
}

TEST(PanelTest, OpenedPanelIsVisible) {
    TestPanel panel;
    panel.Close();
    panel.Open();
    EXPECT_TRUE(panel.IsVisible());
}

TEST(PanelTest, ToggleFlipsVisibility) {
    TestPanel panel;
    EXPECT_TRUE(panel.IsVisible());
    panel.Toggle();
    EXPECT_FALSE(panel.IsVisible());
    panel.Toggle();
    EXPECT_TRUE(panel.IsVisible());
}

TEST(PanelTest, SetVisibleControlsVisibility) {
    TestPanel panel;
    panel.SetVisible(false);
    EXPECT_FALSE(panel.IsVisible());
    panel.SetVisible(true);
    EXPECT_TRUE(panel.IsVisible());
}

TEST(PanelTest, UIManagerSkipsDrawWhenHidden) {
    bimeup::ui::UIManager manager;
    auto panel = std::make_unique<TestPanel>();
    auto* raw = panel.get();
    raw->Close();
    manager.AddPanel(std::move(panel));

    manager.BeginFrame();
    manager.EndFrame(VK_NULL_HANDLE);

    EXPECT_EQ(raw->drawCount, 0);
}

TEST(PanelTest, UIManagerDrawsVisiblePanel) {
    bimeup::ui::UIManager manager;
    auto panel = std::make_unique<TestPanel>();
    auto* raw = panel.get();
    manager.AddPanel(std::move(panel));

    manager.BeginFrame();
    manager.EndFrame(VK_NULL_HANDLE);

    EXPECT_EQ(raw->drawCount, 1);
}

}  // namespace
