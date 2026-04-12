#include <gtest/gtest.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/VulkanContext.h>

using bimeup::renderer::Device;
using bimeup::renderer::MeshBuffer;
using bimeup::renderer::MeshData;
using bimeup::renderer::MeshHandle;
using bimeup::renderer::Vertex;
using bimeup::renderer::VulkanContext;

class MeshBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_meshBuffer.reset();
        m_device.reset();
        m_context.reset();
    }

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

    static MeshData MakeQuad() {
        MeshData data;
        data.vertices = {
            {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
            {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
            {{0.5F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
            {{-0.5F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}},
        };
        data.indices = {0, 1, 2, 2, 3, 0};
        return data;
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<MeshBuffer> m_meshBuffer;
};

TEST_F(MeshBufferTest, UploadMeshReturnsValidHandle) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle handle = m_meshBuffer->Upload(MakeTriangle());
    EXPECT_NE(handle, MeshBuffer::InvalidHandle);
}

TEST_F(MeshBufferTest, UploadMultipleMeshesReturnsDifferentHandles) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    MeshHandle h2 = m_meshBuffer->Upload(MakeQuad());
    EXPECT_NE(h1, h2);
}

TEST_F(MeshBufferTest, GetDrawParamsReturnsCorrectCounts) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle handle = m_meshBuffer->Upload(MakeTriangle());

    auto params = m_meshBuffer->GetDrawParams(handle);
    EXPECT_EQ(params.indexCount, 3);
    EXPECT_EQ(params.firstIndex, 0);
    EXPECT_EQ(params.vertexOffset, 0);
}

TEST_F(MeshBufferTest, SecondMeshDrawParamsOffsetCorrectly) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    MeshHandle h2 = m_meshBuffer->Upload(MakeQuad());

    auto p1 = m_meshBuffer->GetDrawParams(h1);
    auto p2 = m_meshBuffer->GetDrawParams(h2);

    EXPECT_EQ(p1.indexCount, 3);
    EXPECT_EQ(p1.firstIndex, 0);
    EXPECT_EQ(p1.vertexOffset, 0);

    EXPECT_EQ(p2.indexCount, 6);
    EXPECT_EQ(p2.firstIndex, 3);  // after triangle's 3 indices
    EXPECT_EQ(p2.vertexOffset, 3);  // after triangle's 3 vertices
}

TEST_F(MeshBufferTest, RemoveMeshMakesHandleInvalid) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    MeshHandle handle = m_meshBuffer->Upload(MakeTriangle());
    m_meshBuffer->Remove(handle);
    EXPECT_FALSE(m_meshBuffer->IsValid(handle));
}

TEST_F(MeshBufferTest, MeshCountTracksUploadAndRemove) {
    m_meshBuffer = std::make_unique<MeshBuffer>(*m_device);
    EXPECT_EQ(m_meshBuffer->MeshCount(), 0);

    MeshHandle h1 = m_meshBuffer->Upload(MakeTriangle());
    EXPECT_EQ(m_meshBuffer->MeshCount(), 1);

    m_meshBuffer->Upload(MakeQuad());
    EXPECT_EQ(m_meshBuffer->MeshCount(), 2);

    m_meshBuffer->Remove(h1);
    EXPECT_EQ(m_meshBuffer->MeshCount(), 1);
}
