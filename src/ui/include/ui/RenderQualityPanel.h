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
// path. RP.12c tuning: default intensity lowered to 0.15 (wide-area bounces
// were dominating the tonemapped frame at 1.0), and `maxLuminance` caps the
// per-channel post-accumulation contribution so uniformly-lit walls can't
// glow past the cap even when 64 taps all agree.
struct SsilSettings {
    bool enabled{false};
    float radius{0.5F};
    float intensity{0.15F};
    float normalRejection{2.0F};
    float maxLuminance{0.5F};
};

// RP.11c SMAA 1x post-process knobs. The renderer runs the blend draw
// unconditionally (it's the only path from the LDR intermediate to the
// swapchain); when `enabled` is false the edge + weights passes are
// skipped and the blend shader short-circuits to a passthrough via its
// push-constant `enabled` flag, so disabling AA doesn't pay the 3-pass
// cost or the stale-weights risk. No quality preset — SMAA 1x defaults
// ship sharp (scope decision at RP.11 kickoff).
struct SmaaSettings {
    bool enabled{true};
};

struct RenderQualitySettings {
    renderer::LightingScene lighting{renderer::MakeDefaultLighting()};

    int msaaSamples{1};          // R.2 — 1, 2, 4, 8

    // Pre-ACES linear multiplier on composited HDR in `tonemap.frag`. The
    // three-point lighting sum (ambient ~0.6 + key ~1.0 + fill ~0.45 + rim
    // ~0.35) can push bright surfaces to ~2.5 in HDR space, which ACES
    // tonemaps to near-white. 0.6 brings the mid-grey through the knee and
    // leaves headroom for direct light without blowing out.
    float exposure{0.6F};

    OutlineSettings outline{};
    SsilSettings ssil{};
    SmaaSettings smaa{};              // RP.11c — replaces FxaaSettings
    renderer::FogSettings fog{};      // RP.9b
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
