#pragma once

#include <renderer/RenderMode.h>
#include <ui/Panel.h>

#include <functional>

namespace bimeup::ui {

class Toolbar : public Panel {
public:
    using OpenFileCallback = std::function<void()>;
    using RenderModeCallback = std::function<void(renderer::RenderMode)>;
    using FitToViewCallback = std::function<void()>;
    using FrameSelectedCallback = std::function<void()>;
    using MeasureModeCallback = std::function<void(bool active)>;
    using PointOfViewCallback = std::function<void(bool active)>;

    Toolbar() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetOnOpenFile(OpenFileCallback callback);
    void SetOnRenderModeChanged(RenderModeCallback callback);
    void SetOnFitToView(FitToViewCallback callback);
    void SetOnFrameSelected(FrameSelectedCallback callback);
    void SetOnMeasureModeChanged(MeasureModeCallback callback);
    void SetOnPointOfViewChanged(PointOfViewCallback callback);

    [[nodiscard]] renderer::RenderMode GetRenderMode() const;
    void SetRenderMode(renderer::RenderMode mode);

    [[nodiscard]] bool IsMeasureModeActive() const { return m_measureModeActive; }
    [[nodiscard]] bool IsPointOfViewActive() const { return m_pointOfViewActive; }

    void TriggerOpenFile();
    void TriggerRenderMode(renderer::RenderMode mode);
    void TriggerFitToView();
    void TriggerFrameSelected();
    void TriggerMeasureMode(bool active);
    void TriggerPointOfView(bool active);

private:
    renderer::RenderMode m_renderMode = renderer::RenderMode::Shaded;
    bool m_measureModeActive = false;
    bool m_pointOfViewActive = false;
    OpenFileCallback m_onOpenFile;
    RenderModeCallback m_onRenderModeChanged;
    FitToViewCallback m_onFitToView;
    FrameSelectedCallback m_onFrameSelected;
    MeasureModeCallback m_onMeasureModeChanged;
    PointOfViewCallback m_onPointOfViewChanged;
};

}  // namespace bimeup::ui
