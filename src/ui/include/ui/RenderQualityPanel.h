#pragma once

#include <renderer/Lighting.h>
#include <ui/Panel.h>

#include <glm/vec4.hpp>

namespace bimeup::ui {

// RP.6d outline-pass knobs surfaced to the user. Driven by the panel's
// "Selection outline" section; `main.cpp` reads these into
// `OutlinePipeline::PushConstants` each frame. `texelSize` is filled in by
// the renderer per-frame from the swapchain extent so it tracks resizes.
struct OutlineSettings {
    bool enabled{true};
    glm::vec4 selectedColor{1.0F, 0.6F, 0.1F, 1.0F};
    glm::vec4 hoverColor{0.2F, 0.7F, 1.0F, 0.8F};
    float thickness{2.0F};            // pixels between centre tap and edge tap
    float depthEdgeThreshold{0.05F};  // metres — Sobel cutoff for the within-selection fallback
};

struct RenderQualitySettings {
    renderer::LightingScene lighting{renderer::MakeDefaultLighting()};

    int msaaSamples{1};          // R.2 — 1, 2, 4, 8

    OutlineSettings outline{};
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
