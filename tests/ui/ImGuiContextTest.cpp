#include <gtest/gtest.h>

#include <ui/ImGuiContext.h>

#include <imgui.h>

namespace {

TEST(ImGuiContextTest, CreatesAndDestroysContext) {
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    {
        bimeup::ui::ImGuiContext ctx;
        EXPECT_NE(ImGui::GetCurrentContext(), nullptr);
        EXPECT_FALSE(ctx.HasVulkanBackend());
    }
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
}

TEST(ImGuiContextTest, EnablesDockingAndKeyboardNav) {
    bimeup::ui::ImGuiContext ctx;
    const ImGuiIO& io = ImGui::GetIO();
    EXPECT_TRUE((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0);
    EXPECT_TRUE((io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0);
}

}  // namespace
