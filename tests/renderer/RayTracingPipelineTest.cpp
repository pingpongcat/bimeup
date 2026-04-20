#include <gtest/gtest.h>
#include <renderer/Device.h>
#include <renderer/RayTracingPipeline.h>
#include <renderer/VulkanContext.h>

#include <memory>
#include <string>

using bimeup::renderer::Device;
using bimeup::renderer::RayTracingPipeline;
using bimeup::renderer::RayTracingPipelineSettings;
using bimeup::renderer::VulkanContext;

// Stage 9.3 — RT pipeline + SBT. Same RT-contract as 9.1.b / 9.2: strict
// no-op on non-RT devices; GPU-path tests `GTEST_SKIP` when the probe
// says no. Shaders come from the `rt_probe.*` stubs compiled into the
// build-time shader dir (see `CompileShaders.cmake`).
class RayTracingPipelineTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        s_context = std::make_unique<VulkanContext>(true);
        s_device = std::make_unique<Device>(s_context->GetInstance());
    }

    static void TearDownTestSuite() {
        s_device.reset();
        s_context.reset();
    }

    void SetUp() override { m_device = s_device.get(); }

    static RayTracingPipelineSettings MakeSettings(bool withAnyHit = true) {
        RayTracingPipelineSettings s;
        const std::string dir = BIMEUP_SHADER_DIR;
        s.raygenPath = dir + "/rt_probe.rgen.spv";
        s.missPath = dir + "/rt_probe.rmiss.spv";
        s.closestHitPath = dir + "/rt_probe.rchit.spv";
        if (withAnyHit) {
            s.anyHitPath = dir + "/rt_probe.rahit.spv";
        }
        return s;
    }

    Device* m_device = nullptr;
    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
};

std::unique_ptr<VulkanContext> RayTracingPipelineTest::s_context;
std::unique_ptr<Device> RayTracingPipelineTest::s_device;

TEST_F(RayTracingPipelineTest, ConstructSucceedsOnAnyDevice) {
    RayTracingPipeline rt(*m_device);
    EXPECT_FALSE(rt.IsValid());
    EXPECT_EQ(rt.GetPipeline(), VK_NULL_HANDLE);
    EXPECT_EQ(rt.GetLayout(), VK_NULL_HANDLE);
    EXPECT_EQ(rt.GetRaygenRegion().size, 0u);
    EXPECT_EQ(rt.GetMissRegion().size, 0u);
    EXPECT_EQ(rt.GetHitRegion().size, 0u);
    EXPECT_EQ(rt.GetCallableRegion().size, 0u);
}

TEST_F(RayTracingPipelineTest, BuildNoOpWhenRayTracingUnavailable) {
    if (m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device advertises RT — no-op branch not exercised here";
    }
    RayTracingPipeline rt(*m_device);
    EXPECT_FALSE(rt.Build(MakeSettings()));
    EXPECT_FALSE(rt.IsValid());
    EXPECT_EQ(rt.GetRaygenRegion().deviceAddress, 0u);
}

TEST_F(RayTracingPipelineTest, BuildEmptyPathsReturnsFalseOnRtDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — build-path skipped";
    }
    RayTracingPipeline rt(*m_device);
    RayTracingPipelineSettings bad;  // all paths empty
    EXPECT_FALSE(rt.Build(bad));
    EXPECT_FALSE(rt.IsValid());
}

TEST_F(RayTracingPipelineTest, BuildCreatesPipelineAndSbtOnRtDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — build-path skipped";
    }
    RayTracingPipeline rt(*m_device);
    EXPECT_TRUE(rt.Build(MakeSettings(/*withAnyHit=*/true)));
    EXPECT_TRUE(rt.IsValid());
    EXPECT_NE(rt.GetPipeline(), VK_NULL_HANDLE);
    EXPECT_NE(rt.GetLayout(), VK_NULL_HANDLE);

    const auto& rg = rt.GetRaygenRegion();
    const auto& ms = rt.GetMissRegion();
    const auto& ht = rt.GetHitRegion();
    const auto& cl = rt.GetCallableRegion();

    // raygen region must have size == stride (Vulkan spec).
    EXPECT_NE(rg.deviceAddress, 0u);
    EXPECT_GT(rg.stride, 0u);
    EXPECT_EQ(rg.size, rg.stride);

    EXPECT_NE(ms.deviceAddress, 0u);
    EXPECT_GT(ms.stride, 0u);
    EXPECT_GE(ms.size, ms.stride);

    EXPECT_NE(ht.deviceAddress, 0u);
    EXPECT_GT(ht.stride, 0u);
    EXPECT_GE(ht.size, ht.stride);

    // callable table is unused for stage 9 — empty region.
    EXPECT_EQ(cl.deviceAddress, 0u);
    EXPECT_EQ(cl.size, 0u);
}

TEST_F(RayTracingPipelineTest, BuildWithoutAnyHitStillSucceeds) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — build-path skipped";
    }
    RayTracingPipeline rt(*m_device);
    EXPECT_TRUE(rt.Build(MakeSettings(/*withAnyHit=*/false)));
    EXPECT_TRUE(rt.IsValid());
    EXPECT_GT(rt.GetHitRegion().size, 0u);
}

TEST_F(RayTracingPipelineTest, RebuildReplacesPipelineHandle) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — build-path skipped";
    }
    RayTracingPipeline rt(*m_device);
    EXPECT_TRUE(rt.Build(MakeSettings()));
    const VkPipeline first = rt.GetPipeline();
    const VkDeviceAddress firstRgAddr = rt.GetRaygenRegion().deviceAddress;
    EXPECT_TRUE(rt.Build(MakeSettings()));
    EXPECT_NE(rt.GetPipeline(), first);
    // SBT was torn down + reallocated too — device addresses should differ in
    // most cases, but a driver is allowed to reuse the same address. We only
    // assert the pipeline handle changed, which is a stable rebuild signal.
    (void)firstRgAddr;
}
