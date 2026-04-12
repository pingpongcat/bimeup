#pragma once

#include <platform/Window.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

namespace bimeup::platform {
class Input;
}

namespace bimeup::renderer {
class VulkanContext;
class Device;
class Swapchain;
class RenderLoop;
}

namespace bimeup::core {

struct AppConfig {
    platform::WindowConfig window;
    bool enableVR = false;
    bool enableRayTracing = false;
    std::string configPath;
};

class Application {
public:
    explicit Application(const AppConfig& config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void Run();
    bool RunOneFrame();
    void RequestShutdown();

    [[nodiscard]] platform::Window& GetWindow();
    [[nodiscard]] platform::Input& GetInput();
    [[nodiscard]] renderer::Device& GetDevice();
    [[nodiscard]] renderer::Swapchain& GetSwapchain();
    [[nodiscard]] renderer::RenderLoop& GetRenderLoop();

private:
    bool m_shutdownRequested = false;

    std::unique_ptr<platform::Window> m_window;
    std::unique_ptr<platform::Input> m_input;
    std::unique_ptr<renderer::VulkanContext> m_vulkanContext;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<renderer::Device> m_device;
    std::unique_ptr<renderer::Swapchain> m_swapchain;
    std::unique_ptr<renderer::RenderLoop> m_renderLoop;
};

}  // namespace bimeup::core
