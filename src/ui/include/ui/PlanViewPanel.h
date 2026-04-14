#pragma once

#include <glm/glm.hpp>

#include <ui/Panel.h>

#include <cstdint>
#include <string>
#include <vector>

namespace bimeup::renderer {
class Camera;
class ClipPlaneManager;
}  // namespace bimeup::renderer

namespace bimeup::ui {

struct PlanLevel {
    std::string name;
    float elevation;  // world-space Y of the floor
};

// Clip plane equation that keeps geometry at or below (elevation + cutAbove).
// Matches renderer::ClipPlane convention: a point p is kept when
// dot(eq.xyz, p) + eq.w >= 0. Normal is -Y, d = elevation + cutAbove.
glm::vec4 ComputePlanClipEquation(float elevation, float cutAbove);

class PlanViewPanel : public Panel {
public:
    PlanViewPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetClipPlaneManager(renderer::ClipPlaneManager* manager) { m_clipManager = manager; }
    void SetCamera(renderer::Camera* camera) { m_camera = camera; }

    void SetLevels(std::vector<PlanLevel> levels);
    [[nodiscard]] const std::vector<PlanLevel>& Levels() const { return m_levels; }

    void SetSceneBounds(const glm::vec3& min, const glm::vec3& max);

    void SetViewportAspect(float aspect) { m_viewportAspect = aspect; }

    void SetCutAbove(float cutAbove) { m_cutAbove = cutAbove; }
    [[nodiscard]] float CutAbove() const { return m_cutAbove; }

    [[nodiscard]] int ActiveLevel() const { return m_activeLevel; }

    // Apply a plan view for the given level index. Creates / updates a single
    // managed clip plane and, if a camera is attached, switches to top-down
    // orthographic framed to the scene bounds. Returns false on bad index.
    bool ActivateLevel(int index);

    // Remove the managed clip plane and reset active index to -1. Leaves
    // camera as-is so the user can keep orbiting.
    void Deactivate();

private:
    void ApplyCameraForPlan();

    renderer::ClipPlaneManager* m_clipManager = nullptr;
    renderer::Camera* m_camera = nullptr;
    std::vector<PlanLevel> m_levels;
    glm::vec3 m_sceneMin{-1.0F};
    glm::vec3 m_sceneMax{1.0F};
    bool m_hasBounds = false;
    float m_cutAbove = 1.2F;
    float m_viewportAspect = 1.0F;
    int m_activeLevel = -1;
    std::uint32_t m_clipPlaneId = 0;  // 0 == ClipPlaneManager::kInvalidId
};

}  // namespace bimeup::ui
