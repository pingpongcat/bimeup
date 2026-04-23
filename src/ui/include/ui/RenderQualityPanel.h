#pragma once

#include <glm/vec3.hpp>

#include <renderer/Lighting.h>
#include <ui/Panel.h>

namespace bimeup::ui {

// RP.19 — iryoku/smaa's LOW/MEDIUM/HIGH presets, differing only in the
// horizontal/vertical + diagonal max-search-steps. HIGH is the pre-RP.19
// default (bit-compatible); LOW and MEDIUM trade edge-travel for fill rate
// on fragments whose edges extend beyond the search radius (long thin
// staircases, long facade runs).
enum class SmaaQuality {
    Low,
    Medium,
    High,
};

constexpr int MaxSearchSteps(SmaaQuality q) {
    switch (q) {
        case SmaaQuality::Low:    return 4;
        case SmaaQuality::Medium: return 8;
        case SmaaQuality::High:   return 16;
    }
    return 16;
}

constexpr int MaxSearchStepsDiag(SmaaQuality q) {
    switch (q) {
        case SmaaQuality::Low:    return 2;
        case SmaaQuality::Medium: return 4;
        case SmaaQuality::High:   return 8;
    }
    return 8;
}

// RP.11c / RP.19 — SMAA 1x post-process knobs. The renderer runs the blend
// draw unconditionally (only path from the LDR intermediate to the swap-
// chain); when `enabled` is false the edge + weights passes are skipped and
// the blend shader short-circuits to a passthrough via its push-constant
// `enabled` flag, so disabling AA doesn't pay the 3-pass cost or the stale-
// weights risk. `threshold` is the edge-detection luma gate (0.05 aggressive,
// 0.1 balanced, 0.2 conservative); `quality` selects iryoku's LOW/MEDIUM/
// HIGH search-step preset.
struct SmaaSettings {
    bool enabled{true};
    float threshold{0.1F};
    SmaaQuality quality{SmaaQuality::High};
};

// RP.20 — XeGTAO runtime knobs. Defaults mirror the pre-RP.20 hardcoded
// literals in `RenderLoop::RunXeGtao` (chosen at RP.12d for architectural
// scenes: contact-AO at 35 cm, 60 % full-horizon falloff, balanced
// intensity + shadowPower). `falloff` replaces the Chapman `bias` field
// retired at RP.12e.
struct SsaoSettings {
    float radius{0.35F};       // view-space sample radius (metres)
    float falloff{0.6F};       // horizon tap falloff ratio [0, 1]
    float intensity{0.5F};     // darkening multiplier; 0 = off, 1 = reference
    float shadowPower{1.5F};   // exponent on the final AO term
};

// RP.21 — feature-edge overlay runtime knobs. `enabled` defaults on (pre-
// RP.21 defaulted off with a measurement-mode auto-enable hack — both
// retired here). `color` + `opacity` feed `edge_overlay.frag`'s push
// constant; `width` drives `vkCmdSetLineWidth` via the overlay pipeline's
// `VK_DYNAMIC_STATE_LINE_WIDTH`. Width > 1.0 requires
// `Device::HasWideLines()`; callers clamp to 1.0 on devices that lack it.
struct EdgeOverlaySettings {
    bool enabled{true};
    glm::vec3 color{0.25F, 0.25F, 0.25F};
    float opacity{0.55F};
    float width{1.0F};
};

// Stage 9.Q.4 — three-mode user-facing render-mode selector. Pivot
// 2026-04-23 retired the 9.8.d hybrid-composite design; the new modes are:
//   - `Rasterised` (default): classical raster path, bit-compatible with
//     the pre-Stage-9 renderer. Always available.
//   - `RayQuery`: still the forward-shaded raster path, but `basic.frag`
//     swaps the PCF shadow-map sample for an inline `rayQueryEXT` trace
//     against the TLAS. Gated on `Device::HasRayQuery()` — callers
//     disable the radio when the device doesn't expose `VK_KHR_ray_query`.
//   - `RayTracing`: future-work placeholder for a fully separate RT
//     pipeline (9.RT). Permanently disabled in the panel today; the enum
//     value exists so the UI shape doesn't change when 9.RT lands.
enum class RenderMode {
    Rasterised,
    RayQuery,
    RayTracing,
};

struct RenderQualitySettings {
    // RP.16.4.b — three-point `lighting` replaced by sun-driven scene. The
    // panel's Sun widgets (site/date/time) are wired in RP.16.6; 16.7 loads
    // real site data on model load. `sun.exposure` feeds the tonemap pass.
    renderer::SunLightingScene sun{};

    SmaaSettings smaa{};              // RP.11c — replaces FxaaSettings
    SsaoSettings ssao{};              // RP.20 — XeGTAO tuning knobs
    EdgeOverlaySettings edges{};      // RP.21 — feature-edge overlay

    // Stage 9.Q.4 — render-mode selector. Default `Rasterised` keeps the
    // out-of-the-box experience bit-compatible with the pre-Stage-9 path.
    RenderMode mode{RenderMode::Rasterised};

    // Stage 9.Q.4 — set by main.cpp from `Device::HasRayQuery()` so the
    // panel can grey out the `Ray query` radio when the GPU doesn't
    // advertise `VK_KHR_ray_query`. The panel does not know about
    // `Device`; main flips this once at startup. The `Ray tracing` radio
    // stays permanently disabled regardless of this flag (9.RT placeholder).
    bool rayQueryAvailable{false};

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
