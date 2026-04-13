#pragma once

#include <renderer/Lighting.h>
#include <ui/Panel.h>

namespace bimeup::ui {

struct RenderQualitySettings {
    renderer::LightingScene lighting{renderer::MakeDefaultLighting()};

    int msaaSamples{1};          // R.2 — 1, 2, 4, 8
    bool shadowsEnabled{false};  // R.3
    int shadowMapSize{1024};
    bool ssaoEnabled{false};     // R.4
    float ssaoRadius{0.5F};
    float ssaoBias{0.025F};
};

class RenderQualityPanel : public Panel {
public:
    RenderQualityPanel() = default;

    [[nodiscard]] const char* GetName() const override;
    void OnDraw() override;

    [[nodiscard]] const RenderQualitySettings& GetSettings() const { return m_settings; }
    [[nodiscard]] RenderQualitySettings& MutableSettings() { return m_settings; }

private:
    RenderQualitySettings m_settings{};
};

}  // namespace bimeup::ui
