#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bimeup::renderer {

class Device;
class Swapchain;
class Shader;
class TonemapPipeline;

class RenderLoop {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    // HDR offscreen format used between the scene render pass and the tonemap
    // resolve pass. Matches the Stage RP plan — enough dynamic range to keep
    // bright lighting / future SSIL bounces from clipping before the curve.
    static constexpr VkFormat HDR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

    RenderLoop(const Device& device, Swapchain& swapchain,
               const std::string& shaderDir,
               VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    ~RenderLoop();

    RenderLoop(const RenderLoop&) = delete;
    RenderLoop& operator=(const RenderLoop&) = delete;
    RenderLoop(RenderLoop&&) = delete;
    RenderLoop& operator=(RenderLoop&&) = delete;

    [[nodiscard]] bool BeginFrame();
    [[nodiscard]] bool EndFrame();

    void WaitIdle();

    void SetClearColor(float r, float g, float b, float a = 1.0f);

    /// Register a callback invoked inside BeginFrame() after the command buffer is
    /// begun but BEFORE the main scene render pass starts — suitable for
    /// recording a depth-only shadow pass whose output is sampled by the main pass.
    using PreMainPassCallback = std::function<void(VkCommandBuffer)>;
    void SetPreMainPassCallback(PreMainPassCallback callback);

    /// Register a callback invoked while the tonemap/present pass is active,
    /// AFTER the internal fullscreen tonemap draw has been recorded. This is
    /// where UI (ImGui) draw data is emitted so it lands on top of the
    /// tonemapped image instead of being fed through the ACES curve twice.
    using InPresentPassCallback = std::function<void(VkCommandBuffer)>;
    void SetInPresentPassCallback(InPresentPassCallback callback);

    [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const;
    [[nodiscard]] uint32_t GetCurrentFrameIndex() const;
    [[nodiscard]] uint32_t GetCurrentImageIndex() const;
    /// Render pass for scene geometry — HDR colour (+ depth, + optional MSAA
    /// resolve) targeting the offscreen HDR image. Bind scene pipelines here.
    [[nodiscard]] VkRenderPass GetRenderPass() const;
    [[nodiscard]] VkSampleCountFlagBits GetSampleCount() const;
    /// Render pass for the final HDR→LDR tonemap + UI overlay targeting the
    /// swapchain. Bind ImGui and any other post-tonemap pipelines here;
    /// sample count is always 1× (the swapchain is never multisampled).
    [[nodiscard]] VkRenderPass GetPresentRenderPass() const;
    [[nodiscard]] VkSampleCountFlagBits GetPresentSampleCount() const {
        return VK_SAMPLE_COUNT_1_BIT;
    }

    /// Change the MSAA sample count. Waits for GPU idle, tears down the main
    /// render pass / HDR & depth attachments / framebuffers, and rebuilds them.
    /// Both render pass handles are recreated — any pipeline bound to the
    /// previous passes must be rebuilt by the caller before the next BeginFrame().
    void SetSampleCount(VkSampleCountFlagBits samples);

    /// Rebuild HDR/depth/MSAA images, framebuffers, and tonemap descriptor
    /// bindings to match the current swapchain extent. Call after
    /// `Swapchain::Recreate(...)` on a resize. The render pass handles are
    /// preserved (format/samples unchanged) so pipelines stay valid.
    void RecreateForSwapchain();

private:
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CreateRenderPass();
    void CreatePresentRenderPass();
    void CreateHdrResources();
    void CreateDepthResources();
    void CreateFramebuffers();
    void CreateTonemapPipeline();
    void CreateTonemapDescriptors();
    void UpdateTonemapDescriptors();
    void CleanupFrameResources();
    void CleanupTonemapDescriptors();
    void Cleanup();

    static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);

    const Device& m_device;
    Swapchain& m_swapchain;
    std::string m_shaderDir;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_commandBuffers{};

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
    // One render-finished semaphore per swapchain image (presentation engine may still hold the
    // previous one until that image is re-acquired — reusing a per-frame semaphore triggers
    // VUID-vkQueueSubmit-pSignalSemaphores-00067).
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkRenderPass m_presentRenderPass = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    // Multisampled HDR colour attachment (only used when m_samples > 1x; resolved into the
    // single-sample HDR image each frame).
    VkImage m_colorImage = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;
    VmaAllocation m_colorAllocation = VK_NULL_HANDLE;
    // Per-swapchain-image HDR colour target (R16G16B16A16_SFLOAT). Written by the main pass,
    // sampled by the tonemap pass.
    std::vector<VkImage> m_hdrImages;
    std::vector<VkImageView> m_hdrImageViews;
    std::vector<VmaAllocation> m_hdrAllocations;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkFramebuffer> m_presentFramebuffers;

    // Tonemap pass: shaders loaded once in the ctor, pipeline rebuilt on MSAA/size changes.
    // Descriptor set layout + pool built once; descriptor sets recreated on swapchain resize
    // so they point at the fresh per-image HDR views.
    std::unique_ptr<Shader> m_tonemapVert;
    std::unique_ptr<Shader> m_tonemapFrag;
    std::unique_ptr<TonemapPipeline> m_tonemapPipeline;
    VkSampler m_hdrSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_tonemapSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_tonemapDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_tonemapDescriptorSets;

    uint32_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    VkClearColorValue m_clearColor = {{0.1f, 0.1f, 0.1f, 1.0f}};
    bool m_frameStarted = false;

    PreMainPassCallback m_preMainPass;
    InPresentPassCallback m_inPresentPass;
};

}  // namespace bimeup::renderer
