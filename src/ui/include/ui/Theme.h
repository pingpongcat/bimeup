#pragma once

#include <imgui.h>

namespace bimeup::ui {

struct ThemeColors {
    ImVec4 background;
    ImVec4 panel;
    ImVec4 header;
    ImVec4 text;
    ImVec4 textDisabled;
    ImVec4 accent;
    ImVec4 accentHover;
    ImVec4 accentActive;
    ImVec4 border;
};

class Theme {
public:
    static ThemeColors BimColors();
    static void Apply();
    static void Apply(const ThemeColors& colors);
};

}  // namespace bimeup::ui
