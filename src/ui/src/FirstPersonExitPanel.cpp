#include <ui/FirstPersonExitPanel.h>

#include <imgui.h>

#include <utility>

namespace bimeup::ui {

FirstPersonExitPanel::FirstPersonExitPanel() {
    SetVisible(false);
}

const char* FirstPersonExitPanel::GetName() const {
    return "First Person";
}

void FirstPersonExitPanel::SetOnExit(ExitCallback callback) {
    m_onExit = std::move(callback);
}

void FirstPersonExitPanel::TriggerExit() {
    if (m_onExit) {
        m_onExit();
    }
}

void FirstPersonExitPanel::OnDraw() {
    // Pinned to the top-right corner of the viewport so it's immediately
    // discoverable while in first-person mode. Minimal chrome — the only
    // interactive element is the Exit button.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    constexpr float kMargin = 10.0F;
    const ImVec2 pivot(1.0F, 0.0F);
    const ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x - kMargin,
                     vp->WorkPos.y + kMargin);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);

    // NoFocusOnAppearing + NoBringToFrontOnFocus + NoNav keep the window
    // from grabbing keyboard focus — otherwise ImGui sets WantCaptureKeyboard
    // and the WASD/arrow polling in main.cpp is suppressed while walking.
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoNav;
    if (ImGui::Begin(GetName(), nullptr, kFlags)) {
        if (ImGui::Button("Exit Point of View")) {
            TriggerExit();
        }
    }
    ImGui::End();
}

}  // namespace bimeup::ui
