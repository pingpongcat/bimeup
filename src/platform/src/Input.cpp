#include <platform/Input.h>

namespace bimeup::platform {

Input::Input(Window& window) : m_window(window) {
    auto* handle = m_window.GetHandle();
    glfwSetWindowUserPointer(handle, this);
    glfwSetKeyCallback(handle, KeyCallbackGlfw);
    glfwSetCursorPosCallback(handle, CursorPosCallbackGlfw);
    glfwSetMouseButtonCallback(handle, MouseButtonCallbackGlfw);
    glfwSetScrollCallback(handle, ScrollCallbackGlfw);
}

Input::~Input() {
    auto* handle = m_window.GetHandle();
    glfwSetKeyCallback(handle, nullptr);
    glfwSetCursorPosCallback(handle, nullptr);
    glfwSetMouseButtonCallback(handle, nullptr);
    glfwSetScrollCallback(handle, nullptr);
    glfwSetWindowUserPointer(handle, nullptr);
}

void Input::OnKey(KeyCallback cb) {
    m_keyCallbacks.push_back(std::move(cb));
}

void Input::OnMouseMove(MouseMoveCallback cb) {
    m_mouseMoveCallbacks.push_back(std::move(cb));
}

void Input::OnMouseButton(MouseButtonCallback cb) {
    m_mouseButtonCallbacks.push_back(std::move(cb));
}

void Input::OnScroll(ScrollCallback cb) {
    m_scrollCallbacks.push_back(std::move(cb));
}

bool Input::IsKeyDown(Key key) const {
    auto it = m_keyStates.find(static_cast<int>(key));
    return it != m_keyStates.end() && it->second;
}

glm::dvec2 Input::GetMousePosition() const {
    return m_mousePosition;
}

void Input::KeyCallbackGlfw(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    if (action == GLFW_REPEAT) {
        return;
    }

    auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input) return;

    bool pressed = (action == GLFW_PRESS);
    auto mappedKey = static_cast<Key>(key);

    input->m_keyStates[key] = pressed;

    for (auto& cb : input->m_keyCallbacks) {
        cb(mappedKey, pressed);
    }
}

void Input::CursorPosCallbackGlfw(GLFWwindow* window, double xpos, double ypos) {
    auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input) return;

    input->m_mousePosition = {xpos, ypos};

    for (auto& cb : input->m_mouseMoveCallbacks) {
        cb(xpos, ypos);
    }
}

void Input::MouseButtonCallbackGlfw(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input) return;

    bool pressed = (action == GLFW_PRESS);
    auto mappedButton = static_cast<MouseButton>(button);

    for (auto& cb : input->m_mouseButtonCallbacks) {
        cb(mappedButton, pressed);
    }
}

void Input::ScrollCallbackGlfw(GLFWwindow* window, double xoffset, double yoffset) {
    auto* input = static_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!input) return;

    for (auto& cb : input->m_scrollCallbacks) {
        cb(xoffset, yoffset);
    }
}

}  // namespace bimeup::platform
