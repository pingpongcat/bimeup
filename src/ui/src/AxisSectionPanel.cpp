#include <ui/AxisSectionPanel.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <scene/AxisSectionController.h>

#include <array>
#include <cmath>
#include <cstddef>

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

std::size_t AxisIndex(scene::Axis axis) {
    return static_cast<std::size_t>(axis);
}

ImGuizmo::OPERATION AxisTranslateOp(scene::Axis axis) {
    switch (axis) {
        case scene::Axis::X: return ImGuizmo::TRANSLATE_X;
        case scene::Axis::Y: return ImGuizmo::TRANSLATE_Y;
        case scene::Axis::Z: return ImGuizmo::TRANSLATE_Z;
    }
    return ImGuizmo::TRANSLATE_X;
}

glm::vec3 AxisUnit(scene::Axis axis) {
    switch (axis) {
        case scene::Axis::X: return {1.0F, 0.0F, 0.0F};
        case scene::Axis::Y: return {0.0F, 1.0F, 0.0F};
        case scene::Axis::Z: return {0.0F, 0.0F, 1.0F};
    }
    return {1.0F, 0.0F, 0.0F};
}

// +1 when the kept side is +axis, -1 when it is -axis.
float KeptSign(scene::SectionMode mode) {
    // CutBack flips the normal to -axis (see AxisSectionController::MakeAxisEquation).
    // CutFront and SectionOnly both keep the +axis side.
    return (mode == scene::SectionMode::CutBack) ? -1.0F : 1.0F;
}

// GL-convention projection: ImGuizmo matrices have Y up. ImGui screen space is
// Y down with origin top-left, so flip Y after the NDC divide.
bool ProjectToScreen(const glm::mat4& view, const glm::mat4& projection,
                     const glm::vec3& worldPoint, const ImVec2& displaySize,
                     ImVec2& outScreen) {
    const glm::vec4 clip = projection * view * glm::vec4(worldPoint, 1.0F);
    if (clip.w <= 1e-4F) return false;
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    outScreen = ImVec2((ndcX * 0.5F + 0.5F) * displaySize.x,
                       (0.5F - ndcY * 0.5F) * displaySize.y);
    return true;
}

void DrawDirectionMarker(const glm::mat4& view, const glm::mat4& projection,
                         scene::Axis axis, const scene::AxisSectionSlot& slot,
                         const ImVec2& displaySize) {
    const glm::vec3 unit = AxisUnit(axis);
    const glm::vec3 origin = unit * slot.offset;
    const float sign = KeptSign(slot.mode);
    // Second world-space point 1m into the "kept" side — screen projection of
    // origin→kept gives the arrow direction in pixels.
    const glm::vec3 tip = origin + unit * sign;

    ImVec2 originPx;
    ImVec2 tipPx;
    if (!ProjectToScreen(view, projection, origin, displaySize, originPx)) return;
    if (!ProjectToScreen(view, projection, tip, displaySize, tipPx)) return;

    const ImVec2 d{tipPx.x - originPx.x, tipPx.y - originPx.y};
    const float lenSq = d.x * d.x + d.y * d.y;
    if (lenSq < 1.0F) return;  // axis is almost edge-on; hide the marker
    const float len = std::sqrt(lenSq);
    const ImVec2 dir{d.x / len, d.y / len};

    // Axis-mode-independent visual length in screen pixels.
    constexpr float kArrowPx = 42.0F;
    const ImVec2 head{originPx.x + dir.x * kArrowPx, originPx.y + dir.y * kArrowPx};

    // Perpendicular for the arrowhead wings.
    const ImVec2 perp{-dir.y, dir.x};
    constexpr float kWing = 8.0F;
    const ImVec2 wingA{head.x - dir.x * kWing + perp.x * kWing,
                       head.y - dir.y * kWing + perp.y * kWing};
    const ImVec2 wingB{head.x - dir.x * kWing - perp.x * kWing,
                       head.y - dir.y * kWing - perp.y * kWing};

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 col = (slot.mode == scene::SectionMode::SectionOnly)
                          ? IM_COL32(255, 200, 64, 255)   // amber
                          : IM_COL32(80, 220, 120, 255);  // green
    constexpr float kThickness = 2.5F;
    dl->AddLine(originPx, head, col, kThickness);
    dl->AddLine(head, wingA, col, kThickness);
    dl->AddLine(head, wingB, col, kThickness);
    dl->AddCircleFilled(originPx, 3.5F, col);
}

}  // namespace

glm::mat4 MakeAxisGizmoTransform(scene::Axis axis, float offset) {
    glm::mat4 m{1.0F};
    m[3][AxisIndex(axis)] = offset;
    return m;
}

float ExtractAxisOffset(const glm::mat4& transform, scene::Axis axis) {
    return transform[3][AxisIndex(axis)];
}

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

    // Direction markers + translate gizmo drawn outside the panel window so
    // they appear over the scene viewport.
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 displaySize = io.DisplaySize;

    for (const auto& btn : kAxisButtons) {
        if (!m_controller->HasSlot(btn.axis)) continue;
        DrawDirectionMarker(m_view, m_projection, btn.axis,
                            *m_controller->GetSlot(btn.axis), displaySize);
    }

    if (m_activeAxis.has_value() && m_controller->HasSlot(*m_activeAxis)) {
        const scene::Axis axis = *m_activeAxis;
        const auto slot = *m_controller->GetSlot(axis);

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetRect(0.0F, 0.0F, displaySize.x, displaySize.y);

        glm::mat4 xform = MakeAxisGizmoTransform(axis, slot.offset);
        const bool used = ImGuizmo::Manipulate(&m_view[0][0],
                                               &m_projection[0][0],
                                               AxisTranslateOp(axis),
                                               ImGuizmo::WORLD,
                                               &xform[0][0]);
        if (used) {
            SetSlotOffset(axis, ExtractAxisOffset(xform, axis));
        }
    }
}

}  // namespace bimeup::ui
