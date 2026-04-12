#include <gtest/gtest.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Buffer.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Buffer;
using bimeup::renderer::BufferType;
using bimeup::renderer::DescriptorPool;
using bimeup::renderer::DescriptorSet;
using bimeup::renderer::DescriptorSetLayout;
using bimeup::renderer::Device;
using bimeup::renderer::VulkanContext;

class DescriptorSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
};

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
