#include <ui/AxisSectionPanel.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <scene/AxisSectionController.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>

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
std::optional<ImVec2> ProjectPointToScreen(const glm::mat4& view,
                                           const glm::mat4& projection,
                                           const glm::vec3& worldPoint,
                                           const glm::vec2& displaySize) {
    const glm::vec4 clip = projection * view * glm::vec4(worldPoint, 1.0F);
    if (clip.w <= 1e-4F) return std::nullopt;
    const float ndcX = clip.x / clip.w;
    const float ndcY = clip.y / clip.w;
    return ImVec2((ndcX * 0.5F + 0.5F) * displaySize.x,
                  (0.5F - ndcY * 0.5F) * displaySize.y);
}

void DrawDirectionMarker(const glm::mat4& view, const glm::mat4& projection,
                         scene::Axis axis, const scene::AxisSectionSlot& slot,
                         const glm::vec2& displaySize) {
    const glm::vec3 unit = AxisUnit(axis);
    const glm::vec3 origin = unit * slot.offset;
    const float sign = KeptSign(slot.mode);
    // Second world-space point 1m into the "kept" side — screen projection of
    // origin→kept gives the arrow direction in pixels.
    const glm::vec3 tip = origin + unit * sign;

    const auto originOpt = ProjectPointToScreen(view, projection, origin, displaySize);
    const auto tipOpt = ProjectPointToScreen(view, projection, tip, displaySize);
    if (!originOpt.has_value() || !tipOpt.has_value()) return;
    const ImVec2 originPx = *originOpt;
    const ImVec2 tipPx = *tipOpt;

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

std::optional<glm::vec2> ProjectPlaneOriginToScreen(
    const glm::mat4& view, const glm::mat4& projection,
    scene::Axis axis, float offset, const glm::vec2& displaySize) {
    const glm::vec3 origin = AxisUnit(axis) * offset;
    const auto px = ProjectPointToScreen(view, projection, origin, displaySize);
    if (!px.has_value()) return std::nullopt;
    return glm::vec2{px->x, px->y};
}

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

void AxisSectionPanel::DrawModeContextPopup(scene::Axis axis) {
    if (m_controller == nullptr || !m_controller->HasSlot(axis)) return;

    const ImGuiIO& io = ImGui::GetIO();
    const glm::vec2 displaySize{io.DisplaySize.x, io.DisplaySize.y};
    const auto slot = *m_controller->GetSlot(axis);
    const auto originOpt = ProjectPlaneOriginToScreen(m_view, m_projection, axis,
                                                      slot.offset, displaySize);
    if (!originOpt.has_value()) return;

    // Tiny transparent host window centred on the marker origin. Right-click
    // inside opens the popup; the window body itself has no decoration and
    // stays behind other UI so it never steals focus.
    constexpr float kHitSize = 22.0F;
    ImGui::SetNextWindowPos(ImVec2(originOpt->x - kHitSize * 0.5F,
                                   originOpt->y - kHitSize * 0.5F),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kHitSize, kHitSize), ImGuiCond_Always);

    constexpr ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

    char wndId[32];
    std::snprintf(wndId, sizeof(wndId), "##axissec_hit_%d",
                  static_cast<int>(axis));

    if (ImGui::Begin(wndId, nullptr, hostFlags)) {
        ImGui::InvisibleButton("hit", ImVec2(kHitSize, kHitSize));
        if (ImGui::BeginPopupContextItem("mode_menu",
                                         ImGuiPopupFlags_MouseButtonRight)) {
            for (const auto& mb : kModeButtons) {
                const bool selected = (slot.mode == mb.second);
                if (ImGui::Selectable(mb.first, selected)) {
                    SetSlotMode(axis, mb.second);
                }
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
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
    const glm::vec2 displaySize{io.DisplaySize.x, io.DisplaySize.y};

    for (const auto& btn : kAxisButtons) {
        if (!m_controller->HasSlot(btn.axis)) continue;
        const auto slot = *m_controller->GetSlot(btn.axis);
        DrawDirectionMarker(m_view, m_projection, btn.axis, slot, displaySize);
        DrawModeContextPopup(btn.axis);
    }

    if (m_activeAxis.has_value() && m_controller->HasSlot(*m_activeAxis)) {
        const scene::Axis axis = *m_activeAxis;
        const auto slot = *m_controller->GetSlot(axis);

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
        ImGuizmo::SetRect(0.0F, 0.0F, io.DisplaySize.x, io.DisplaySize.y);

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
