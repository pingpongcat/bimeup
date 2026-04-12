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

    Toolbar() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    void SetOnOpenFile(OpenFileCallback callback);
    void SetOnRenderModeChanged(RenderModeCallback callback);
    void SetOnFitToView(FitToViewCallback callback);

    [[nodiscard]] renderer::RenderMode GetRenderMode() const;
    void SetRenderMode(renderer::RenderMode mode);

    void TriggerOpenFile();
    void TriggerRenderMode(renderer::RenderMode mode);
    void TriggerFitToView();

private:
    renderer::RenderMode m_renderMode = renderer::RenderMode::Shaded;
    OpenFileCallback m_onOpenFile;
    RenderModeCallback m_onRenderModeChanged;
    FitToViewCallback m_onFitToView;
};

}  // namespace bimeup::ui
