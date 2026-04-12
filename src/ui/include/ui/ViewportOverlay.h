#pragma once

#include <ui/Panel.h>

#include <glm/vec3.hpp>

namespace bimeup::ui {

class ViewportOverlay : public Panel {
public:
    ViewportOverlay() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetFps(float fps);
    [[nodiscard]] float GetFps() const;

    void SetCameraPosition(glm::vec3 position);
    [[nodiscard]] glm::vec3 GetCameraPosition() const;

    void SetCameraForward(glm::vec3 forward);
    [[nodiscard]] glm::vec3 GetCameraForward() const;

    void SetFpsCounterVisible(bool visible);
    [[nodiscard]] bool IsFpsCounterVisible() const;

    void SetCameraInfoVisible(bool visible);
    [[nodiscard]] bool IsCameraInfoVisible() const;

    void SetAxesGizmoVisible(bool visible);
    [[nodiscard]] bool IsAxesGizmoVisible() const;

private:
    float m_fps = 0.0F;
    glm::vec3 m_cameraPosition{0.0F};
    glm::vec3 m_cameraForward{0.0F, 0.0F, -1.0F};
    bool m_fpsCounterVisible = true;
    bool m_cameraInfoVisible = true;
    bool m_axesGizmoVisible = true;
};

}  // namespace bimeup::ui
