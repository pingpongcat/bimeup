#include <gtest/gtest.h>
#include <renderer/AccelerationStructure.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Buffer.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/ShadowPass.h>
#include <renderer/TopLevelAS.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::AccelerationStructure;
using bimeup::renderer::Buffer;
using bimeup::renderer::BufferType;
using bimeup::renderer::DescriptorPool;
using bimeup::renderer::DescriptorSet;
using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::MeshData;
using bimeup::renderer::ShadowMap;
using bimeup::renderer::TlasInstance;
using bimeup::renderer::TopLevelAS;
using bimeup::renderer::Vertex;
using bimeup::renderer::VulkanContext;

class DescriptorSetTest : public ::testing::Test {
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

    Device* m_device = nullptr;

    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
};

std::unique_ptr<VulkanContext> DescriptorSetTest::s_context;
std::unique_ptr<Device> DescriptorSetTest::s_device;

TEST_F(DescriptorSetTest, CreateLayout) {
    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    EXPECT_NE(layout.GetLayout(), VK_NULL_HANDLE);
}

TEST_F(DescriptorSetTest, CreateLayoutMultipleBindings) {
    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT}
    });

    EXPECT_NE(layout.GetLayout(), VK_NULL_HANDLE);
}

TEST_F(DescriptorSetTest, CreatePool) {
    DescriptorPool pool(*m_device, 10, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}
    });

    EXPECT_NE(pool.GetPool(), VK_NULL_HANDLE);
}

TEST_F(DescriptorSetTest, AllocateDescriptorSet) {
    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    DescriptorPool pool(*m_device, 1, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
    });

    DescriptorSet descriptorSet(*m_device, pool, layout);

    EXPECT_NE(descriptorSet.GetSet(), VK_NULL_HANDLE);
}

TEST_F(DescriptorSetTest, UpdateWithBuffer) {
    struct UBO {
        float matrix[16];
    };
    UBO ubo{};

    Buffer buffer(*m_device, BufferType::Uniform, sizeof(UBO), &ubo);

    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    DescriptorPool pool(*m_device, 1, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
    });

    DescriptorSet descriptorSet(*m_device, pool, layout);

    // Should not throw — updates the descriptor set to point to the buffer
    EXPECT_NO_THROW(descriptorSet.UpdateBuffer(0, buffer));
}

TEST_F(DescriptorSetTest, AllocateMultipleSets) {
    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
    });

    DescriptorPool pool(*m_device, 3, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3}
    });

    DescriptorSet set1(*m_device, pool, layout);
    DescriptorSet set2(*m_device, pool, layout);
    DescriptorSet set3(*m_device, pool, layout);

    EXPECT_NE(set1.GetSet(), VK_NULL_HANDLE);
    EXPECT_NE(set2.GetSet(), VK_NULL_HANDLE);
    EXPECT_NE(set3.GetSet(), VK_NULL_HANDLE);

    // All three should be distinct
    EXPECT_NE(set1.GetSet(), set2.GetSet());
    EXPECT_NE(set2.GetSet(), set3.GetSet());
}

TEST_F(DescriptorSetTest, UpdateWithImage) {
    // Shadow map provides a real image view + sampler for the combined-image-sampler
    // descriptor update path used by the main pass to sample shadow depth.
    ShadowMap shadowMap(*m_device, 512U);

    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
    });
    DescriptorPool pool(*m_device, 1, {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    });
    DescriptorSet set(*m_device, pool, layout);

    EXPECT_NO_THROW(set.UpdateImage(0, shadowMap.GetImageView(), shadowMap.GetSampler(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

// 9.Q.2 — Descriptor-set layout must accept the acceleration-structure type
// at any binding slot. Skipped on devices that don't have the AS extension
// (neither RT pipeline nor ray-query enabled), because the descriptor type
// itself is defined by VK_KHR_acceleration_structure.
TEST_F(DescriptorSetTest, LayoutAcceptsAccelerationStructureBinding) {
    if (!m_device->HasRayTracing() && !m_device->HasRayQuery()) {
        GTEST_SKIP() << "VK_KHR_acceleration_structure not enabled on this device";
    }

    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
        {4, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT},
    });

    EXPECT_NE(layout.GetLayout(), VK_NULL_HANDLE);
}

// 9.Q.2 — UpdateAccelerationStructure binds a TLAS handle to the given slot.
// Builds a 1-triangle BLAS + 1-instance TLAS so the call exercises the same
// VK_KHR_acceleration_structure write path raster shaders will hit in 9.Q.3.
TEST_F(DescriptorSetTest, UpdateWithAccelerationStructure) {
    if (!m_device->HasRayTracing() && !m_device->HasRayQuery()) {
        GTEST_SKIP() << "VK_KHR_acceleration_structure not enabled on this device";
    }

    AccelerationStructure as(*m_device);
    MeshData mesh;
    mesh.vertices = {
        Vertex{{0, 0, 0}, {0, 0, 1}, {1, 1, 1, 1}},
        Vertex{{1, 0, 0}, {0, 0, 1}, {1, 1, 1, 1}},
        Vertex{{0, 1, 0}, {0, 0, 1}, {1, 1, 1, 1}},
    };
    mesh.indices = {0, 1, 2};
    auto blas = as.BuildBlas(mesh);
    ASSERT_TRUE(as.IsValid(blas));

    TopLevelAS tlas(*m_device);
    TlasInstance instance{};
    instance.blasAddress = as.GetDeviceAddress(blas);
    ASSERT_TRUE(tlas.Build({instance}));

    DescriptorSetLayout layout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT},
    });
    DescriptorPool pool(*m_device, 1, {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
    });
    DescriptorSet set(*m_device, pool, layout);

    EXPECT_NO_THROW(set.UpdateAccelerationStructure(0, tlas.GetHandle()));
}

TEST_F(DescriptorSetTest, DestructorCleansUp) {
    {
        DescriptorSetLayout layout(*m_device, {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT}
        });
        EXPECT_NE(layout.GetLayout(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked descriptor set layout

    {
        DescriptorPool pool(*m_device, 1, {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
        });
        EXPECT_NE(pool.GetPool(), VK_NULL_HANDLE);
    }
    // Validation layers would catch leaked descriptor pool
}
