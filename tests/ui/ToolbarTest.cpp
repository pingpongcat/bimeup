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

TEST(ToolbarTest, TriggerFrameSelectedInvokesCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnFrameSelected([&] { ++calls; });
    toolbar.TriggerFrameSelected();
    EXPECT_EQ(calls, 1);
}

TEST(ToolbarTest, TriggerFrameSelectedNoCallbackIsNoop) {
    Toolbar toolbar;
    toolbar.TriggerFrameSelected();
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

TEST(ToolbarTest, PointOfViewDefaultsOff) {
    Toolbar toolbar;
    EXPECT_FALSE(toolbar.IsPointOfViewActive());
}

TEST(ToolbarTest, TriggerPointOfViewUpdatesStateAndInvokesCallback) {
    Toolbar toolbar;
    int calls = 0;
    bool received = false;
    toolbar.SetOnPointOfViewChanged([&](bool active) {
        received = active;
        ++calls;
    });
    toolbar.TriggerPointOfView(true);
    EXPECT_TRUE(toolbar.IsPointOfViewActive());
    EXPECT_TRUE(received);
    EXPECT_EQ(calls, 1);

    toolbar.TriggerPointOfView(false);
    EXPECT_FALSE(toolbar.IsPointOfViewActive());
    EXPECT_FALSE(received);
    EXPECT_EQ(calls, 2);
}

TEST(ToolbarTest, TriggerPointOfViewWithSameStateDoesNotFireCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnPointOfViewChanged([&](bool) { ++calls; });
    toolbar.TriggerPointOfView(false);  // same as default
    EXPECT_EQ(calls, 0);
}

TEST(ToolbarTest, ActivatingPointOfViewDeactivatesMeasure) {
    Toolbar toolbar;
    int measureCalls = 0;
    bool measureState = true;
    toolbar.SetOnMeasureModeChanged([&](bool active) {
        measureState = active;
        ++measureCalls;
    });
    toolbar.TriggerMeasureMode(true);
    EXPECT_EQ(measureCalls, 1);
    EXPECT_TRUE(measureState);

    toolbar.TriggerPointOfView(true);
    EXPECT_TRUE(toolbar.IsPointOfViewActive());
    EXPECT_FALSE(toolbar.IsMeasureModeActive());
    EXPECT_EQ(measureCalls, 2);  // one on, one off from the mutex
    EXPECT_FALSE(measureState);
}

TEST(ToolbarTest, ActivatingMeasureDeactivatesPointOfView) {
    Toolbar toolbar;
    int povCalls = 0;
    bool povState = true;
    toolbar.SetOnPointOfViewChanged([&](bool active) {
        povState = active;
        ++povCalls;
    });
    toolbar.TriggerPointOfView(true);
    EXPECT_EQ(povCalls, 1);
    EXPECT_TRUE(povState);

    toolbar.TriggerMeasureMode(true);
    EXPECT_TRUE(toolbar.IsMeasureModeActive());
    EXPECT_FALSE(toolbar.IsPointOfViewActive());
    EXPECT_EQ(povCalls, 2);
    EXPECT_FALSE(povState);
}

TEST(ToolbarTest, MeasurementsVisibleDefaultsOn) {
    Toolbar toolbar;
    EXPECT_TRUE(toolbar.AreMeasurementsVisible());
}

TEST(ToolbarTest, TriggerMeasurementsVisibleUpdatesStateAndInvokesCallback) {
    Toolbar toolbar;
    int calls = 0;
    bool received = true;
    toolbar.SetOnMeasurementsVisibilityChanged([&](bool visible) {
        received = visible;
        ++calls;
    });
    toolbar.TriggerMeasurementsVisible(false);
    EXPECT_FALSE(toolbar.AreMeasurementsVisible());
    EXPECT_FALSE(received);
    EXPECT_EQ(calls, 1);

    toolbar.TriggerMeasurementsVisible(true);
    EXPECT_TRUE(toolbar.AreMeasurementsVisible());
    EXPECT_TRUE(received);
    EXPECT_EQ(calls, 2);
}

TEST(ToolbarTest, SetMeasurementsVisibleDoesNotFireCallback) {
    Toolbar toolbar;
    int calls = 0;
    toolbar.SetOnMeasurementsVisibilityChanged([&](bool) { ++calls; });
    toolbar.SetMeasurementsVisible(false);  // external sync — no callback
    EXPECT_FALSE(toolbar.AreMeasurementsVisible());
    EXPECT_EQ(calls, 0);
}

TEST(ToolbarTest, DeactivatingOneDoesNotToggleTheOther) {
    Toolbar toolbar;
    int measureCalls = 0;
    int povCalls = 0;
    toolbar.SetOnMeasureModeChanged([&](bool) { ++measureCalls; });
    toolbar.SetOnPointOfViewChanged([&](bool) { ++povCalls; });

    toolbar.TriggerMeasureMode(true);
    toolbar.TriggerMeasureMode(false);
    EXPECT_EQ(measureCalls, 2);
    EXPECT_EQ(povCalls, 0);

    toolbar.TriggerPointOfView(true);
    toolbar.TriggerPointOfView(false);
    EXPECT_EQ(measureCalls, 2);
    EXPECT_EQ(povCalls, 2);
}

}  // namespace
