#include <gtest/gtest.h>
#include <renderer/Buffer.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Buffer;
using bimeup::renderer::BufferType;
using bimeup::renderer::Device;
using bimeup::renderer::VulkanContext;

class BufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_buffer.reset();
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<Buffer> m_buffer;
};

TEST_F(BufferTest, CreateVertexBuffer) {
    const std::vector<float> vertices = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    m_buffer = std::make_unique<Buffer>(
        *m_device, BufferType::Vertex,
        vertices.size() * sizeof(float), vertices.data());

    EXPECT_NE(m_buffer->GetBuffer(), VK_NULL_HANDLE);
    EXPECT_EQ(m_buffer->GetSize(), vertices.size() * sizeof(float));
    EXPECT_EQ(m_buffer->GetType(), BufferType::Vertex);
}

TEST_F(BufferTest, CreateIndexBuffer) {
    const std::vector<uint32_t> indices = {0, 1, 2};

    m_buffer = std::make_unique<Buffer>(
        *m_device, BufferType::Index,
        indices.size() * sizeof(uint32_t), indices.data());

    EXPECT_NE(m_buffer->GetBuffer(), VK_NULL_HANDLE);
    EXPECT_EQ(m_buffer->GetSize(), indices.size() * sizeof(uint32_t));
    EXPECT_EQ(m_buffer->GetType(), BufferType::Index);
}

TEST_F(BufferTest, CreateUniformBuffer) {
    struct UBO {
        float matrix[16];
    };
    UBO ubo{};

    m_buffer = std::make_unique<Buffer>(
        *m_device, BufferType::Uniform,
        sizeof(UBO), &ubo);

    EXPECT_NE(m_buffer->GetBuffer(), VK_NULL_HANDLE);
    EXPECT_EQ(m_buffer->GetSize(), sizeof(UBO));
    EXPECT_EQ(m_buffer->GetType(), BufferType::Uniform);
}

TEST_F(BufferTest, VertexBufferSizeMatchesInput) {
    const std::vector<float> vertices = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };

    m_buffer = std::make_unique<Buffer>(
        *m_device, BufferType::Vertex,
        vertices.size() * sizeof(float), vertices.data());

    EXPECT_EQ(m_buffer->GetSize(), 4 * 3 * sizeof(float));
}

TEST_F(BufferTest, UniformBufferMappable) {
    struct UBO {
        float value;
    };
    UBO ubo{42.0f};

    m_buffer = std::make_unique<Buffer>(
        *m_device, BufferType::Uniform,
        sizeof(UBO), &ubo);

    // Uniform buffers should be host-visible and mappable for updates
    void* mapped = m_buffer->Map();
    ASSERT_NE(mapped, nullptr);
    auto* readBack = static_cast<UBO*>(mapped);
    EXPECT_FLOAT_EQ(readBack->value, 42.0f);
    m_buffer->Unmap();
}

TEST_F(BufferTest, DestructorCleansUp) {
    const std::vector<float> data = {1.0f, 2.0f, 3.0f};
    {
        Buffer buffer(*m_device, BufferType::Vertex,
                      data.size() * sizeof(float), data.data());
        EXPECT_NE(buffer.GetBuffer(), VK_NULL_HANDLE);
    }
    // Validation layers + sanitizers would catch leaks or use-after-free
}
