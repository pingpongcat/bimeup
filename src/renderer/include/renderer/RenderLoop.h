#pragma once

#include <vulkan/vulkan.h>

#include <array>

namespace bimeup::renderer {

class Device;
class Swapchain;

class RenderLoop {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    RenderLoop(const Device& device, Swapchain& swapchain);
    ~RenderLoop();

    RenderLoop(const RenderLoop&) = delete;
    RenderLoop& operator=(const RenderLoop&) = delete;
    RenderLoop(RenderLoop&&) = delete;
    RenderLoop& operator=(RenderLoop&&) = delete;

    [[nodiscard]] bool BeginFrame();
    [[nodiscard]] bool EndFrame();

    void WaitIdle();

    void SetClearColor(float r, float g, float b, float a = 1.0f);

    [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const;
    [[nodiscard]] uint32_t GetCurrentFrameIndex() const;
    [[nodiscard]] uint32_t GetCurrentImageIndex() const;

private:
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void Cleanup();

    static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                      VkImageLayout oldLayout, VkImageLayout newLayout);

    const Device& m_device;
    Swapchain& m_swapchain;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_commandBuffers{};

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_renderFinishedSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

    uint32_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    VkClearColorValue m_clearColor = {{0.1f, 0.1f, 0.1f, 1.0f}};
    bool m_frameStarted = false;
};

}  // namespace bimeup::renderer
