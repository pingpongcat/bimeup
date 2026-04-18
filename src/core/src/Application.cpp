#include <vulkan/vulkan.h>

#include <core/Application.h>

#include <platform/Input.h>
#include <renderer/Device.h>
#include <renderer/RenderLoop.h>
#include <renderer/Swapchain.h>
#include <renderer/VulkanContext.h>
#include <tools/Log.h>

#include <GLFW/glfw3.h>

#include <span>
#include <stdexcept>

namespace bimeup::core {

Application::Application(const AppConfig& config) {
    m_window = std::make_unique<platform::Window>(config.window);
    m_input = std::make_unique<platform::Input>(*m_window);

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
    m_vulkanContext = std::make_unique<renderer::VulkanContext>(true, requiredExts);

    if (glfwCreateWindowSurface(m_vulkanContext->GetInstance(), m_window->GetHandle(), nullptr,
                                &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    m_device = std::make_unique<renderer::Device>(m_vulkanContext->GetInstance(), m_surface);

    auto fbSize = m_window->GetFramebufferSize();
    m_swapchain = std::make_unique<renderer::Swapchain>(
        *m_device, m_surface,
        VkExtent2D{static_cast<uint32_t>(fbSize.x), static_cast<uint32_t>(fbSize.y)});

    m_renderLoop = std::make_unique<renderer::RenderLoop>(*m_device, *m_swapchain,
                                                           BIMEUP_SHADER_DIR);
}

Application::~Application() {
    if (m_renderLoop) {
        m_renderLoop->WaitIdle();
    }
    m_renderLoop.reset();
    m_swapchain.reset();
    m_device.reset();
    if (m_surface != VK_NULL_HANDLE && m_vulkanContext) {
        vkDestroySurfaceKHR(m_vulkanContext->GetInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_vulkanContext.reset();
    m_input.reset();
    m_window.reset();
}

void Application::Run() {
    while (RunOneFrame()) {
    }
}

bool Application::RunOneFrame() {
    if (m_shutdownRequested || m_window->ShouldClose()) {
        return false;
    }

    m_window->PollEvents();

    if (m_renderLoop->BeginFrame()) {
        (void)m_renderLoop->EndFrame();
    }

    return !m_shutdownRequested && !m_window->ShouldClose();
}

void Application::RequestShutdown() {
    m_shutdownRequested = true;
}

platform::Window& Application::GetWindow() {
    return *m_window;
}

platform::Input& Application::GetInput() {
    return *m_input;
}

renderer::Device& Application::GetDevice() {
    return *m_device;
}

renderer::Swapchain& Application::GetSwapchain() {
    return *m_swapchain;
}

renderer::RenderLoop& Application::GetRenderLoop() {
    return *m_renderLoop;
}

}  // namespace bimeup::core
