#pragma once

#include <glm/glm.hpp>

#include <ui/Panel.h>

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

glm::vec4 MakeAxisPlaneEquation(AxisPreset preset);

class ClipPlanesPanel : public Panel {
public:
    ClipPlanesPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetManager(renderer::ClipPlaneManager* manager) { m_manager = manager; }
    [[nodiscard]] renderer::ClipPlaneManager* GetManager() const { return m_manager; }

private:
    renderer::ClipPlaneManager* m_manager = nullptr;
};

}  // namespace bimeup::ui
