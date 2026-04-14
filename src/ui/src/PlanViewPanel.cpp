#include "ui/PlanViewPanel.h"

#include <imgui.h>

#include <renderer/Camera.h>
#include <renderer/ClipPlaneManager.h>

#include <utility>

namespace bimeup::ui {

glm::vec4 ComputePlanClipEquation(float elevation, float cutAbove) {
    return {0.0F, -1.0F, 0.0F, elevation + cutAbove};
}

const char* PlanViewPanel::GetName() const { return "Plan View"; }

void PlanViewPanel::SetLevels(std::vector<PlanLevel> levels) {
    m_levels = std::move(levels);
    if (m_activeLevel >= static_cast<int>(m_levels.size())) {
        Deactivate();
    }
}

void PlanViewPanel::SetSceneBounds(const glm::vec3& min, const glm::vec3& max) {
    m_sceneMin = min;
    m_sceneMax = max;
    m_hasBounds = true;
}

bool PlanViewPanel::ActivateLevel(int index) {
    if (index < 0 || index >= static_cast<int>(m_levels.size())) {
        return false;
    }

    const float elevation = m_levels[static_cast<std::size_t>(index)].elevation;
    const glm::vec4 eq = ComputePlanClipEquation(elevation, m_cutAbove);

    if (m_clipManager != nullptr) {
        if (m_clipPlaneId != renderer::ClipPlaneManager::kInvalidId &&
            m_clipManager->Contains(m_clipPlaneId)) {
            m_clipManager->UpdatePlane(m_clipPlaneId, eq);
            m_clipManager->SetEnabled(m_clipPlaneId, true);
        } else {
            m_clipPlaneId = m_clipManager->AddPlane(eq);
        }
    }

    m_activeLevel = index;
    ApplyCameraForPlan();
    return true;
}

void PlanViewPanel::Deactivate() {
    if (m_clipManager != nullptr && m_clipPlaneId != renderer::ClipPlaneManager::kInvalidId) {
        m_clipManager->RemovePlane(m_clipPlaneId);
    }
    m_clipPlaneId = renderer::ClipPlaneManager::kInvalidId;
    m_activeLevel = -1;
}

void PlanViewPanel::ApplyCameraForPlan() {
    if (m_camera == nullptr) {
        return;
    }

    glm::vec3 min = m_hasBounds ? m_sceneMin : glm::vec3{-1.0F};
    glm::vec3 max = m_hasBounds ? m_sceneMax : glm::vec3{1.0F};

    const glm::vec3 center = 0.5F * (min + max);
    const glm::vec3 size = max - min;
    const float orthoHeight = 1.2F * std::max(size.x, size.z);

    m_camera->SetOrbitTarget(center);
    m_camera->SetOrthographic(orthoHeight, 1.0F, 0.1F, 1000.0F);
    m_camera->SetAxisView(renderer::AxisView::Top);
}

void PlanViewPanel::OnDraw() {
    if (!IsVisible()) {
        return;
    }
    if (!ImGui::Begin(GetName())) {
        ImGui::End();
        return;
    }

    const char* currentLabel = (m_activeLevel < 0)
                                   ? "Free 3D"
                                   : m_levels[static_cast<std::size_t>(m_activeLevel)].name.c_str();

    if (ImGui::BeginCombo("Level", currentLabel)) {
        if (ImGui::Selectable("Free 3D", m_activeLevel < 0)) {
            Deactivate();
        }
        for (int i = 0; i < static_cast<int>(m_levels.size()); ++i) {
            const bool selected = (m_activeLevel == i);
            if (ImGui::Selectable(m_levels[static_cast<std::size_t>(i)].name.c_str(), selected)) {
                ActivateLevel(i);
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::SliderFloat("Cut above floor (m)", &m_cutAbove, 0.1F, 3.0F)) {
        if (m_activeLevel >= 0) {
            ActivateLevel(m_activeLevel);  // re-apply with new offset
        }
    }

    if (m_activeLevel >= 0 && ImGui::Button("Exit plan view")) {
        Deactivate();
    }

    ImGui::End();
}

}  // namespace bimeup::ui
