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
    using MeasurementsVisibleCallback = std::function<void(bool visible)>;

    Toolbar() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetOnOpenFile(OpenFileCallback callback);
    void SetOnRenderModeChanged(RenderModeCallback callback);
    void SetOnFitToView(FitToViewCallback callback);
    void SetOnFrameSelected(FrameSelectedCallback callback);
    void SetOnMeasureModeChanged(MeasureModeCallback callback);
    void SetOnPointOfViewChanged(PointOfViewCallback callback);
    void SetOnMeasurementsVisibilityChanged(MeasurementsVisibleCallback callback);

    [[nodiscard]] renderer::RenderMode GetRenderMode() const;
    void SetRenderMode(renderer::RenderMode mode);

    [[nodiscard]] bool IsMeasureModeActive() const { return m_measureModeActive; }
    [[nodiscard]] bool IsPointOfViewActive() const { return m_pointOfViewActive; }
    [[nodiscard]] bool AreMeasurementsVisible() const { return m_measurementsVisible; }
    void SetMeasurementsVisible(bool visible) { m_measurementsVisible = visible; }

    void TriggerOpenFile();
    void TriggerRenderMode(renderer::RenderMode mode);
    void TriggerFitToView();
    void TriggerFrameSelected();
    void TriggerMeasureMode(bool active);
    void TriggerPointOfView(bool active);
    void TriggerMeasurementsVisible(bool visible);

private:
    renderer::RenderMode m_renderMode = renderer::RenderMode::Shaded;
    bool m_measureModeActive = false;
    bool m_pointOfViewActive = false;
    bool m_measurementsVisible = true;
    OpenFileCallback m_onOpenFile;
    RenderModeCallback m_onRenderModeChanged;
    FitToViewCallback m_onFitToView;
    FrameSelectedCallback m_onFrameSelected;
    MeasureModeCallback m_onMeasureModeChanged;
    PointOfViewCallback m_onPointOfViewChanged;
    MeasurementsVisibleCallback m_onMeasurementsVisibilityChanged;
};

}  // namespace bimeup::ui
