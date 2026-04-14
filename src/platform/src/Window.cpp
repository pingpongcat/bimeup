#include <platform/Window.h>

#include <stdexcept>

namespace bimeup::platform {

void Window::InitGlfw() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // No OpenGL — we use Vulkan
}

void Window::TerminateGlfw() {
    glfwTerminate();
}

Window::Window(const WindowConfig& config) {
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (!m_window) {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::PollEvents() {
    glfwPollEvents();
}

glm::ivec2 Window::GetSize() const {
    int w, h;
    glfwGetWindowSize(m_window, &w, &h);
    return {w, h};
}

glm::ivec2 Window::GetFramebufferSize() const {
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    return {w, h};
}

void Window::SetTitle(const std::string& title) {
    glfwSetWindowTitle(m_window, title.c_str());
}

GLFWwindow* Window::GetHandle() const {
    return m_window;
}

}  // namespace bimeup::platform
