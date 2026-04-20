#pragma once

#include <ui/Panel.h>

#include <functional>

namespace bimeup::ui {

class Toolbar : public Panel {
public:
    using OpenFileCallback = std::function<void()>;
    using EdgesCallback = std::function<void(bool enabled)>;
    using FitToViewCallback = std::function<void()>;
    using FrameSelectedCallback = std::function<void()>;
    using MeasureModeCallback = std::function<void(bool active)>;
    using PointOfViewCallback = std::function<void(bool active)>;
    using MeasurementsVisibleCallback = std::function<void(bool visible)>;

    Toolbar() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetOnOpenFile(OpenFileCallback callback);
    void SetOnEdgesChanged(EdgesCallback callback);
    void SetOnFitToView(FitToViewCallback callback);
    void SetOnFrameSelected(FrameSelectedCallback callback);
    void SetOnMeasureModeChanged(MeasureModeCallback callback);
    void SetOnPointOfViewChanged(PointOfViewCallback callback);
    void SetOnMeasurementsVisibilityChanged(MeasurementsVisibleCallback callback);

    [[nodiscard]] bool AreEdgesEnabled() const { return m_edgesEnabled; }
    void SetEdgesEnabled(bool enabled) { m_edgesEnabled = enabled; }

    [[nodiscard]] bool IsMeasureModeActive() const { return m_measureModeActive; }
    [[nodiscard]] bool IsPointOfViewActive() const { return m_pointOfViewActive; }
    [[nodiscard]] bool AreMeasurementsVisible() const { return m_measurementsVisible; }
    void SetMeasurementsVisible(bool visible) { m_measurementsVisible = visible; }

    void TriggerOpenFile();
    void TriggerEdges(bool enabled);
    void TriggerFitToView();
    void TriggerFrameSelected();
    void TriggerMeasureMode(bool active);
    void TriggerPointOfView(bool active);
    void TriggerMeasurementsVisible(bool visible);

private:
    bool m_edgesEnabled = true;
    bool m_measureModeActive = false;
    bool m_pointOfViewActive = false;
    bool m_measurementsVisible = true;
    OpenFileCallback m_onOpenFile;
    EdgesCallback m_onEdgesChanged;
    FitToViewCallback m_onFitToView;
    FrameSelectedCallback m_onFrameSelected;
    MeasureModeCallback m_onMeasureModeChanged;
    PointOfViewCallback m_onPointOfViewChanged;
    MeasurementsVisibleCallback m_onMeasurementsVisibilityChanged;
};

}  // namespace bimeup::ui
