#pragma once

#include <glm/glm.hpp>

#include <ui/Panel.h>

namespace bimeup::scene {
class AxisSectionController;
enum class Axis : unsigned char;
enum class SectionMode : unsigned char;
}  // namespace bimeup::scene

namespace bimeup::ui {

/// ImGui panel for axis-locked section planes (8.3). Exposes X/Y/Z toggle
/// buttons + a precision offset slider per active slot. Drag, mode switch,
/// and close are handled by the in-viewport `DrawAxisHandle` gizmo.
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

    void SetOffsetRange(float minVal, float maxVal);
    [[nodiscard]] float OffsetMin() const { return m_offsetMin; }
    [[nodiscard]] float OffsetMax() const { return m_offsetMax; }

    void ToggleAxis(scene::Axis axis);
    void SetSlotMode(scene::Axis axis, scene::SectionMode mode);
    void SetSlotOffset(scene::Axis axis, float offset);

private:
    scene::AxisSectionController* m_controller = nullptr;
    glm::mat4 m_view{1.0F};
    glm::mat4 m_projection{1.0F};
    float m_offsetMin = -10.0F;
    float m_offsetMax = 10.0F;
};

}  // namespace bimeup::ui
