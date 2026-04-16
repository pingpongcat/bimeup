#include <ui/TypeVisibilityPanel.h>

#include <scene/Scene.h>

#include <imgui.h>

namespace bimeup::ui {

const char* TypeVisibilityPanel::GetName() const {
    return "Types";
}

void TypeVisibilityPanel::SetScene(scene::Scene* scene) {
    m_scene = scene;
    Refresh();
}

void TypeVisibilityPanel::Refresh() {
    if (m_scene == nullptr) {
        m_types.clear();
        m_visible.clear();
        return;
    }
    m_types = m_scene->GetUniqueTypes();
    // Seed visibility flags for newly-appearing types (default visible).
    // Preserve flags for types that were already tracked.
    std::unordered_map<std::string, bool> refreshed;
    refreshed.reserve(m_types.size());
    for (const auto& t : m_types) {
        auto it = m_visible.find(t);
        refreshed[t] = (it != m_visible.end()) ? it->second : true;
    }
    m_visible = std::move(refreshed);
}

void TypeVisibilityPanel::ApplyDefaults() {
    if (m_scene == nullptr) return;
    for (const auto& t : scene::DefaultHiddenTypes()) {
        auto it = m_visible.find(t);
        if (it == m_visible.end()) continue;  // type not present in this scene
        SetTypeVisible(t, false);
    }
}

bool TypeVisibilityPanel::IsTypeVisible(const std::string& ifcType) const {
    auto it = m_visible.find(ifcType);
    return it == m_visible.end() ? true : it->second;
}

void TypeVisibilityPanel::SetTypeVisible(const std::string& ifcType, bool visible) {
    m_visible[ifcType] = visible;
    if (m_scene != nullptr) {
        m_scene->SetVisibilityByType(ifcType, visible);
    }
}

void TypeVisibilityPanel::ReapplyToScene() {
    if (m_scene == nullptr) return;
    for (const auto& [ifcType, visible] : m_visible) {
        m_scene->SetVisibilityByType(ifcType, visible);
    }
}

void TypeVisibilityPanel::OnDraw() {
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }
    if (m_scene == nullptr || m_types.empty()) {
        ImGui::TextUnformatted("No model loaded");
        ImGui::End();
        return;
    }

    if (ImGui::SmallButton("Show all")) {
        for (const auto& t : m_types) SetTypeVisible(t, true);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Hide all")) {
        for (const auto& t : m_types) SetTypeVisible(t, false);
    }
    ImGui::Separator();

    for (const auto& t : m_types) {
        ImGui::PushID(t.c_str());
        bool v = IsTypeVisible(t);
        if (ImGui::Checkbox(t.c_str(), &v)) {
            SetTypeVisible(t, v);
        }
        ImGui::PopID();
    }
    ImGui::End();
}

}  // namespace bimeup::ui
