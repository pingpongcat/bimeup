#include <gtest/gtest.h>

#include <ui/ImGuiContext.h>

#include <imgui.h>
#include <ImGuizmo.h>

namespace {

TEST(ImGuizmoIntegrationTest, SymbolsLinkAndIsUsingIsFalseInitially) {
    bimeup::ui::ImGuiContext ctx;
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    EXPECT_FALSE(ImGuizmo::IsUsing());
    EXPECT_FALSE(ImGuizmo::IsOver());
}

}  // namespace
