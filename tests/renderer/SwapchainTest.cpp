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
    static void SetUpTestSuite() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        s_window = glfwCreateWindow(800, 600, "SwapchainTest", nullptr, nullptr);
        ASSERT_NE(s_window, nullptr);

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
        s_context = std::make_unique<VulkanContext>(true, requiredExts);

        VkResult result = glfwCreateWindowSurface(
            s_context->GetInstance(), s_window, nullptr, &s_surface);
        ASSERT_EQ(result, VK_SUCCESS);

        s_device = std::make_unique<Device>(s_context->GetInstance(), s_surface);
    }

    static void TearDownTestSuite() {
        s_device.reset();
        if (s_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(s_context->GetInstance(), s_surface, nullptr);
            s_surface = VK_NULL_HANDLE;
        }
        s_context.reset();
        if (s_window != nullptr) {
            glfwDestroyWindow(s_window);
            s_window = nullptr;
        }
        glfwTerminate();
    }

    void SetUp() override {
        m_surface = s_surface;
        m_device = s_device.get();
    }

    void TearDown() override {
        m_swapchain.reset();
    }

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    Device* m_device = nullptr;
    std::unique_ptr<Swapchain> m_swapchain;

    static GLFWwindow* s_window;
    static VkSurfaceKHR s_surface;
    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
};

GLFWwindow* SwapchainTest::s_window = nullptr;
VkSurfaceKHR SwapchainTest::s_surface = VK_NULL_HANDLE;
std::unique_ptr<VulkanContext> SwapchainTest::s_context;
std::unique_ptr<Device> SwapchainTest::s_device;

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
