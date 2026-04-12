#include <gtest/gtest.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::VulkanContext;

TEST(VulkanContextTest, CreateInstance) {
    VulkanContext ctx(true);  // enable validation

    EXPECT_NE(ctx.GetInstance(), VK_NULL_HANDLE);
}

TEST(VulkanContextTest, ValidationLayersActiveInDebug) {
    VulkanContext ctx(true);

    EXPECT_TRUE(ctx.HasValidationLayers());
}

TEST(VulkanContextTest, DebugMessengerCreatedWithValidation) {
    VulkanContext ctx(true);

    EXPECT_TRUE(ctx.HasDebugMessenger());
}

TEST(VulkanContextTest, NoValidationLayers) {
    VulkanContext ctx(false);

    EXPECT_NE(ctx.GetInstance(), VK_NULL_HANDLE);
    EXPECT_FALSE(ctx.HasValidationLayers());
    EXPECT_FALSE(ctx.HasDebugMessenger());
}

TEST(VulkanContextTest, DestructorCleansUp) {
    {
        VulkanContext ctx(true);
        EXPECT_NE(ctx.GetInstance(), VK_NULL_HANDLE);
    }
    // If destructor doesn't clean up properly, validation layers would report leaks
    // and sanitizers would catch use-after-free. No crash = pass.
}
