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
    // RP.16.4.b — three-point `lighting` replaced by sun-driven scene. The
    // panel's Sun widgets (site/date/time) are wired in RP.16.6; 16.7 loads
    // real site data on model load. `sun.exposure` feeds the tonemap pass.
    renderer::SunLightingScene sun{};

    SmaaSettings smaa{};              // RP.11c — replaces FxaaSettings

    // RP.16.6 — panel-local date/time/site UI state. On draw, the panel
    // recomputes `sun.julianDayUtc` from (year, month, day, hourLocal) +
    // the site longitude (UTC offset ≈ longitude / 15°). Year is not
    // surfaced as a widget but is kept here so tests/callers can pin it.
    int year{2026};
    int month{6};          // 1..12
    int day{21};           // 1..31
    float hourLocal{12.0F};  // 0..24 local solar time

    // When true, `sun.siteLocation` + `sun.trueNorthRad` are expected to
    // be pushed in from IfcSite metadata by main.cpp on model load
    // (RP.16.7). The panel's lat/lon sliders are read-only in that mode.
    // When false, the sliders own lat/lon.
    bool useSiteGeolocation{true};
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
