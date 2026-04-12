#include <gtest/gtest.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Device;
using bimeup::renderer::VulkanContext;

class DeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
    }

    void TearDown() override {
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
};

TEST_F(DeviceTest, PhysicalDeviceSelected) {
    m_device = std::make_unique<Device>(m_context->GetInstance());

    EXPECT_NE(m_device->GetPhysicalDevice(), VK_NULL_HANDLE);
}

TEST_F(DeviceTest, LogicalDeviceCreated) {
    m_device = std::make_unique<Device>(m_context->GetInstance());

    EXPECT_NE(m_device->GetDevice(), VK_NULL_HANDLE);
}

TEST_F(DeviceTest, GraphicsQueueAvailable) {
    m_device = std::make_unique<Device>(m_context->GetInstance());

    EXPECT_NE(m_device->GetGraphicsQueue(), VK_NULL_HANDLE);
}

TEST_F(DeviceTest, GraphicsQueueFamilyValid) {
    m_device = std::make_unique<Device>(m_context->GetInstance());

    // Queue family index should be retrievable (we just check it was found)
    VkPhysicalDevice physDev = m_device->GetPhysicalDevice();
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueFamilyCount, nullptr);

    EXPECT_LT(m_device->GetGraphicsQueueFamily(), queueFamilyCount);
}

TEST_F(DeviceTest, DestructorCleansUp) {
    {
        Device device(m_context->GetInstance());
        EXPECT_NE(device.GetDevice(), VK_NULL_HANDLE);
    }
    // Validation layers + sanitizers would catch leaks or use-after-free
}
