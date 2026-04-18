#pragma once

#include <ui/Panel.h>

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace bimeup::scene {
class MeasureTool;
}

namespace bimeup::ui {

class ViewportOverlay : public Panel {
public:
    ViewportOverlay() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    /// Provide measurement state to draw on top of the viewport. Pass nullptr to disable.
    void SetMeasurement(const scene::MeasureTool* tool,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        glm::vec2 framebufferSize);

    /// Snap candidate at the cursor: hover-time world position + whether it
    /// snapped to a vertex (true) or just landed on a face (false).
    /// Pass nullopt to clear (e.g. when measure mode is off or cursor is off-model).
    void SetSnapCandidate(std::optional<glm::vec3> world, bool isVertex);

private:
    const scene::MeasureTool* m_measureTool = nullptr;
    glm::mat4 m_measureView{1.0F};
    glm::mat4 m_measureProj{1.0F};
    glm::vec2 m_measureFbSize{0.0F};

    std::optional<glm::vec3> m_snapCandidate;
    bool m_snapIsVertex = false;
};

}  // namespace bimeup::ui
