#pragma once

#include <optional>

#include <ui/Panel.h>

namespace bimeup::scene {
class AxisSectionController;
enum class Axis : unsigned char;
enum class SectionMode : unsigned char;
}  // namespace bimeup::scene

namespace bimeup::ui {

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
    scene::AxisSectionController* m_controller = nullptr;
    std::optional<scene::Axis> m_activeAxis;
    float m_offsetMin = -10.0F;
    float m_offsetMax = 10.0F;
};

}  // namespace bimeup::ui
