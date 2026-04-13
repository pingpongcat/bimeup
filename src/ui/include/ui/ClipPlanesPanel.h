#pragma once

#include <glm/glm.hpp>

#include <ui/Panel.h>

#include <cstdint>
#include <optional>

namespace bimeup::renderer {
class ClipPlaneManager;
}

namespace bimeup::ui {

enum class AxisPreset {
    XPositive,
    XNegative,
    YPositive,
    YNegative,
    ZPositive,
    ZNegative,
};

enum class GizmoMode : std::uint8_t { Translate, Rotate };

glm::vec4 MakeAxisPlaneEquation(AxisPreset preset);

class ClipPlanesPanel : public Panel {
public:
    ClipPlanesPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetManager(renderer::ClipPlaneManager* manager) { m_manager = manager; }
    [[nodiscard]] renderer::ClipPlaneManager* GetManager() const { return m_manager; }

    void SetCameraMatrices(const glm::mat4& view, const glm::mat4& projection) {
        m_view = view;
        m_projection = projection;
    }

    [[nodiscard]] std::optional<std::uint32_t> ActivePlaneId() const { return m_activePlaneId; }
    void SetActivePlaneId(std::optional<std::uint32_t> id) { m_activePlaneId = id; }
    [[nodiscard]] GizmoMode ActiveGizmoMode() const { return m_gizmoMode; }

private:
    renderer::ClipPlaneManager* m_manager = nullptr;
    glm::mat4 m_view{1.0F};
    glm::mat4 m_projection{1.0F};
    std::optional<std::uint32_t> m_activePlaneId;
    GizmoMode m_gizmoMode{GizmoMode::Translate};
};

}  // namespace bimeup::ui
