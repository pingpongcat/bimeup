#include <ui/AxisSectionPanel.h>

#include <imgui.h>
#include <scene/AxisSectionController.h>
#include <ui/AxisSectionGizmo.hpp>

#include <array>
#include <cstddef>

namespace bimeup::ui {

namespace {

struct AxisButton {
    const char* label;
    scene::Axis axis;
};

// BIM convention: Z is vertical (height), Y is depth. The viewer's world is
// Y-up, so the UI "Z" label maps to the world Y axis and "Y" maps to world Z.
constexpr std::array<AxisButton, 3> kAxisButtons{{
    {"X", scene::Axis::X},
    {"Y", scene::Axis::Z},
    {"Z", scene::Axis::Y},
}};

}  // namespace

const char* AxisSectionPanel::GetName() const {
    return "Axis Section";
}

void AxisSectionPanel::SetController(scene::AxisSectionController* controller) {
    m_controller = controller;
}

void AxisSectionPanel::SetOffsetRange(float minVal, float maxVal) {
    m_offsetMin = minVal;
    m_offsetMax = maxVal;
}

void AxisSectionPanel::ToggleAxis(scene::Axis axis) {
    if (m_controller == nullptr) return;
    if (m_controller->HasSlot(axis)) {
        m_controller->ClearSlot(axis);
    } else {
        m_controller->SetSlot(axis, {0.0F, scene::SectionMode::CutFront});
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
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
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

        float offset = slot.offset;
        char label[32];
        std::snprintf(label, sizeof(label), "%s offset", btn.label);
        if (ImGui::SliderFloat(label, &offset, m_offsetMin, m_offsetMax, "%.3f")) {
            SetSlotOffset(btn.axis, offset);
        }
        ImGui::PopID();
    }

    if (!anyActive) {
        ImGui::TextDisabled("No axis plane. Click X, Y, or Z to add one.");
    }

    ImGui::End();

    // In-viewport gizmo, one per active slot. Drag the grab to change offset;
    // B/F/C pills select mode; (×) removes the slot.
    const ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 displaySize{io.DisplaySize.x, io.DisplaySize.y};

    for (const auto& btn : kAxisButtons) {
        if (!m_controller->HasSlot(btn.axis)) continue;
        auto slot = *m_controller->GetSlot(btn.axis);
        float offset = slot.offset;
        scene::SectionMode mode = slot.mode;
        bool open = true;
        const bool changed = DrawAxisHandle(btn.axis, offset, mode, open,
                                            m_view, m_projection, displaySize);
        if (changed) {
            if (!open) {
                m_controller->ClearSlot(btn.axis);
            } else if (offset != slot.offset || mode != slot.mode) {
                m_controller->SetSlot(btn.axis, {offset, mode});
            }
        }
    }
}

}  // namespace bimeup::ui
