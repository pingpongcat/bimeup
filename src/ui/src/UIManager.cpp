#include <ui/UIManager.h>

#include <ui/Panel.h>

#include <imgui.h>

#include <utility>

namespace bimeup::ui {

UIManager::UIManager() = default;

UIManager::~UIManager() = default;

void UIManager::InitVulkanBackend(const VulkanBackendInfo& info) {
    m_context.InitVulkanBackend(info);
}

void UIManager::ShutdownVulkanBackend() {
    m_context.ShutdownVulkanBackend();
}

void UIManager::AddPanel(std::unique_ptr<Panel> panel) {
    if (!panel) {
        return;
    }
    m_panels.push_back(std::move(panel));
}

std::size_t UIManager::PanelCount() const {
    return m_panels.size();
}

void UIManager::BeginFrame() {
    if (!m_context.HasVulkanBackend()) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.DisplaySize.x <= 0.0F || io.DisplaySize.y <= 0.0F) {
            io.DisplaySize = ImVec2(1.0F, 1.0F);
        }
        if (!io.Fonts->IsBuilt()) {
            unsigned char* pixels = nullptr;
            int width = 0;
            int height = 0;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            io.Fonts->SetTexID(static_cast<ImTextureID>(1));
        }
    }
    m_context.BeginFrame();
    for (auto& panel : m_panels) {
        panel->OnDraw();
    }
}

void UIManager::EndFrame(VkCommandBuffer commandBuffer) {
    m_context.EndFrame(commandBuffer);
}

void UIManager::SetMinImageCount(uint32_t minImageCount) {
    m_context.SetMinImageCount(minImageCount);
}

ImGuiContext& UIManager::GetContext() {
    return m_context;
}

const ImGuiContext& UIManager::GetContext() const {
    return m_context;
}

}  // namespace bimeup::ui
