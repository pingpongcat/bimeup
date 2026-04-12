#pragma once

#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/vec2.hpp>

namespace bimeup::platform {

struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string title = "Bimeup";
    bool resizable = true;
    bool visible = true;
};

class Window {
public:
    static void InitGlfw();
    static void TerminateGlfw();

    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool ShouldClose() const;
    void PollEvents();
    [[nodiscard]] glm::ivec2 GetSize() const;
    [[nodiscard]] glm::ivec2 GetFramebufferSize() const;
    void SetTitle(const std::string& title);
    [[nodiscard]] GLFWwindow* GetHandle() const;

private:
    GLFWwindow* m_window = nullptr;
};

}  // namespace bimeup::platform
