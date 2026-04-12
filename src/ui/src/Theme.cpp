#include <ui/Theme.h>

namespace bimeup::ui {

namespace {

constexpr ImVec4 RGB(int r, int g, int b, float a = 1.0f) {
    return ImVec4(static_cast<float>(r) / 255.0f,
                  static_cast<float>(g) / 255.0f,
                  static_cast<float>(b) / 255.0f, a);
}

}  // namespace

ThemeColors Theme::BimColors() {
    ThemeColors c{};
    c.background = RGB(30, 33, 38);
    c.panel = RGB(44, 48, 54);
    c.header = RGB(55, 60, 68);
    c.text = RGB(232, 236, 241);
    c.textDisabled = RGB(130, 138, 148);
    c.accent = RGB(0, 122, 204);
    c.accentHover = RGB(38, 152, 228);
    c.accentActive = RGB(0, 92, 160);
    c.border = RGB(18, 20, 24);
    return c;
}

void Theme::Apply() { Apply(BimColors()); }

void Theme::Apply(const ThemeColors& c) {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding = 0.0f;
    s.ChildRounding = 0.0f;
    s.PopupRounding = 2.0f;
    s.FrameRounding = 2.0f;
    s.GrabRounding = 2.0f;
    s.TabRounding = 2.0f;
    s.ScrollbarRounding = 2.0f;

    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.TabBorderSize = 0.0f;

    s.WindowPadding = ImVec2(8, 8);
    s.FramePadding = ImVec2(6, 4);
    s.ItemSpacing = ImVec2(6, 4);
    s.ItemInnerSpacing = ImVec2(4, 4);
    s.IndentSpacing = 18.0f;
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 10.0f;

    ImVec4* col = s.Colors;
    col[ImGuiCol_Text] = c.text;
    col[ImGuiCol_TextDisabled] = c.textDisabled;
    col[ImGuiCol_WindowBg] = c.background;
    col[ImGuiCol_ChildBg] = c.background;
    col[ImGuiCol_PopupBg] = c.panel;
    col[ImGuiCol_Border] = c.border;
    col[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    col[ImGuiCol_FrameBg] = c.panel;
    col[ImGuiCol_FrameBgHovered] =
        ImVec4(c.panel.x * 1.2f, c.panel.y * 1.2f, c.panel.z * 1.2f, 1.0f);
    col[ImGuiCol_FrameBgActive] = c.header;

    col[ImGuiCol_TitleBg] = c.header;
    col[ImGuiCol_TitleBgActive] = c.header;
    col[ImGuiCol_TitleBgCollapsed] = c.background;

    col[ImGuiCol_MenuBarBg] = c.header;
    col[ImGuiCol_ScrollbarBg] = c.background;
    col[ImGuiCol_ScrollbarGrab] = c.panel;
    col[ImGuiCol_ScrollbarGrabHovered] = c.header;
    col[ImGuiCol_ScrollbarGrabActive] = c.accent;

    col[ImGuiCol_CheckMark] = c.accent;
    col[ImGuiCol_SliderGrab] = c.accent;
    col[ImGuiCol_SliderGrabActive] = c.accentActive;

    col[ImGuiCol_Button] = c.accent;
    col[ImGuiCol_ButtonHovered] = c.accentHover;
    col[ImGuiCol_ButtonActive] = c.accentActive;

    col[ImGuiCol_Header] = c.header;
    col[ImGuiCol_HeaderHovered] = c.accentHover;
    col[ImGuiCol_HeaderActive] = c.accent;

    col[ImGuiCol_Separator] = c.border;
    col[ImGuiCol_SeparatorHovered] = c.accent;
    col[ImGuiCol_SeparatorActive] = c.accentActive;

    col[ImGuiCol_ResizeGrip] = c.panel;
    col[ImGuiCol_ResizeGripHovered] = c.accentHover;
    col[ImGuiCol_ResizeGripActive] = c.accentActive;

    col[ImGuiCol_Tab] = c.header;
    col[ImGuiCol_TabHovered] = c.accentHover;
    col[ImGuiCol_TabSelected] = c.accent;
    col[ImGuiCol_TabDimmed] = c.panel;
    col[ImGuiCol_TabDimmedSelected] = c.header;

    col[ImGuiCol_DockingPreview] =
        ImVec4(c.accent.x, c.accent.y, c.accent.z, 0.5f);
    col[ImGuiCol_DockingEmptyBg] = c.background;

    col[ImGuiCol_PlotLines] = c.accent;
    col[ImGuiCol_PlotLinesHovered] = c.accentHover;
    col[ImGuiCol_PlotHistogram] = c.accent;
    col[ImGuiCol_PlotHistogramHovered] = c.accentHover;

    col[ImGuiCol_TableHeaderBg] = c.header;
    col[ImGuiCol_TableBorderStrong] = c.border;
    col[ImGuiCol_TableBorderLight] = c.border;
    col[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_TableRowBgAlt] =
        ImVec4(c.panel.x, c.panel.y, c.panel.z, 0.35f);

    col[ImGuiCol_TextSelectedBg] =
        ImVec4(c.accent.x, c.accent.y, c.accent.z, 0.45f);
    col[ImGuiCol_DragDropTarget] = c.accentHover;
    col[ImGuiCol_NavCursor] = c.accentHover;
    col[ImGuiCol_NavWindowingHighlight] =
        ImVec4(c.accent.x, c.accent.y, c.accent.z, 0.7f);
    col[ImGuiCol_NavWindowingDimBg] = ImVec4(0, 0, 0, 0.35f);
    col[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
}

}  // namespace bimeup::ui
