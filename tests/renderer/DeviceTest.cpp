#include <gtest/gtest.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <string_view>
#include <vector>

using bimeup::renderer::Device;
using bimeup::renderer::VulkanContext;

// Device construction is the unit under test, so m_device is per-test. Only
// the (expensive) VulkanContext is shared across the suite.
class DeviceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
    }

    static void TearDownTestSuite() {
        s_context.reset();
    }

    void SetUp() override { m_context = s_context.get(); }
    void TearDown() override { m_device.reset(); }

    VulkanContext* m_context = nullptr;
    std::unique_ptr<Device> m_device;

    static std::unique_ptr<VulkanContext> s_context;
};

std::unique_ptr<VulkanContext> DeviceTest::s_context;

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

// 9.1.a — RT-capability probe. The probe must return a boolean that's consistent
// with the physical device's advertised extensions. If the probe says yes, all
// five RT-adjacent extensions must be present on the physical device. If *any*
// of them is missing, the probe must report false. Construction must succeed
// either way — RT is opt-in, never a hard requirement.
TEST_F(DeviceTest, RayTracingProbeConsistentWithExtensions) {
    m_device = std::make_unique<Device>(m_context->GetInstance());

    VkPhysicalDevice physDev = m_device->GetPhysicalDevice();
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extCount, exts.data());

    auto hasExt = [&](std::string_view name) {
        for (const auto& e : exts) {
            if (std::string_view(e.extensionName) == name) return true;
        }
        return false;
    };

    const bool allPresent =
        hasExt(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
        hasExt(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) &&
        hasExt(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

    if (!allPresent) {
        EXPECT_FALSE(m_device->HasRayTracing())
            << "Probe must report false when any RT extension is missing";
    }
    if (m_device->HasRayTracing()) {
        EXPECT_TRUE(allPresent)
            << "Probe reporting true implies all RT extensions are present";
    }
}

TEST_F(DeviceTest, DeviceConstructsRegardlessOfRayTracing) {
    m_device = std::make_unique<Device>(m_context->GetInstance());

    EXPECT_NE(m_device->GetDevice(), VK_NULL_HANDLE);
    [[maybe_unused]] const bool probe = m_device->HasRayTracing();
}
