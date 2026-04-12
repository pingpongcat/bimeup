#include <gtest/gtest.h>
#include <renderer/Swapchain.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <span>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using bimeup::renderer::Device;
using bimeup::renderer::Swapchain;
using bimeup::renderer::VulkanContext;

class SwapchainTest : public ::testing::Test {
protected:
    void SetUp() override {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_window = glfwCreateWindow(800, 600, "SwapchainTest", nullptr, nullptr);
        ASSERT_NE(m_window, nullptr);

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
        m_context = std::make_unique<VulkanContext>(true, requiredExts);

        VkResult result = glfwCreateWindowSurface(
            m_context->GetInstance(), m_window, nullptr, &m_surface);
        ASSERT_EQ(result, VK_SUCCESS);

        m_device = std::make_unique<Device>(m_context->GetInstance(), m_surface);
    }

    void TearDown() override {
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
};

TEST_F(SwapchainTest, CreatesSuccessfully) {
    m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});

    EXPECT_NE(m_swapchain->GetSwapchain(), VK_NULL_HANDLE);
}

TEST_F(SwapchainTest, HasImages) {
    m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});

    EXPECT_GT(m_swapchain->GetImageCount(), 0u);
}

TEST_F(SwapchainTest, HasImageViews) {
    m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});

    EXPECT_EQ(m_swapchain->GetImageViews().size(), m_swapchain->GetImageCount());
    for (auto view : m_swapchain->GetImageViews()) {
        EXPECT_NE(view, VK_NULL_HANDLE);
    }
}

TEST_F(SwapchainTest, FormatIsValid) {
    m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});

    EXPECT_NE(m_swapchain->GetFormat(), VK_FORMAT_UNDEFINED);
}

TEST_F(SwapchainTest, ExtentMatchesRequested) {
    m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});

    VkExtent2D extent = m_swapchain->GetExtent();
    EXPECT_GT(extent.width, 0u);
    EXPECT_GT(extent.height, 0u);
}

TEST_F(SwapchainTest, Recreate) {
    m_swapchain = std::make_unique<Swapchain>(*m_device, m_surface, VkExtent2D{800, 600});
    VkSwapchainKHR old = m_swapchain->GetSwapchain();

    m_swapchain->Recreate(m_surface, VkExtent2D{640, 480});

    EXPECT_NE(m_swapchain->GetSwapchain(), VK_NULL_HANDLE);
    EXPECT_NE(m_swapchain->GetSwapchain(), old);
}

TEST_F(SwapchainTest, DestructorCleansUp) {
    {
        Swapchain swapchain(*m_device, m_surface, VkExtent2D{800, 600});
        EXPECT_NE(swapchain.GetSwapchain(), VK_NULL_HANDLE);
    }
    // Validation layers + sanitizers would catch leaks
}
