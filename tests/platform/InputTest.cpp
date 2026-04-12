#include <gtest/gtest.h>
#include <platform/Input.h>

class InputTest : public ::testing::Test {
protected:
    void SetUp() override {
        bimeup::platform::Window::InitGlfw();
    }

    void TearDown() override {
        bimeup::platform::Window::TerminateGlfw();
    }

    // Helper: retrieve the GLFW callback that Input registered, then restore it.
    // glfwSetKeyCallback(handle, nullptr) returns the old callback and clears it,
    // so we immediately restore it.
    static GLFWkeyfun GetKeyCallback(GLFWwindow* handle) {
        GLFWkeyfun cb = glfwSetKeyCallback(handle, nullptr);
        glfwSetKeyCallback(handle, cb);
        return cb;
    }

    static GLFWcursorposfun GetCursorPosCallback(GLFWwindow* handle) {
        GLFWcursorposfun cb = glfwSetCursorPosCallback(handle, nullptr);
        glfwSetCursorPosCallback(handle, cb);
        return cb;
    }

    static GLFWmousebuttonfun GetMouseButtonCallback(GLFWwindow* handle) {
        GLFWmousebuttonfun cb = glfwSetMouseButtonCallback(handle, nullptr);
        glfwSetMouseButtonCallback(handle, cb);
        return cb;
    }

    static GLFWscrollfun GetScrollCallback(GLFWwindow* handle) {
        GLFWscrollfun cb = glfwSetScrollCallback(handle, nullptr);
        glfwSetScrollCallback(handle, cb);
        return cb;
    }
};

TEST_F(InputTest, KeyCallbackFiresOnKeyPress) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    bool fired = false;
    bimeup::platform::Key receivedKey{};
    bool receivedPressed = false;

    input.OnKey([&](bimeup::platform::Key key, bool pressed) {
        fired = true;
        receivedKey = key;
        receivedPressed = pressed;
    });

    auto* handle = window.GetHandle();
    auto keyCb = GetKeyCallback(handle);
    ASSERT_NE(keyCb, nullptr);
    keyCb(handle, GLFW_KEY_A, 0, GLFW_PRESS, 0);

    EXPECT_TRUE(fired);
    EXPECT_EQ(receivedKey, bimeup::platform::Key::A);
    EXPECT_TRUE(receivedPressed);
}

TEST_F(InputTest, KeyCallbackFiresOnKeyRelease) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    bool receivedPressed = true;

    input.OnKey([&](bimeup::platform::Key /*key*/, bool pressed) {
        receivedPressed = pressed;
    });

    auto* handle = window.GetHandle();
    auto keyCb = GetKeyCallback(handle);
    keyCb(handle, GLFW_KEY_W, 0, GLFW_RELEASE, 0);

    EXPECT_FALSE(receivedPressed);
}

TEST_F(InputTest, MouseMoveCallbackFires) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    bool fired = false;
    double rx = 0.0, ry = 0.0;

    input.OnMouseMove([&](double x, double y) {
        fired = true;
        rx = x;
        ry = y;
    });

    auto* handle = window.GetHandle();
    auto cursorCb = GetCursorPosCallback(handle);
    ASSERT_NE(cursorCb, nullptr);
    cursorCb(handle, 100.5, 200.5);

    EXPECT_TRUE(fired);
    EXPECT_DOUBLE_EQ(rx, 100.5);
    EXPECT_DOUBLE_EQ(ry, 200.5);
}

TEST_F(InputTest, MouseButtonCallbackFires) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    bool fired = false;
    bimeup::platform::MouseButton receivedButton{};
    bool receivedPressed = false;

    input.OnMouseButton([&](bimeup::platform::MouseButton button, bool pressed) {
        fired = true;
        receivedButton = button;
        receivedPressed = pressed;
    });

    auto* handle = window.GetHandle();
    auto mbCb = GetMouseButtonCallback(handle);
    ASSERT_NE(mbCb, nullptr);
    mbCb(handle, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);

    EXPECT_TRUE(fired);
    EXPECT_EQ(receivedButton, bimeup::platform::MouseButton::Left);
    EXPECT_TRUE(receivedPressed);
}

TEST_F(InputTest, ScrollCallbackFires) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    bool fired = false;
    double rxOff = 0.0, ryOff = 0.0;

    input.OnScroll([&](double xOffset, double yOffset) {
        fired = true;
        rxOff = xOffset;
        ryOff = yOffset;
    });

    auto* handle = window.GetHandle();
    auto scrollCb = GetScrollCallback(handle);
    ASSERT_NE(scrollCb, nullptr);
    scrollCb(handle, 0.0, 3.0);

    EXPECT_TRUE(fired);
    EXPECT_DOUBLE_EQ(rxOff, 0.0);
    EXPECT_DOUBLE_EQ(ryOff, 3.0);
}

TEST_F(InputTest, IsKeyDownTracksState) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    EXPECT_FALSE(input.IsKeyDown(bimeup::platform::Key::Space));

    auto* handle = window.GetHandle();
    auto keyCb = GetKeyCallback(handle);

    keyCb(handle, GLFW_KEY_SPACE, 0, GLFW_PRESS, 0);
    EXPECT_TRUE(input.IsKeyDown(bimeup::platform::Key::Space));

    keyCb(handle, GLFW_KEY_SPACE, 0, GLFW_RELEASE, 0);
    EXPECT_FALSE(input.IsKeyDown(bimeup::platform::Key::Space));
}

TEST_F(InputTest, GetMousePositionUpdatesOnCursorMove) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    auto pos = input.GetMousePosition();
    EXPECT_DOUBLE_EQ(pos.x, 0.0);
    EXPECT_DOUBLE_EQ(pos.y, 0.0);

    auto* handle = window.GetHandle();
    auto cursorCb = GetCursorPosCallback(handle);
    cursorCb(handle, 42.0, 84.0);

    pos = input.GetMousePosition();
    EXPECT_DOUBLE_EQ(pos.x, 42.0);
    EXPECT_DOUBLE_EQ(pos.y, 84.0);
}

TEST_F(InputTest, MultipleKeyCallbacksAllFire) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    int fireCount = 0;
    input.OnKey([&](bimeup::platform::Key, bool) { ++fireCount; });
    input.OnKey([&](bimeup::platform::Key, bool) { ++fireCount; });

    auto* handle = window.GetHandle();
    auto keyCb = GetKeyCallback(handle);
    keyCb(handle, GLFW_KEY_D, 0, GLFW_PRESS, 0);

    EXPECT_EQ(fireCount, 2);
}

TEST_F(InputTest, KeyRepeatIgnored) {
    bimeup::platform::WindowConfig config;
    config.visible = false;
    bimeup::platform::Window window(config);
    bimeup::platform::Input input(window);

    int fireCount = 0;
    input.OnKey([&](bimeup::platform::Key, bool) { ++fireCount; });

    auto* handle = window.GetHandle();
    auto keyCb = GetKeyCallback(handle);
    keyCb(handle, GLFW_KEY_A, 0, GLFW_REPEAT, 0);

    EXPECT_EQ(fireCount, 0);
}
