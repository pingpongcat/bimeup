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

// RP.7d SSIL (screen-space indirect lighting) knobs. The renderer gates the
// pass off under MSAA (inherits the depth-pyramid gate) regardless of the
// `enabled` flag; the flag primarily drives the dispatch for the non-MSAA
// path. Defaults match the `RunSsil` fallback constants.
struct SsilSettings {
    bool enabled{false};
    float radius{0.5F};
    float intensity{1.0F};
    float normalRejection{2.0F};
};

// RP.8c FXAA post-process knobs. The renderer runs the FXAA draw
// unconditionally (it's the only path from the LDR intermediate to the
// swapchain); when `enabled` is false the draw becomes a cheap texture
// copy. `quality` is 0 (LOW) or 1 (HIGH) and drives the sub-pixel blend
// branch in `fxaa.frag`.
struct FxaaSettings {
    bool enabled{true};
    int quality{1};  // 0 = LOW, 1 = HIGH
};

struct RenderQualitySettings {
    renderer::LightingScene lighting{renderer::MakeDefaultLighting()};

    int msaaSamples{1};          // R.2 — 1, 2, 4, 8

    OutlineSettings outline{};
    SsilSettings ssil{};
    FxaaSettings fxaa{};
    renderer::FogSettings fog{};      // RP.9b
    renderer::BloomSettings bloom{};  // RP.10c
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
