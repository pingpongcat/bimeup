#include <gtest/gtest.h>

#include <ui/ImGuiContext.h>
#include <ui/Theme.h>

#include <imgui.h>

#include <cmath>

namespace {

bool ApproxEq(const ImVec4& a, const ImVec4& b, float eps = 1e-4f) {
    return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps &&
           std::abs(a.z - b.z) < eps && std::abs(a.w - b.w) < eps;
}

TEST(ThemeTest, BimColorsAreDeterministic) {
    const auto a = bimeup::ui::Theme::BimColors();
    const auto b = bimeup::ui::Theme::BimColors();
    EXPECT_TRUE(ApproxEq(a.background, b.background));
    EXPECT_TRUE(ApproxEq(a.accent, b.accent));
}

TEST(ThemeTest, BimColorsHaveHighContrastText) {
    const auto c = bimeup::ui::Theme::BimColors();
    const float bgL = 0.2126f * c.background.x + 0.7152f * c.background.y +
                      0.0722f * c.background.z;
    const float txL =
        0.2126f * c.text.x + 0.7152f * c.text.y + 0.0722f * c.text.z;
    // High contrast: text luminance should differ from background by >= 0.5
    EXPECT_GE(std::abs(txL - bgL), 0.5f);
}

TEST(ThemeTest, ApplyChangesStyleFromDefaultDark) {
    bimeup::ui::ImGuiContext ctx;
    ImGui::StyleColorsDark();
    const ImVec4 defaultWindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

    bimeup::ui::Theme::Apply();
    const ImVec4 themedWindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

    EXPECT_FALSE(ApproxEq(defaultWindowBg, themedWindowBg));
}

TEST(ThemeTest, ApplySetsExpectedColorsOnCurrentStyle) {
    bimeup::ui::ImGuiContext ctx;
    const auto colors = bimeup::ui::Theme::BimColors();
    bimeup::ui::Theme::Apply(colors);

    const ImGuiStyle& style = ImGui::GetStyle();
    EXPECT_TRUE(ApproxEq(style.Colors[ImGuiCol_WindowBg], colors.background));
    EXPECT_TRUE(ApproxEq(style.Colors[ImGuiCol_FrameBg], colors.panel));
    EXPECT_TRUE(ApproxEq(style.Colors[ImGuiCol_Header], colors.header));
    EXPECT_TRUE(ApproxEq(style.Colors[ImGuiCol_Text], colors.text));
    EXPECT_TRUE(ApproxEq(style.Colors[ImGuiCol_Button], colors.accent));
    EXPECT_TRUE(
        ApproxEq(style.Colors[ImGuiCol_ButtonHovered], colors.accentHover));
    EXPECT_TRUE(
        ApproxEq(style.Colors[ImGuiCol_ButtonActive], colors.accentActive));
    EXPECT_TRUE(ApproxEq(style.Colors[ImGuiCol_Border], colors.border));
}

TEST(ThemeTest, ApplySetsTightSpacingForDenseBimTooling) {
    bimeup::ui::ImGuiContext ctx;
    bimeup::ui::Theme::Apply();
    const ImGuiStyle& style = ImGui::GetStyle();
    // Professional BIM tooling uses sharp corners and tight padding.
    EXPECT_FLOAT_EQ(style.WindowRounding, 0.0f);
    EXPECT_FLOAT_EQ(style.FrameRounding, 2.0f);
    EXPECT_GE(style.FramePadding.x, 4.0f);
    EXPECT_GE(style.ItemSpacing.x, 4.0f);
    EXPECT_EQ(style.WindowBorderSize, 1.0f);
    EXPECT_EQ(style.FrameBorderSize, 0.0f);
}

}  // namespace
