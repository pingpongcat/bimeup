#include <gtest/gtest.h>

#include <ui/RenderQualityPanel.h>

using bimeup::ui::RenderQualityPanel;
using bimeup::ui::RenderQualitySettings;

// RP.14.2 — symmetric to RP.14.1.a's `RenderLoopExposesMsaaAccessors` and
// RP.14.1.b's `PipelineConfigExposesRasterizationSamples` guards. The panel
// was the last MSAA-shaped surface left in the UI — if someone re-adds the
// `msaaSamples` field, this static_assert breaks the build before a user-
// facing regression (MSAA radio disabling XeGTAO) can ship.
template <typename T>
concept RenderQualitySettingsExposesMsaaSamples =
    requires(T s) { s.msaaSamples; };
static_assert(!RenderQualitySettingsExposesMsaaSamples<RenderQualitySettings>,
              "RP.14.2 — RenderQualitySettings::msaaSamples retired; MSAA is "
              "gone project-wide and cannot come back without revisiting the "
              "XeGTAO / outline / depth-pyramid gates that depend on 1×.");

namespace {

TEST(RenderQualityPanelTest, HasPanelName) {
    RenderQualityPanel panel;
    EXPECT_STREQ(panel.GetName(), "Render Quality");
}

// RP.16.6 — three-point + sky-colour fields retired; the panel drives a
// single sun-driven scene. These asserts pin the RenderQualitySettings
// shape after the RP.16.6 rewrite: sun scene, SMAA, panel-local
// date/time UI state, and the "use site geolocation" switch.

TEST(RenderQualityPanelTest, DefaultSettingsHaveShadowsOff) {
    RenderQualityPanel panel;
    const auto& s = panel.GetSettings();
    EXPECT_FALSE(s.sun.shadow.enabled);
}

TEST(RenderQualityPanelTest, MutableSettingsAllowsExternalUpdate) {
    RenderQualityPanel panel;
    panel.MutableSettings().sun.indoorLightsEnabled = true;
    EXPECT_TRUE(panel.GetSettings().sun.indoorLightsEnabled);
}

TEST(RenderQualityPanelTest, DefaultPanelIsVisible) {
    RenderQualityPanel panel;
    EXPECT_TRUE(panel.IsVisible());
}

TEST(RenderQualityPanelTest, DefaultDateTimeIsMidsummerNoon) {
    // Midsummer-noon default maximises sun elevation at northern-hemisphere
    // sites — a sensible starting render for architectural viewing.
    RenderQualityPanel panel;
    const auto& s = panel.GetSettings();
    EXPECT_EQ(s.month, 6);
    EXPECT_EQ(s.day, 21);
    EXPECT_FLOAT_EQ(s.hourLocal, 12.0F);
}

TEST(RenderQualityPanelTest, DefaultUsesSiteGeolocation) {
    // RP.16.7 pushes IfcSite lat/lon in on model load; default ON so users
    // get plausible geography out of the box without touching sliders.
    RenderQualityPanel panel;
    EXPECT_TRUE(panel.GetSettings().useSiteGeolocation);
}

TEST(RenderQualityPanelTest, DefaultIndoorLightsOff) {
    RenderQualityPanel panel;
    EXPECT_FALSE(panel.GetSettings().sun.indoorLightsEnabled);
}

// RP.18.5 — window transmission ships enabled; the whole point of RP.18 is
// that the default raster renderer tints sun through IfcWindow glass without
// user opt-in. Disabling this toggle falls back to the pre-RP.18 binary
// visibility path (bit-compatible regression guard).
TEST(RenderQualityPanelTest, DefaultWindowTransmissionOn) {
    RenderQualityPanel panel;
    EXPECT_TRUE(panel.GetSettings().sun.shadow.windowTransmission);
}

// RP.19 — SMAA tuning knobs. Threshold controls the edge-detection gate in
// `smaa_edge.frag`; 0.1 matches the iryoku/smaa reference "balanced" value and
// is what RenderLoop pushed as a hardcoded literal pre-RP.19. Quality preset
// drives `smaa_weights.frag`'s max-search-steps: HIGH = iryoku's HIGH preset
// (16 / 8), which is what shipped before RP.19 — default stays HIGH so the
// out-of-the-box image is bit-compatible.
TEST(RenderQualityPanelTest, DefaultSmaaThresholdIsPointOne) {
    RenderQualityPanel panel;
    EXPECT_FLOAT_EQ(panel.GetSettings().smaa.threshold, 0.1F);
}

TEST(RenderQualityPanelTest, DefaultSmaaQualityIsHigh) {
    RenderQualityPanel panel;
    EXPECT_EQ(panel.GetSettings().smaa.quality, bimeup::ui::SmaaQuality::High);
}

// RP.21 — edges default ON now that the overlay has per-setting knobs. The
// pre-RP.21 default was off + a measurement-mode auto-enable hack; both are
// retired. Colour/opacity/width defaults match the old hardcoded `kEdgeColor`
// literal so the out-of-the-box look is unchanged except that edges now show
// immediately at startup.
TEST(RenderQualityPanelTest, DefaultEdgesEnabled) {
    RenderQualityPanel panel;
    EXPECT_TRUE(panel.GetSettings().edges.enabled);
}

TEST(RenderQualityPanelTest, DefaultEdgeStyleMatchesPreRp21Literal) {
    RenderQualityPanel panel;
    const auto& edges = panel.GetSettings().edges;
    EXPECT_FLOAT_EQ(edges.color.r, 0.25F);
    EXPECT_FLOAT_EQ(edges.color.g, 0.25F);
    EXPECT_FLOAT_EQ(edges.color.b, 0.25F);
    EXPECT_FLOAT_EQ(edges.opacity, 0.55F);
    EXPECT_FLOAT_EQ(edges.width, 1.0F);
}

// RP.20 — XeGTAO knobs pin the architectural defaults chosen at RP.12d /
// RP.12e. Shifting any of these changes every rendered frame; locking them
// here means a future slider tweak has to announce itself via a failing test.
TEST(RenderQualityPanelTest, DefaultSsaoParamsAreArchitectural) {
    RenderQualityPanel panel;
    const auto& ssao = panel.GetSettings().ssao;
    EXPECT_FLOAT_EQ(ssao.radius, 0.35F);
    EXPECT_FLOAT_EQ(ssao.falloff, 0.6F);
    EXPECT_FLOAT_EQ(ssao.intensity, 0.5F);
    EXPECT_FLOAT_EQ(ssao.shadowPower, 1.5F);
}

TEST(RenderQualityPanelTest, SmaaQualityPresetsMapToIryokuSearchSteps) {
    // LOW = iryoku LOW (4/2), MEDIUM = iryoku MEDIUM (8/4), HIGH = iryoku HIGH
    // (16/8). These numbers feed `smaa_weights.frag`'s push constants; pinning
    // them here means a future "let's try 12/6" tweak has to announce itself
    // via a failing test rather than silently shifting the default render.
    EXPECT_EQ(bimeup::ui::MaxSearchSteps(bimeup::ui::SmaaQuality::Low), 4);
    EXPECT_EQ(bimeup::ui::MaxSearchSteps(bimeup::ui::SmaaQuality::Medium), 8);
    EXPECT_EQ(bimeup::ui::MaxSearchSteps(bimeup::ui::SmaaQuality::High), 16);
    EXPECT_EQ(bimeup::ui::MaxSearchStepsDiag(bimeup::ui::SmaaQuality::Low), 2);
    EXPECT_EQ(bimeup::ui::MaxSearchStepsDiag(bimeup::ui::SmaaQuality::Medium), 4);
    EXPECT_EQ(bimeup::ui::MaxSearchStepsDiag(bimeup::ui::SmaaQuality::High), 8);
}

// Stage 9.Q.4 — three-radio render-mode selector pinned to Rasterised by
// default so every launch is bit-compatible with the pre-Stage-9 classical
// renderer. `Ray query` becomes selectable once `rayQueryAvailable` is
// flipped by main.cpp after `Device::HasRayQuery()`. `Ray tracing` is a
// future-work hook — the enum value exists so the panel UI shape doesn't
// need to change when 9.RT lands, but the panel keeps the radio
// permanently disabled with a tooltip until then.
TEST(RenderQualityPanelTest, DefaultRenderModeIsRasterised) {
    RenderQualityPanel panel;
    EXPECT_EQ(panel.GetSettings().mode, bimeup::ui::RenderMode::Rasterised);
}

TEST(RenderQualityPanelTest, DefaultRayQueryAvailabilityIsFalse) {
    RenderQualityPanel panel;
    EXPECT_FALSE(panel.GetSettings().rayQueryAvailable);
}

TEST(RenderQualityPanelTest, RenderModeEnumHasRayQueryAndRayTracing) {
    // Compile-time guard: the 9.Q.4 pivot retired the 9.8.d
    // `HybridRt` / `PathTraced` enum values in favour of `RayQuery` /
    // `RayTracing`. A reintroduction of the old names would fail to
    // compile this test (and main.cpp's mode mapping).
    [[maybe_unused]] constexpr auto rq = bimeup::ui::RenderMode::RayQuery;
    [[maybe_unused]] constexpr auto rt = bimeup::ui::RenderMode::RayTracing;
}

TEST(RenderQualityPanelTest, MutableSettingsAllowsDateTimeEdits) {
    RenderQualityPanel panel;
    auto& s = panel.MutableSettings();
    s.month = 12;
    s.day = 21;
    s.hourLocal = 9.5F;
    s.useSiteGeolocation = false;
    EXPECT_EQ(panel.GetSettings().month, 12);
    EXPECT_EQ(panel.GetSettings().day, 21);
    EXPECT_FLOAT_EQ(panel.GetSettings().hourLocal, 9.5F);
    EXPECT_FALSE(panel.GetSettings().useSiteGeolocation);
}

}  // namespace
