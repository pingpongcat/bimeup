#include <gtest/gtest.h>
#include <core/Application.h>
#include <platform/Window.h>

using bimeup::core::AppConfig;
using bimeup::core::Application;

class ApplicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        bimeup::platform::Window::InitGlfw();
    }

    void TearDown() override {
        bimeup::platform::Window::TerminateGlfw();
    }

    static AppConfig MakeHeadlessConfig() {
        AppConfig config;
        config.window.visible = false;
        config.window.width = 400;
        config.window.height = 300;
        return config;
    }
};

TEST_F(ApplicationTest, StartsAndShutsDownCleanly) {
    Application app(MakeHeadlessConfig());
}

TEST_F(ApplicationTest, RunsOneFrame) {
    Application app(MakeHeadlessConfig());
    EXPECT_TRUE(app.RunOneFrame());
}

TEST_F(ApplicationTest, RequestShutdownStopsRun) {
    Application app(MakeHeadlessConfig());
    app.RequestShutdown();
    app.Run();
}

TEST_F(ApplicationTest, WindowAccessible) {
    Application app(MakeHeadlessConfig());
    EXPECT_NE(app.GetWindow().GetHandle(), nullptr);
}
