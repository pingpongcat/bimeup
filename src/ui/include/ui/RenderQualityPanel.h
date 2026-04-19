#pragma once

#include <renderer/Lighting.h>
#include <ui/Panel.h>

namespace bimeup::ui {

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

    // Pre-ACES linear multiplier on composited HDR in `tonemap.frag`. The
    // three-point lighting sum (ambient ~0.6 + key ~1.0 + fill ~0.45 + rim
    // ~0.35) can push bright surfaces to ~2.5 in HDR space, which ACES
    // tonemaps to near-white. 0.6 brings the mid-grey through the knee and
    // leaves headroom for direct light without blowing out.
    float exposure{0.6F};

    SmaaSettings smaa{};              // RP.11c — replaces FxaaSettings
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
