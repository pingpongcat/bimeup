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

TEST(RenderQualityPanelTest, DefaultSettingsEnableAllLights) {
    RenderQualityPanel panel;
    const auto& settings = panel.GetSettings();
    EXPECT_TRUE(settings.lighting.key.enabled);
    EXPECT_TRUE(settings.lighting.fill.enabled);
    EXPECT_TRUE(settings.lighting.rim.enabled);
}

TEST(RenderQualityPanelTest, DefaultSettingsHaveShadowsOff) {
    RenderQualityPanel panel;
    const auto& s = panel.GetSettings();
    EXPECT_FALSE(s.lighting.shadow.enabled);
}

TEST(RenderQualityPanelTest, MutableSettingsAllowsExternalUpdate) {
    RenderQualityPanel panel;
    panel.MutableSettings().lighting.key.intensity = 3.5F;
    EXPECT_FLOAT_EQ(panel.GetSettings().lighting.key.intensity, 3.5F);
}

TEST(RenderQualityPanelTest, DefaultPanelIsVisible) {
    RenderQualityPanel panel;
    EXPECT_TRUE(panel.IsVisible());
}

}  // namespace
