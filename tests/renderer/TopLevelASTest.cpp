#include <gtest/gtest.h>
#include <renderer/AccelerationStructure.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/TopLevelAS.h>
#include <renderer/VulkanContext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using bimeup::renderer::AccelerationStructure;
using bimeup::renderer::Device;
using bimeup::renderer::MeshData;
using bimeup::renderer::TlasInstance;
using bimeup::renderer::TopLevelAS;
using bimeup::renderer::VulkanContext;

// Stage 9.2 — TLAS build from scene instances. Same RT-contract as 9.1.b:
// strict no-op on non-RT devices. Tests that need an actual GPU BLAS/TLAS
// build GTEST_SKIP gracefully when the probe says no.
class TopLevelASTest : public ::testing::Test {
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

    static MeshData MakeTriangle() {
        MeshData data;
        data.vertices = {
            {{0.0F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F, 1.0F}},
            {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 1.0F, 0.0F, 1.0F}},
            {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 1.0F, 1.0F}},
        };
        data.indices = {0, 1, 2};
        return data;
    }

    Device* m_device = nullptr;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
};

std::unique_ptr<VulkanContext> TopLevelASTest::s_context;
std::unique_ptr<Device> TopLevelASTest::s_device;

TEST_F(TopLevelASTest, ConstructSucceedsOnAnyDevice) {
    TopLevelAS tlas(*m_device);
    EXPECT_FALSE(tlas.IsValid());
    EXPECT_EQ(tlas.InstanceCount(), 0u);
    EXPECT_EQ(tlas.GetDeviceAddress(), 0u);
}

TEST_F(TopLevelASTest, BuildEmptyInstancesIsNoOp) {
    TopLevelAS tlas(*m_device);
    EXPECT_FALSE(tlas.Build({}));
    EXPECT_EQ(tlas.InstanceCount(), 0u);
    EXPECT_FALSE(tlas.IsValid());
}

TEST_F(TopLevelASTest, BuildNoOpWhenRayTracingUnavailable) {
    if (m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device advertises RT — no-op branch not exercised here";
    }
    TopLevelAS tlas(*m_device);
    TlasInstance inst;
    inst.blasAddress = 0xDEADBEEF;
    EXPECT_FALSE(tlas.Build({inst}));
    EXPECT_EQ(tlas.InstanceCount(), 0u);
    EXPECT_FALSE(tlas.IsValid());
}

TEST_F(TopLevelASTest, BuildMatchesInstanceCountOnRayTracingDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — TLAS-build path skipped";
    }
    AccelerationStructure accel(*m_device);
    auto blas = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(blas, AccelerationStructure::InvalidHandle);
    const VkDeviceAddress blasAddr = accel.GetDeviceAddress(blas);

    TopLevelAS tlas(*m_device);
    TlasInstance a{glm::mat4(1.0F), blasAddr, 0, 0xFF};
    TlasInstance b{glm::translate(glm::mat4(1.0F), glm::vec3(2.0F, 0.0F, 0.0F)),
                   blasAddr, 1, 0xFF};

    EXPECT_TRUE(tlas.Build({a, b}));
    EXPECT_TRUE(tlas.IsValid());
    EXPECT_EQ(tlas.InstanceCount(), 2u);
    EXPECT_NE(tlas.GetDeviceAddress(), 0u);
    EXPECT_NE(tlas.GetHandle(), VK_NULL_HANDLE);
}

TEST_F(TopLevelASTest, RebuildUpdatesInstanceCount) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — TLAS-build path skipped";
    }
    AccelerationStructure accel(*m_device);
    auto blas = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(blas, AccelerationStructure::InvalidHandle);
    const VkDeviceAddress blasAddr = accel.GetDeviceAddress(blas);

    TopLevelAS tlas(*m_device);
    TlasInstance a{glm::mat4(1.0F), blasAddr, 0, 0xFF};
    EXPECT_TRUE(tlas.Build({a}));
    EXPECT_EQ(tlas.InstanceCount(), 1u);
    const VkAccelerationStructureKHR firstHandle = tlas.GetHandle();

    TlasInstance b = a;
    b.customIndex = 1;
    TlasInstance c = a;
    c.customIndex = 2;
    EXPECT_TRUE(tlas.Build({a, b, c}));
    EXPECT_EQ(tlas.InstanceCount(), 3u);
    // Rebuild must replace the old AS, not leak it — handle is re-created.
    EXPECT_NE(tlas.GetHandle(), firstHandle);
    EXPECT_TRUE(tlas.IsValid());
}

TEST_F(TopLevelASTest, DefaultInstanceFlagsAreZero) {
    // Stage 9.6.a — `flags` defaults to 0 so callers that don't opt into
    // transmission keep the pre-9.6 opaque-ray semantics. `TopLevelAS::Build`
    // OR's the always-on `TRIANGLE_FACING_CULL_DISABLE_BIT` on top.
    TlasInstance inst;
    EXPECT_EQ(inst.flags, 0u);
}

TEST_F(TopLevelASTest, BuildWithTransmissiveFlagsSucceedsOnRtDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — TLAS-build path skipped";
    }
    AccelerationStructure accel(*m_device);
    auto blas = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(blas, AccelerationStructure::InvalidHandle);

    // Stage 9.6.a — glass instance: per-instance flag forces the geometry
    // non-opaque so the shadow ray's any-hit shader runs (9.6.b).
    TopLevelAS tlas(*m_device);
    TlasInstance glass{glm::mat4(1.0F), accel.GetDeviceAddress(blas), 0, 0xFF};
    glass.flags = VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;

    EXPECT_TRUE(tlas.Build({glass}));
    EXPECT_TRUE(tlas.IsValid());
    EXPECT_EQ(tlas.InstanceCount(), 1u);
}

TEST_F(TopLevelASTest, RebuildDownToZeroInstancesResets) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — TLAS-build path skipped";
    }
    AccelerationStructure accel(*m_device);
    auto blas = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(blas, AccelerationStructure::InvalidHandle);

    TopLevelAS tlas(*m_device);
    TlasInstance a{glm::mat4(1.0F), accel.GetDeviceAddress(blas), 0, 0xFF};
    EXPECT_TRUE(tlas.Build({a}));
    EXPECT_TRUE(tlas.IsValid());

    // Scene cleared — rebuild with no instances should leave the TLAS empty.
    EXPECT_FALSE(tlas.Build({}));
    EXPECT_FALSE(tlas.IsValid());
    EXPECT_EQ(tlas.InstanceCount(), 0u);
}
