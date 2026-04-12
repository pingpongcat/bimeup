#pragma once

#include <platform/Window.h>

#include <functional>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

namespace bimeup::platform {

// Values match GLFW key codes for zero-cost conversion
enum class Key : int {
    Unknown = -1,
    Space = 32,
    D0 = 48, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
};

enum class MouseButton : int {
    Left = 0,
    Right = 1,
    Middle = 2,
};

using KeyCallback = std::function<void(Key, bool pressed)>;
using MouseMoveCallback = std::function<void(double x, double y)>;
using MouseButtonCallback = std::function<void(MouseButton, bool pressed)>;
using ScrollCallback = std::function<void(double xOffset, double yOffset)>;

class Input {
public:
    explicit Input(Window& window);
    ~Input();

    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;

    void OnKey(KeyCallback cb);
    void OnMouseMove(MouseMoveCallback cb);
    void OnMouseButton(MouseButtonCallback cb);
    void OnScroll(ScrollCallback cb);

    [[nodiscard]] bool IsKeyDown(Key key) const;
    [[nodiscard]] glm::dvec2 GetMousePosition() const;

private:
    Window& m_window;

    std::vector<KeyCallback> m_keyCallbacks;
    std::vector<MouseMoveCallback> m_mouseMoveCallbacks;
    std::vector<MouseButtonCallback> m_mouseButtonCallbacks;
    std::vector<ScrollCallback> m_scrollCallbacks;

    std::unordered_map<int, bool> m_keyStates;
    glm::dvec2 m_mousePosition{0.0, 0.0};

    static void KeyCallbackGlfw(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CursorPosCallbackGlfw(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallbackGlfw(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallbackGlfw(GLFWwindow* window, double xoffset, double yoffset);
};

}  // namespace bimeup::platform
