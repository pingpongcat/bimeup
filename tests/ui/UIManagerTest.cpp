#include <gtest/gtest.h>

#include <ui/Panel.h>
#include <ui/UIManager.h>

#include <vulkan/vulkan.h>

#include <memory>

namespace {

class DummyPanel : public bimeup::ui::Panel {
public:
    [[nodiscard]] const char* GetName() const override { return "Dummy"; }
    void OnDraw() override { ++drawCount; }
    int drawCount = 0;
};

TEST(UIManagerTest, CreatesAndDestroysWithoutCrash) {
    bimeup::ui::UIManager manager;
    EXPECT_EQ(manager.PanelCount(), 0U);
}

TEST(UIManagerTest, AddsPanel) {
    bimeup::ui::UIManager manager;
    manager.AddPanel(std::make_unique<DummyPanel>());
    EXPECT_EQ(manager.PanelCount(), 1U);
}

TEST(UIManagerTest, AddingNullPanelIsIgnored) {
    bimeup::ui::UIManager manager;
    manager.AddPanel(nullptr);
    EXPECT_EQ(manager.PanelCount(), 0U);
}

TEST(UIManagerTest, DestroysWithAttachedPanels) {
    bimeup::ui::UIManager manager;
    manager.AddPanel(std::make_unique<DummyPanel>());
    manager.AddPanel(std::make_unique<DummyPanel>());
    EXPECT_EQ(manager.PanelCount(), 2U);
}

TEST(UIManagerTest, BeginFrameInvokesPanelDraw) {
    bimeup::ui::UIManager manager;
    auto panel = std::make_unique<DummyPanel>();
    auto* raw = panel.get();
    manager.AddPanel(std::move(panel));

    manager.BeginFrame();
    manager.EndFrame(VK_NULL_HANDLE);

    EXPECT_EQ(raw->drawCount, 1);
}

TEST(UIManagerTest, EndFrameWithoutBeginFrameIsSafe) {
    bimeup::ui::UIManager manager;
    manager.EndFrame(VK_NULL_HANDLE);
}

TEST(UIManagerTest, ExposesImGuiContext) {
    bimeup::ui::UIManager manager;
    EXPECT_FALSE(manager.GetContext().HasVulkanBackend());
}

}  // namespace
