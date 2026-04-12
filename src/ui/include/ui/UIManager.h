#pragma once

#include <ui/ImGuiContext.h>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace bimeup::ui {

class Panel;

class UIManager {
public:
    UIManager();
    ~UIManager();

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager(UIManager&&) = delete;
    UIManager& operator=(UIManager&&) = delete;

    void InitVulkanBackend(const VulkanBackendInfo& info);
    void ShutdownVulkanBackend();

    void AddPanel(std::unique_ptr<Panel> panel);
    [[nodiscard]] std::size_t PanelCount() const;

    void BeginFrame();
    void EndFrame(VkCommandBuffer commandBuffer);

    void SetMinImageCount(uint32_t minImageCount);

    [[nodiscard]] ImGuiContext& GetContext();
    [[nodiscard]] const ImGuiContext& GetContext() const;

private:
    ImGuiContext m_context;
    std::vector<std::unique_ptr<Panel>> m_panels;
};

}  // namespace bimeup::ui
