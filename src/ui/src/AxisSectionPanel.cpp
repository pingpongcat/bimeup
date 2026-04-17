#include <ui/AxisSectionPanel.h>

#include <imgui.h>
#include <scene/AxisSectionController.h>

#include <array>

namespace bimeup::ui {

namespace {

struct AxisButton {
    const char* label;
    scene::Axis axis;
};

constexpr std::array<AxisButton, 3> kAxisButtons{{
    {"X", scene::Axis::X},
    {"Y", scene::Axis::Y},
    {"Z", scene::Axis::Z},
}};

constexpr std::array<std::pair<const char*, scene::SectionMode>, 3> kModeButtons{{
    {"Cut Front", scene::SectionMode::CutFront},
    {"Cut Back", scene::SectionMode::CutBack},
    {"Section", scene::SectionMode::SectionOnly},
}};

}  // namespace

const char* AxisSectionPanel::GetName() const {
    return "Axis Section";
}

void AxisSectionPanel::SetController(scene::AxisSectionController* controller) {
    if (m_controller != controller) {
        m_controller = controller;
        m_activeAxis.reset();
    }
}

void AxisSectionPanel::SetOffsetRange(float minVal, float maxVal) {
    m_offsetMin = minVal;
    m_offsetMax = maxVal;
}

void AxisSectionPanel::ToggleAxis(scene::Axis axis) {
    if (m_controller == nullptr) return;
    if (m_controller->HasSlot(axis)) {
        m_controller->ClearSlot(axis);
        if (m_activeAxis == axis) m_activeAxis.reset();
    } else {
        m_controller->SetSlot(axis, {0.0F, scene::SectionMode::CutFront});
        if (!m_activeAxis.has_value()) m_activeAxis = axis;
    }
}

void AxisSectionPanel::SetSlotMode(scene::Axis axis, scene::SectionMode mode) {
    if (m_controller == nullptr || !m_controller->HasSlot(axis)) return;
    const auto slot = m_controller->GetSlot(axis);
    m_controller->SetSlot(axis, {slot->offset, mode});
}

void AxisSectionPanel::SetSlotOffset(scene::Axis axis, float offset) {
    if (m_controller == nullptr || !m_controller->HasSlot(axis)) return;
    const auto slot = m_controller->GetSlot(axis);
    m_controller->SetSlot(axis, {offset, slot->mode});
}

void AxisSectionPanel::PruneActiveIfMissing() {
    if (!m_activeAxis.has_value() || m_controller == nullptr) return;
    if (!m_controller->HasSlot(*m_activeAxis)) m_activeAxis.reset();
}

void AxisSectionPanel::OnDraw() {
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }

    if (m_controller == nullptr) {
        ImGui::TextDisabled("Axis section controller not wired.");
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Axis plane:");
    ImGui::SameLine();
    for (std::size_t i = 0; i < kAxisButtons.size(); ++i) {
        const auto& btn = kAxisButtons[i];
        const bool on = m_controller->HasSlot(btn.axis);

        if (on) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::SmallButton(btn.label)) {
            ToggleAxis(btn.axis);
        }
        if (on) ImGui::PopStyleColor();
        if (i + 1 < kAxisButtons.size()) ImGui::SameLine();
    }

    ImGui::Separator();

    bool anyActive = false;
    for (const auto& btn : kAxisButtons) {
        if (!m_controller->HasSlot(btn.axis)) continue;
        anyActive = true;

        const auto slot = *m_controller->GetSlot(btn.axis);
        ImGui::PushID(static_cast<int>(btn.axis));

        bool isActive = (m_activeAxis == btn.axis);
        if (ImGui::RadioButton(btn.label, isActive)) {
            m_activeAxis = btn.axis;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(gizmo)");

        int currentMode = static_cast<int>(slot.mode);
        for (std::size_t m = 0; m < kModeButtons.size(); ++m) {
            if (m > 0) ImGui::SameLine();
            ImGui::RadioButton(kModeButtons[m].first, &currentMode, static_cast<int>(kModeButtons[m].second));
        }
        if (currentMode != static_cast<int>(slot.mode)) {
            SetSlotMode(btn.axis, static_cast<scene::SectionMode>(currentMode));
        }

        float offset = slot.offset;
        if (ImGui::SliderFloat("Offset", &offset, m_offsetMin, m_offsetMax, "%.3f")) {
            SetSlotOffset(btn.axis, offset);
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    if (!anyActive) {
        ImGui::TextDisabled("No axis plane. Click X, Y, or Z to add one.");
    }

    PruneActiveIfMissing();

    ImGui::End();
}

}  // namespace bimeup::ui
