#include <gtest/gtest.h>
#include <renderer/RenderLoop.h>
#include <renderer/Swapchain.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

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
