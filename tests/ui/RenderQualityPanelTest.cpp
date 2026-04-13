#include <gtest/gtest.h>

#include <ui/RenderQualityPanel.h>

using bimeup::ui::RenderQualityPanel;
using bimeup::ui::RenderQualitySettings;

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

TEST(RenderQualityPanelTest, DefaultSettingsHaveMsaa1ShadowsOff) {
    RenderQualityPanel panel;
    const auto& s = panel.GetSettings();
    EXPECT_EQ(s.msaaSamples, 1);
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
