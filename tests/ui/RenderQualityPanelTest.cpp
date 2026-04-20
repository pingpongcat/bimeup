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
