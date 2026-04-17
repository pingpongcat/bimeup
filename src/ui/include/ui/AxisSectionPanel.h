#pragma once

#include <optional>

#include <glm/glm.hpp>

#include <ui/Panel.h>

namespace bimeup::scene {
class AxisSectionController;
enum class Axis : unsigned char;
enum class SectionMode : unsigned char;
}  // namespace bimeup::scene

namespace bimeup::ui {

// Identity matrix with translation = offset * axis_unit_vector. Input to
// ImGuizmo::Manipulate for the axis-locked translate gizmo.
glm::mat4 MakeAxisGizmoTransform(scene::Axis axis, float offset);

// Read back the translation component of the gizmo transform along `axis`.
// Ignores translations on the other two axes so SetSlotOffset never absorbs
// stray motion.
float ExtractAxisOffset(const glm::mat4& transform, scene::Axis axis);

// Project the axis-plane origin (axis_unit * offset) to ImGui pixel coords.
// Returns nullopt if the point is behind the camera (clip.w <= 1e-4). Flips Y
// to match ImGui's top-left screen space. Used both for the direction marker
// and for anchoring the in-viewport mode-selector popup (8.3f).
std::optional<glm::vec2> ProjectPlaneOriginToScreen(
    const glm::mat4& view, const glm::mat4& projection,
    scene::Axis axis, float offset, const glm::vec2& displaySize);

/// ImGui panel for axis-locked section planes (8.3). Owns a pointer to an
/// external `scene::AxisSectionController` and exposes X/Y/Z toggles, per-axis
/// mode radios, and an offset slider. The `Toggle*`/`SetSlot*` methods carry
/// the state-change logic so they can be driven from tests without ImGui.
class AxisSectionPanel : public Panel {
public:
    AxisSectionPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetController(scene::AxisSectionController* controller);
    [[nodiscard]] scene::AxisSectionController* GetController() const {
        return m_controller;
    }

    void SetCameraMatrices(const glm::mat4& view, const glm::mat4& projection) {
        m_view = view;
        m_projection = projection;
    }
    [[nodiscard]] const glm::mat4& GetViewMatrix() const { return m_view; }
    [[nodiscard]] const glm::mat4& GetProjectionMatrix() const { return m_projection; }

    void SetActiveAxis(std::optional<scene::Axis> axis) { m_activeAxis = axis; }
    [[nodiscard]] std::optional<scene::Axis> ActiveAxis() const { return m_activeAxis; }

    // World-space offset slider range. Defaults to [-10, 10]; main.cpp can seed
    // from the scene AABB on model load.
    void SetOffsetRange(float minVal, float maxVal);
    [[nodiscard]] float OffsetMin() const { return m_offsetMin; }
    [[nodiscard]] float OffsetMax() const { return m_offsetMax; }

    // State-change hooks, also used by the ImGui widgets in OnDraw.
    void ToggleAxis(scene::Axis axis);
    void SetSlotMode(scene::Axis axis, scene::SectionMode mode);
    void SetSlotOffset(scene::Axis axis, float offset);

    // Clear the active-axis selection if the controller no longer has a slot
    // for it. Mirrors ClipPlanesPanel::PruneActiveIfMissing.
    void PruneActiveIfMissing();

private:
    // 8.3f stretch: ImGui popup anchored at the plane-origin projected screen
    // coords. Opens on right-click (BeginPopupContextItem) and writes the
    // selected mode back via SetSlotMode.
    void DrawModeContextPopup(scene::Axis axis);
    scene::AxisSectionController* m_controller = nullptr;
    glm::mat4 m_view{1.0F};
    glm::mat4 m_projection{1.0F};
    std::optional<scene::Axis> m_activeAxis;
    float m_offsetMin = -10.0F;
    float m_offsetMax = 10.0F;
};

}  // namespace bimeup::ui
