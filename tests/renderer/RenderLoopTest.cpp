#include <gtest/gtest.h>
#include <renderer/OutlinePipeline.h>
#include <renderer/RenderLoop.h>
#include <renderer/Swapchain.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <glm/gtc/matrix_transform.hpp>

#include <span>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using bimeup::renderer::Device;
using bimeup::renderer::RenderLoop;
using bimeup::renderer::Swapchain;
using bimeup::renderer::VulkanContext;

class RenderLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_window = glfwCreateWindow(800, 600, "RenderLoopTest", nullptr, nullptr);
        ASSERT_NE(m_window, nullptr);

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
        m_context = std::make_unique<VulkanContext>(true, requiredExts);

        VkResult result = glfwCreateWindowSurface(
            m_context->GetInstance(), m_window, nullptr, &m_surface);
        ASSERT_EQ(result, VK_SUCCESS);

        m_device = std::make_unique<Device>(m_context->GetInstance(), m_surface);
        m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});
    }

    void TearDown() override {
        m_renderLoop.reset();
        m_swapchain.reset();
        m_device.reset();
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
        }
        m_context.reset();
        if (m_window != nullptr) {
            glfwDestroyWindow(m_window);
        }
        glfwTerminate();
    }

    GLFWwindow* m_window = nullptr;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<Swapchain> m_swapchain;
    std::unique_ptr<RenderLoop> m_renderLoop;
};

TEST_F(RenderLoopTest, CreatesSuccessfully) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
}

TEST_F(RenderLoopTest, SingleFrameCycle) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);

    EXPECT_NE(m_renderLoop->GetCurrentCommandBuffer(), VK_NULL_HANDLE);

    bool ended = m_renderLoop->EndFrame();
    EXPECT_TRUE(ended);

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, MultipleFrames) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    for (int i = 0; i < 5; ++i) {
        bool began = m_renderLoop->BeginFrame();
        ASSERT_TRUE(began);
        bool ended = m_renderLoop->EndFrame();
        EXPECT_TRUE(ended);
    }

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, FrameIndexCycles) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    for (uint32_t i = 0; i < 4; ++i) {
        EXPECT_EQ(m_renderLoop->GetCurrentFrameIndex(),
                  i % RenderLoop::MAX_FRAMES_IN_FLIGHT);

        bool began = m_renderLoop->BeginFrame();
        ASSERT_TRUE(began);
        EXPECT_TRUE(m_renderLoop->EndFrame());
    }

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, PreMainPassCallbackInvokedEachFrame) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    int calls = 0;
    VkCommandBuffer capturedCmd = VK_NULL_HANDLE;
    m_renderLoop->SetPreMainPassCallback([&](VkCommandBuffer cmd) {
        ++calls;
        capturedCmd = cmd;
    });

    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);
    // Callback must have run during BeginFrame with the frame's active command buffer.
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(capturedCmd, m_renderLoop->GetCurrentCommandBuffer());

    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, SetClearColor) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    m_renderLoop->SetClearColor(1.0f, 0.0f, 0.0f);

    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);
    bool ended = m_renderLoop->EndFrame();
    EXPECT_TRUE(ended);

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, DestructorCleansUp) {
    {
        RenderLoop loop(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
        bool began = loop.BeginFrame();
        ASSERT_TRUE(began);
        EXPECT_TRUE(loop.EndFrame());
        loop.WaitIdle();
    }
    // Validation layers + sanitizers would catch leaks
}

// RP.3c — MRT normal G-buffer: the main render pass has a second R16G16_SNORM
// colour attachment per swap image, rebuilt in lockstep with the HDR target.
TEST_F(RenderLoopTest, NormalFormatIsR16G16Snorm) {
    EXPECT_EQ(RenderLoop::NORMAL_FORMAT, VK_FORMAT_R16G16_SNORM);
}

TEST_F(RenderLoopTest, NormalGBufferImageViewsProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetNormalImageView(i), VK_NULL_HANDLE)
            << "normal G-buffer view missing for swap image " << i;
    }
}

TEST_F(RenderLoopTest, NormalGBufferSurvivesSampleCountChange) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetSampleCount(VK_SAMPLE_COUNT_4_BIT);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetNormalImageView(i), VK_NULL_HANDLE);
    }
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// RP.6c — MRT outline stencil G-buffer: the main render pass has a third
// R8_UINT colour attachment per swap image (0 = background, 1 = selected,
// 2 = hovered). Sampled by the RP.6b outline fragment shader. Layout mirrors
// the normal G-buffer — per-swap-image single-sample target + a transient
// MSAA attachment when m_samples > 1x.
TEST_F(RenderLoopTest, StencilFormatIsR8Uint) {
    EXPECT_EQ(RenderLoop::STENCIL_FORMAT, VK_FORMAT_R8_UINT);
}

TEST_F(RenderLoopTest, StencilGBufferImageViewsProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetStencilImageView(i), VK_NULL_HANDLE)
            << "stencil G-buffer view missing for swap image " << i;
    }
}

TEST_F(RenderLoopTest, StencilGBufferSurvivesSampleCountChange) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetSampleCount(VK_SAMPLE_COUNT_4_BIT);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetStencilImageView(i), VK_NULL_HANDLE);
    }
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// RP.4d — Linear depth + depth pyramid: a 4-mip R32_SFLOAT pyramid per swap
// image, built by a compute pass between the main HDR pass and the tonemap
// pass. MSAA path gates off for now (shader needs sampler2DMS; can be added
// when SSAO starts sampling the pyramid).
TEST_F(RenderLoopTest, DepthPyramidFormatIsR32Sfloat) {
    EXPECT_EQ(RenderLoop::DEPTH_PYRAMID_FORMAT, VK_FORMAT_R32_SFLOAT);
}

TEST_F(RenderLoopTest, DepthPyramidMipCountIsFour) {
    EXPECT_EQ(RenderLoop::DEPTH_PYRAMID_MIPS, 4u);
}

TEST_F(RenderLoopTest, DepthPyramidViewProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetDepthPyramidView(i), VK_NULL_HANDLE)
            << "depth pyramid view missing for swap image " << i;
    }
}

TEST_F(RenderLoopTest, DepthPyramidBuiltDuringFrame) {
    // Exercises the full per-frame compute dispatch (linearize + 3 mip levels).
    // Validation layers + sanitizers guard barrier/sync mistakes.
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, DepthPyramidSurvivesSampleCountChange) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetSampleCount(VK_SAMPLE_COUNT_4_BIT);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetDepthPyramidView(i), VK_NULL_HANDLE);
    }
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// RP.5d — SSAO compute pass wired into RenderLoop: per-swap-image half-res
// R8 AO target, main + 2 blur dispatches between the depth-pyramid build
// and the tonemap pass, AO sampled by tonemap.frag and multiplied into the
// HDR colour. MSAA path gates off (inherits the depth pyramid gate — no
// pyramid means no SSAO inputs); AO image is cleared to 1.0 at creation so
// the tonemap multiply stays a no-op when SSAO is skipped.
TEST_F(RenderLoopTest, AoFormatIsR8Unorm) {
    EXPECT_EQ(RenderLoop::AO_FORMAT, VK_FORMAT_R8_UNORM);
}

TEST_F(RenderLoopTest, AoViewProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetAoImageView(i), VK_NULL_HANDLE)
            << "AO view missing for swap image " << i;
    }
}

TEST_F(RenderLoopTest, SsaoDispatchedDuringFrame) {
    // Exercises the full per-frame compute chain (pyramid + SSAO main +
    // 2 blur passes) under Vulkan validation.
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, SsaoSurvivesSampleCountChange) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetSampleCount(VK_SAMPLE_COUNT_4_BIT);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetAoImageView(i), VK_NULL_HANDLE);
    }
    // SSAO is gated off under MSAA (mirrors the depth pyramid gate). Frame
    // cycle must still succeed — tonemap samples the pre-cleared AO image.
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

namespace {

bimeup::renderer::OutlinePipeline::PushConstants MakeDefaultOutlinePush() {
    bimeup::renderer::OutlinePipeline::PushConstants pc{};
    pc.selectedColor = glm::vec4(1.0F, 0.6F, 0.1F, 1.0F);
    pc.hoverColor = glm::vec4(0.2F, 0.7F, 1.0F, 0.8F);
    pc.texelSize = glm::vec2(1.0F / 800.0F, 1.0F / 600.0F);
    pc.thickness = 2.0F;
    pc.depthEdgeThreshold = 0.05F;
    return pc;
}

}  // namespace

// RP.6d — OutlinePipeline + descriptor sets are owned by RenderLoop and
// recorded into the present pass after the tonemap fullscreen draw. Push
// constants and the enable toggle come from the panel via SetOutlineParams.
// The outline pass samples the stencil G-buffer (RP.6c) and the depth
// pyramid mip 0 (RP.4d), so it inherits the depth-pyramid MSAA gate — under
// MSAA the dispatch is skipped and the frame cycles without the outline.
TEST_F(RenderLoopTest, OutlineDrawnDuringFrame) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    m_renderLoop->SetOutlineParams(MakeDefaultOutlinePush(), /*enabled=*/true);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, OutlineDisabledStillCyclesFrame) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    m_renderLoop->SetOutlineParams(MakeDefaultOutlinePush(), /*enabled=*/false);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, OutlineSurvivesSampleCountChange) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetSampleCount(VK_SAMPLE_COUNT_4_BIT);
    m_renderLoop->SetOutlineParams(MakeDefaultOutlinePush(), /*enabled=*/true);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}
