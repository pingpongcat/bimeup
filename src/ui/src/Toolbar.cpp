#include <ui/Toolbar.h>

#include <imgui.h>

namespace bimeup::ui {

const char* Toolbar::GetName() const {
    return "Toolbar";
}

void Toolbar::SetOnOpenFile(OpenFileCallback callback) {
    m_onOpenFile = std::move(callback);
}

void Toolbar::SetOnEdgesChanged(EdgesCallback callback) {
    m_onEdgesChanged = std::move(callback);
}

void Toolbar::SetOnFitToView(FitToViewCallback callback) {
    m_onFitToView = std::move(callback);
}

void Toolbar::SetOnFrameSelected(FrameSelectedCallback callback) {
    m_onFrameSelected = std::move(callback);
}

void Toolbar::SetOnMeasureModeChanged(MeasureModeCallback callback) {
    m_onMeasureModeChanged = std::move(callback);
}

void Toolbar::SetOnPointOfViewChanged(PointOfViewCallback callback) {
    m_onPointOfViewChanged = std::move(callback);
}

void Toolbar::SetOnMeasurementsVisibilityChanged(
    MeasurementsVisibleCallback callback) {
    m_onMeasurementsVisibilityChanged = std::move(callback);
}

void Toolbar::TriggerOpenFile() {
    if (m_onOpenFile) {
        m_onOpenFile();
    }
}

void Toolbar::TriggerEdges(bool enabled) {
    if (enabled == m_edgesEnabled) {
        return;
    }
    m_edgesEnabled = enabled;
    if (m_onEdgesChanged) {
        m_onEdgesChanged(enabled);
    }
}

void Toolbar::TriggerFitToView() {
    if (m_onFitToView) {
        m_onFitToView();
    }
}

void Toolbar::TriggerFrameSelected() {
    if (m_onFrameSelected) {
        m_onFrameSelected();
    }
}

void Toolbar::TriggerMeasureMode(bool active) {
    if (active == m_measureModeActive) {
        return;
    }
    // Measure and Point of View are mutually exclusive: the FPS camera owns
    // the mouse while active, so measurement clicks would pick through the
    // ghosted scene and previously crashed.
    if (active && m_pointOfViewActive) {
        TriggerPointOfView(false);
    }
    m_measureModeActive = active;
    if (m_onMeasureModeChanged) {
        m_onMeasureModeChanged(active);
    }
}

void Toolbar::TriggerPointOfView(bool active) {
    if (active == m_pointOfViewActive) {
        return;
    }
    if (active && m_measureModeActive) {
        TriggerMeasureMode(false);
    }
    m_pointOfViewActive = active;
    if (m_onPointOfViewChanged) {
        m_onPointOfViewChanged(active);
    }
}

void Toolbar::TriggerMeasurementsVisible(bool visible) {
    if (visible == m_measurementsVisible) {
        return;
    }
    m_measurementsVisible = visible;
    if (m_onMeasurementsVisibilityChanged) {
        m_onMeasurementsVisibilityChanged(visible);
    }
}

void Toolbar::OnDraw() {
    if (ImGui::Begin(GetName())) {
        if (ImGui::Button("Open File...")) {
            TriggerOpenFile();
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        ImGui::TextUnformatted("Shaded");
        ImGui::SameLine();
        bool edges = m_edgesEnabled;
        if (ImGui::Checkbox("Edges", &edges)) {
            TriggerEdges(edges);
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        if (ImGui::Button("Fit to View")) {
            TriggerFitToView();
        }
        ImGui::SameLine();
        if (ImGui::Button("Frame Selected")) {
            TriggerFrameSelected();
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        bool measure = m_measureModeActive;
        if (ImGui::Checkbox("Measure", &measure)) {
            TriggerMeasureMode(measure);
        }
        ImGui::SameLine();
        bool measurementsVisible = m_measurementsVisible;
        if (ImGui::Checkbox("Show Measurements", &measurementsVisible)) {
            TriggerMeasurementsVisible(measurementsVisible);
        }
        ImGui::SameLine();
        bool pov = m_pointOfViewActive;
        if (ImGui::Checkbox("Point of View", &pov)) {
            TriggerPointOfView(pov);
        }
    }
    ImGui::End();
}

}  // namespace bimeup::ui
