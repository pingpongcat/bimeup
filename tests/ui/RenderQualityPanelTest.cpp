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

// RP.16.4.b — three-point-shaped asserts retired. Full Sun-widget coverage
// lands in the RP.16.6 panel rewrite; these guard the minimum surface that
// 16.4.b ships (sun scene exists on settings; shadows off by default;
// mutable access works).

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

}  // namespace
