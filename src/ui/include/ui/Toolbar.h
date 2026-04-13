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
    using MeasureModeCallback = std::function<void(bool active)>;

    Toolbar() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetOnOpenFile(OpenFileCallback callback);
    void SetOnRenderModeChanged(RenderModeCallback callback);
    void SetOnFitToView(FitToViewCallback callback);
    void SetOnMeasureModeChanged(MeasureModeCallback callback);

    [[nodiscard]] renderer::RenderMode GetRenderMode() const;
    void SetRenderMode(renderer::RenderMode mode);

    [[nodiscard]] bool IsMeasureModeActive() const { return m_measureModeActive; }

    void TriggerOpenFile();
    void TriggerRenderMode(renderer::RenderMode mode);
    void TriggerFitToView();
    void TriggerMeasureMode(bool active);

private:
    renderer::RenderMode m_renderMode = renderer::RenderMode::Shaded;
    bool m_measureModeActive = false;
    OpenFileCallback m_onOpenFile;
    RenderModeCallback m_onRenderModeChanged;
    FitToViewCallback m_onFitToView;
    MeasureModeCallback m_onMeasureModeChanged;
};

}  // namespace bimeup::ui
