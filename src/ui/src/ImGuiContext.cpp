#include <ui/ImGuiContext.h>

#include <ui/Theme.h>
#include <tools/Log.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <stdexcept>

namespace bimeup::ui {

namespace {

void CheckVkResult(VkResult result) {
    if (result != VK_SUCCESS) {
        LOG_ERROR("ImGui Vulkan backend error: {}", static_cast<int>(result));
    }
}

}  // namespace

ImGuiContext::ImGuiContext() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    Theme::Apply();
}

ImGuiContext::~ImGuiContext() {
    if (m_backendInitialized) {
        ShutdownVulkanBackend();
    }
    ImGui::DestroyContext();
}

void ImGuiContext::InitVulkanBackend(const VulkanBackendInfo& info) {
    if (m_backendInitialized) {
        throw std::runtime_error("ImGui Vulkan backend already initialized");
    }

    if (!ImGui_ImplGlfw_InitForVulkan(info.window, /*install_callbacks=*/true)) {
        throw std::runtime_error("ImGui_ImplGlfw_InitForVulkan failed");
    }

    ImGui_ImplVulkan_InitInfo init{};
    init.ApiVersion = info.apiVersion;
    init.Instance = info.instance;
    init.PhysicalDevice = info.physicalDevice;
    init.Device = info.device;
    init.QueueFamily = info.queueFamily;
    init.Queue = info.queue;
    init.DescriptorPoolSize = 64;
    init.MinImageCount = info.minImageCount;
    init.ImageCount = info.imageCount;
    init.PipelineInfoMain.RenderPass = info.renderPass;
    init.PipelineInfoMain.Subpass = 0;
    init.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init.CheckVkResultFn = CheckVkResult;

    if (!ImGui_ImplVulkan_Init(&init)) {
        ImGui_ImplGlfw_Shutdown();
        throw std::runtime_error("ImGui_ImplVulkan_Init failed");
    }

    m_backendInitialized = true;
}

void ImGuiContext::ShutdownVulkanBackend() {
    if (!m_backendInitialized) {
        return;
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    m_backendInitialized = false;
}

void ImGuiContext::BeginFrame() {
    if (m_backendInitialized) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }
    ImGui::NewFrame();
    m_frameStarted = true;
}

void ImGuiContext::EndFrame(VkCommandBuffer commandBuffer) {
    if (!m_frameStarted) {
        return;
    }
    ImGui::Render();
    if (m_backendInitialized && commandBuffer != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }
    m_frameStarted = false;
}

void ImGuiContext::SetMinImageCount(uint32_t minImageCount) {
    if (m_backendInitialized) {
        ImGui_ImplVulkan_SetMinImageCount(minImageCount);
    }
}

}  // namespace bimeup::ui
