#include <ui/ClipPlanesPanel.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>

#include <array>
#include <cstdio>
#include <optional>

namespace bimeup::ui {

glm::vec4 MakeAxisPlaneEquation(AxisPreset preset) {
    switch (preset) {
        case AxisPreset::XPositive: return {1.0F, 0.0F, 0.0F, 0.0F};
        case AxisPreset::XNegative: return {-1.0F, 0.0F, 0.0F, 0.0F};
        case AxisPreset::YPositive: return {0.0F, 1.0F, 0.0F, 0.0F};
        case AxisPreset::YNegative: return {0.0F, -1.0F, 0.0F, 0.0F};
        case AxisPreset::ZPositive: return {0.0F, 0.0F, 1.0F, 0.0F};
        case AxisPreset::ZNegative: return {0.0F, 0.0F, -1.0F, 0.0F};
    }
    return {0.0F, 1.0F, 0.0F, 0.0F};
}

const char* ClipPlanesPanel::GetName() const {
    return "Clipping Planes";
}

void ClipPlanesPanel::PruneActiveIfMissing() {
    if (!m_activePlaneId.has_value() || m_manager == nullptr) return;
    if (!m_manager->Contains(*m_activePlaneId)) m_activePlaneId.reset();
}

void ClipPlanesPanel::OnDraw() {
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }

    if (m_manager == nullptr) {
        ImGui::TextDisabled("Clip plane manager not wired.");
        ImGui::End();
        return;
    }

    auto& mgr = *m_manager;
    const std::size_t count = mgr.Count();
    const bool atCapacity = count >= renderer::ClipPlaneManager::kMaxPlanes;

    ImGui::Text("Planes: %zu / %zu", count, renderer::ClipPlaneManager::kMaxPlanes);

    ImGui::TextUnformatted("Gizmo:");
    ImGui::SameLine();
    int mode = static_cast<int>(m_gizmoMode);
    ImGui::RadioButton("Translate", &mode, static_cast<int>(GizmoMode::Translate));
    ImGui::SameLine();
    ImGui::RadioButton("Rotate", &mode, static_cast<int>(GizmoMode::Rotate));
    m_gizmoMode = static_cast<GizmoMode>(mode);

    ImGui::Separator();

    struct PresetButton {
        const char* label;
        AxisPreset preset;
    };
    static constexpr std::array<PresetButton, 6> kPresets{{
        {"+X", AxisPreset::XPositive},
        {"-X", AxisPreset::XNegative},
        {"+Y", AxisPreset::YPositive},
        {"-Y", AxisPreset::YNegative},
        {"+Z", AxisPreset::ZPositive},
        {"-Z", AxisPreset::ZNegative},
    }};

    ImGui::TextUnformatted("Add plane");
    ImGui::SameLine();
    ImGui::BeginDisabled(atCapacity);
    for (std::size_t i = 0; i < kPresets.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        if (ImGui::SmallButton(kPresets[i].label)) {
            const std::uint32_t newId = mgr.AddPlane(MakeAxisPlaneEquation(kPresets[i].preset));
            m_activePlaneId = newId;
        }
    }
    ImGui::EndDisabled();
    if (atCapacity) {
        ImGui::TextDisabled("Maximum plane count reached.");
    }
    ImGui::Separator();

    if (count == 0) {
        ImGui::TextDisabled("No clipping planes. Use the buttons above to add one.");
        ImGui::End();
        return;
    }

    std::optional<std::uint32_t> toRemove;

    const auto planesCopy = mgr.Planes();  // copy so edits don't invalidate iteration
    for (const auto& entry : planesCopy) {
        ImGui::PushID(static_cast<int>(entry.id));

        char header[32];
        std::snprintf(header, sizeof(header), "Plane %u", entry.id);
        if (ImGui::CollapsingHeader(header)) {
            bool active = (m_activePlaneId == entry.id);
            if (ImGui::Checkbox("Active (gizmo)", &active)) {
                m_activePlaneId = active ? std::optional<std::uint32_t>{entry.id} : std::nullopt;
            }
            bool enabled = entry.plane.enabled;
            if (ImGui::Checkbox("Enabled", &enabled)) {
                mgr.SetEnabled(entry.id, enabled);
            }

            glm::vec4 eq = entry.plane.equation;
            std::array<float, 3> normal{eq.x, eq.y, eq.z};
            bool changed = false;
            if (ImGui::SliderFloat3("Normal", normal.data(), -1.0F, 1.0F)) {
                eq.x = normal[0];
                eq.y = normal[1];
                eq.z = normal[2];
                changed = true;
            }
            if (ImGui::SliderFloat("Offset (d)", &eq.w, -100.0F, 100.0F)) {
                changed = true;
            }
            if (changed) {
                mgr.UpdatePlane(entry.id, eq);
            }

            if (ImGui::SmallButton("Remove")) {
                toRemove = entry.id;
            }
        }

        ImGui::PopID();
    }

    if (toRemove.has_value()) {
        if (m_activePlaneId == *toRemove) m_activePlaneId.reset();
        mgr.RemovePlane(*toRemove);
    }
    PruneActiveIfMissing();

    ImGui::End();

    // Draw the ImGuizmo overlay for the active plane (if any). This is drawn
    // outside of the panel window so it appears over the scene viewport.
    if (m_activePlaneId.has_value()) {
        const renderer::ClipPlane* current = mgr.Find(*m_activePlaneId);
        if (current != nullptr) {
            const ImGuiIO& io = ImGui::GetIO();
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
            ImGuizmo::SetRect(0.0F, 0.0F, io.DisplaySize.x, io.DisplaySize.y);

            glm::mat4 xform = renderer::PlaneToTransform(*current);

            // PlaneToTransform aligns local +Z with the plane normal. Using LOCAL
            // mode makes TRANSLATE_Z draw the arrow along that normal regardless
            // of which world axis it points to.
            const ImGuizmo::OPERATION op = (m_gizmoMode == GizmoMode::Translate)
                                               ? ImGuizmo::TRANSLATE_Z
                                               : ImGuizmo::ROTATE;
            const bool used = ImGuizmo::Manipulate(&m_view[0][0],
                                                   &m_projection[0][0],
                                                   op,
                                                   ImGuizmo::LOCAL,
                                                   &xform[0][0]);
            if (used) {
                const renderer::ClipPlane updated = renderer::TransformToPlane(xform);
                glm::vec4 eq = updated.equation;
                if (m_gizmoMode == GizmoMode::Translate) {
                    // Translation-only drags should not change the normal; re-project
                    // so numerical drift stays out of the equation.
                    eq.x = current->equation.x;
                    eq.y = current->equation.y;
                    eq.z = current->equation.z;
                }
                mgr.UpdatePlane(*m_activePlaneId, eq);
            }
        } else {
            m_activePlaneId.reset();
        }
    }
}

}  // namespace bimeup::ui
