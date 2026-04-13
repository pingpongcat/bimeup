#include <gtest/gtest.h>

#include <renderer/RenderMode.h>
#include <ui/Toolbar.h>

namespace {

using bimeup::renderer::RenderMode;
using bimeup::ui::Toolbar;

TEST(ToolbarTest, HasPanelName) {
    Toolbar toolbar;
    EXPECT_STREQ(toolbar.GetName(), "Toolbar");
}

TEST(ToolbarTest, DefaultRenderModeIsShaded) {
    Toolbar toolbar;
    EXPECT_EQ(toolbar.GetRenderMode(), RenderMode::Shaded);
}

TEST(ToolbarTest, SetRenderModeUpdatesState) {
    Toolbar toolbar;
    toolbar.SetRenderMode(RenderMode::Wireframe);
    EXPECT_EQ(toolbar.GetRenderMode(), RenderMode::Wireframe);
}

TEST(ToolbarTest, TriggerOpenFileInvokesCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnOpenFile([&calls] { ++calls; });
    toolbar.TriggerOpenFile();
    toolbar.TriggerOpenFile();
    EXPECT_EQ(calls, 2);
}

TEST(ToolbarTest, TriggerOpenFileNoCallbackIsNoop) {
    Toolbar toolbar;
    toolbar.TriggerOpenFile();  // must not crash
    SUCCEED();
}

TEST(ToolbarTest, TriggerRenderModeUpdatesStateAndInvokesCallback) {
    Toolbar toolbar;
    RenderMode received = RenderMode::Shaded;
    int calls = 0;
    toolbar.SetOnRenderModeChanged([&](RenderMode mode) {
        received = mode;
        ++calls;
    });

    toolbar.TriggerRenderMode(RenderMode::Wireframe);
    EXPECT_EQ(toolbar.GetRenderMode(), RenderMode::Wireframe);
    EXPECT_EQ(received, RenderMode::Wireframe);
    EXPECT_EQ(calls, 1);
}

TEST(ToolbarTest, TriggerRenderModeWithSameModeDoesNotFireCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnRenderModeChanged([&](RenderMode) { ++calls; });

    toolbar.TriggerRenderMode(RenderMode::Shaded);  // same as default
    EXPECT_EQ(calls, 0);
}

TEST(ToolbarTest, TriggerFitToViewInvokesCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnFitToView([&] { ++calls; });
    toolbar.TriggerFitToView();
    EXPECT_EQ(calls, 1);
}

TEST(ToolbarTest, TriggerFitToViewNoCallbackIsNoop) {
    Toolbar toolbar;
    toolbar.TriggerFitToView();
    SUCCEED();
}

TEST(ToolbarTest, MeasureModeDefaultsOff) {
    Toolbar toolbar;
    EXPECT_FALSE(toolbar.IsMeasureModeActive());
}

TEST(ToolbarTest, TriggerMeasureModeUpdatesStateAndInvokesCallback) {
    Toolbar toolbar;
    int calls = 0;
    bool received = false;
    toolbar.SetOnMeasureModeChanged([&](bool active) {
        received = active;
        ++calls;
    });
    toolbar.TriggerMeasureMode(true);
    EXPECT_TRUE(toolbar.IsMeasureModeActive());
    EXPECT_TRUE(received);
    EXPECT_EQ(calls, 1);

    toolbar.TriggerMeasureMode(false);
    EXPECT_FALSE(toolbar.IsMeasureModeActive());
    EXPECT_FALSE(received);
    EXPECT_EQ(calls, 2);
}

TEST(ToolbarTest, TriggerMeasureModeWithSameStateDoesNotFireCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnMeasureModeChanged([&](bool) { ++calls; });
    toolbar.TriggerMeasureMode(false);  // same as default
    EXPECT_EQ(calls, 0);
}

}  // namespace
