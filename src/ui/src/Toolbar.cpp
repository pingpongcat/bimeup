#include <ui/Toolbar.h>

#include <imgui.h>

namespace bimeup::ui {

const char* Toolbar::GetName() const {
    return "Toolbar";
}

void Toolbar::SetOnOpenFile(OpenFileCallback callback) {
    m_onOpenFile = std::move(callback);
}

void Toolbar::SetOnRenderModeChanged(RenderModeCallback callback) {
    m_onRenderModeChanged = std::move(callback);
}

void Toolbar::SetOnFitToView(FitToViewCallback callback) {
    m_onFitToView = std::move(callback);
}

void Toolbar::SetOnMeasureModeChanged(MeasureModeCallback callback) {
    m_onMeasureModeChanged = std::move(callback);
}

renderer::RenderMode Toolbar::GetRenderMode() const {
    return m_renderMode;
}

void Toolbar::SetRenderMode(renderer::RenderMode mode) {
    m_renderMode = mode;
}

void Toolbar::TriggerOpenFile() {
    if (m_onOpenFile) {
        m_onOpenFile();
    }
}

void Toolbar::TriggerRenderMode(renderer::RenderMode mode) {
    if (mode == m_renderMode) {
        return;
    }
    m_renderMode = mode;
    if (m_onRenderModeChanged) {
        m_onRenderModeChanged(mode);
    }
}

void Toolbar::TriggerFitToView() {
    if (m_onFitToView) {
        m_onFitToView();
    }
}

void Toolbar::TriggerMeasureMode(bool active) {
    if (active == m_measureModeActive) {
        return;
    }
    m_measureModeActive = active;
    if (m_onMeasureModeChanged) {
        m_onMeasureModeChanged(active);
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

        const bool shaded = m_renderMode == renderer::RenderMode::Shaded;
        if (ImGui::RadioButton("Shaded", shaded)) {
            TriggerRenderMode(renderer::RenderMode::Shaded);
        }
        ImGui::SameLine();
        const bool wireframe = m_renderMode == renderer::RenderMode::Wireframe;
        if (ImGui::RadioButton("Wireframe", wireframe)) {
            TriggerRenderMode(renderer::RenderMode::Wireframe);
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        if (ImGui::Button("Fit to View")) {
            TriggerFitToView();
        }

        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        bool measure = m_measureModeActive;
        if (ImGui::Checkbox("Measure", &measure)) {
            TriggerMeasureMode(measure);
        }
    }
    ImGui::End();
}

}  // namespace bimeup::ui
