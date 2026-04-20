#include <gtest/gtest.h>
#include <renderer/AccelerationStructure.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::AccelerationStructure;
using bimeup::renderer::Device;
using bimeup::renderer::MeshData;
using bimeup::renderer::VulkanContext;

// Stage 9.1.b — BLAS-per-mesh builder. The module is contractually a no-op on
// devices without RT support, so every test must stay benign on a classical-only
// GPU. The VulkanContext+Device are shared across the fixture to keep the suite
// cheap; only the AccelerationStructure instance is per-test.
class AccelerationStructureTest : public ::testing::Test {
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

std::unique_ptr<VulkanContext> AccelerationStructureTest::s_context;
std::unique_ptr<Device> AccelerationStructureTest::s_device;

TEST_F(AccelerationStructureTest, ConstructSucceedsOnAnyDevice) {
    AccelerationStructure accel(*m_device);
    EXPECT_EQ(accel.BlasCount(), 0u);
}

TEST_F(AccelerationStructureTest, BuildBlasOnEmptyMeshReturnsInvalid) {
    AccelerationStructure accel(*m_device);
    MeshData empty;
    EXPECT_EQ(accel.BuildBlas(empty), AccelerationStructure::InvalidHandle);
    EXPECT_EQ(accel.BlasCount(), 0u);
}

TEST_F(AccelerationStructureTest, BuildBlasNoOpWhenRayTracingUnavailable) {
    if (m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device advertises RT — no-op branch not exercised here";
    }
    AccelerationStructure accel(*m_device);
    auto handle = accel.BuildBlas(MakeTriangle());
    EXPECT_EQ(handle, AccelerationStructure::InvalidHandle);
    EXPECT_EQ(accel.BlasCount(), 0u);
}

TEST_F(AccelerationStructureTest, BuildBlasProducesValidHandleOnRayTracingDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — BLAS-build path skipped";
    }
    AccelerationStructure accel(*m_device);
    auto handle = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(handle, AccelerationStructure::InvalidHandle);
    EXPECT_TRUE(accel.IsValid(handle));
    EXPECT_NE(accel.GetHandle(handle), VK_NULL_HANDLE);
    EXPECT_NE(accel.GetDeviceAddress(handle), 0u);
    EXPECT_EQ(accel.BlasCount(), 1u);
}

TEST_F(AccelerationStructureTest, MultipleBlasBuildsGetDistinctHandles) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — BLAS-build path skipped";
    }
    AccelerationStructure accel(*m_device);
    auto h1 = accel.BuildBlas(MakeTriangle());
    auto h2 = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(h1, AccelerationStructure::InvalidHandle);
    ASSERT_NE(h2, AccelerationStructure::InvalidHandle);
    EXPECT_NE(h1, h2);
    EXPECT_NE(accel.GetDeviceAddress(h1), accel.GetDeviceAddress(h2));
    EXPECT_EQ(accel.BlasCount(), 2u);
}
