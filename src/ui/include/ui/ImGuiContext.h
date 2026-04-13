#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace bimeup::ui {

struct VulkanBackendInfo {
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint32_t minImageCount = 2;
    uint32_t imageCount = 2;
    uint32_t apiVersion = VK_API_VERSION_1_0;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
};

class ImGuiContext {
public:
    ImGuiContext();
    ~ImGuiContext();

    ImGuiContext(const ImGuiContext&) = delete;
    ImGuiContext& operator=(const ImGuiContext&) = delete;
    ImGuiContext(ImGuiContext&&) = delete;
    ImGuiContext& operator=(ImGuiContext&&) = delete;

    void InitVulkanBackend(const VulkanBackendInfo& info);
    void ShutdownVulkanBackend();

    void BeginFrame();
    void EndFrame(VkCommandBuffer commandBuffer);

    void SetMinImageCount(uint32_t minImageCount);

    [[nodiscard]] bool HasVulkanBackend() const { return m_backendInitialized; }

private:
    bool m_backendInitialized = false;
    bool m_frameStarted = false;
};

}  // namespace bimeup::ui
