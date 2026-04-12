#include <gtest/gtest.h>
#include <platform/Window.h>

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        bimeup::platform::Window::InitGlfw();
    }

    void TearDown() override {
        bimeup::platform::Window::TerminateGlfw();
    }
};

TEST_F(WindowTest, CreateWithDefaultConfig) {
    bimeup::platform::WindowConfig config;
    config.visible = false;  // headless-friendly
    bimeup::platform::Window window(config);

    EXPECT_NE(window.GetHandle(), nullptr);
}

TEST_F(WindowTest, QuerySize) {
    bimeup::platform::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.visible = false;
    bimeup::platform::Window window(config);

    auto size = window.GetSize();
    EXPECT_EQ(size.x, 800);
    EXPECT_EQ(size.y, 600);
}

TEST_F(WindowTest, ShouldCloseIsFalseInitially) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);

    EXPECT_FALSE(window.ShouldClose());
}

TEST_F(WindowTest, GetFramebufferSize) {
    bimeup::platform::WindowConfig config;
    config.width = 800;
    config.height = 600;
    config.visible = false;
    bimeup::platform::Window window(config);

    auto fbSize = window.GetFramebufferSize();
    EXPECT_GT(fbSize.x, 0);
    EXPECT_GT(fbSize.y, 0);
}

TEST_F(WindowTest, SetTitle) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);

    // Should not throw
    window.SetTitle("New Title");
}
