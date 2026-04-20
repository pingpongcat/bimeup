#include <gtest/gtest.h>

#include <renderer/AccelerationStructure.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/RtIndoorPass.h>
#include <renderer/TopLevelAS.h>
#include <renderer/VulkanContext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <string>

using bimeup::renderer::AccelerationStructure;
using bimeup::renderer::Device;
using bimeup::renderer::MeshData;
using bimeup::renderer::RtIndoorPass;
using bimeup::renderer::TlasInstance;
using bimeup::renderer::TopLevelAS;
using bimeup::renderer::VulkanContext;

// Stage 9.7.a — RT indoor-fill visibility pass. Same RT-contract as 9.4.a
// / 9.5.a: strict no-op on non-RT devices; GPU-path tests `GTEST_SKIP`
// when the capability probe says no.
class RtIndoorPassTest : public ::testing::Test {
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

    static std::string ShaderDir() { return std::string(BIMEUP_SHADER_DIR); }

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

std::unique_ptr<VulkanContext> RtIndoorPassTest::s_context;
std::unique_ptr<Device> RtIndoorPassTest::s_device;

TEST_F(RtIndoorPassTest, ConstructSucceedsOnAnyDevice) {
    RtIndoorPass pass(*m_device);
    EXPECT_FALSE(pass.IsValid());
    EXPECT_EQ(pass.GetVisibilityImage(), VK_NULL_HANDLE);
    EXPECT_EQ(pass.GetVisibilityImageView(), VK_NULL_HANDLE);
    EXPECT_EQ(pass.GetWidth(), 0u);
    EXPECT_EQ(pass.GetHeight(), 0u);
}

TEST_F(RtIndoorPassTest, BuildNoOpWhenRayTracingUnavailable) {
    if (m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device advertises RT — no-op branch not exercised here";
    }
    RtIndoorPass pass(*m_device);
    EXPECT_FALSE(pass.Build(64u, 64u, ShaderDir()));
    EXPECT_FALSE(pass.IsValid());
    EXPECT_EQ(pass.GetVisibilityImage(), VK_NULL_HANDLE);
}

TEST_F(RtIndoorPassTest, PushConstantsSize96Bytes) {
    // mat4 invViewProj (64) + vec4 fillDirWorld (16) + uvec2 extent (8) +
    // uint pad0 (4) + uint pad1 (4) = 96 B. Mirrors `RtShadowPass` exactly
    // — the indoor-fill raygen is a shadow trace with a different fixed
    // direction and a building-scale max-distance.
    EXPECT_EQ(sizeof(RtIndoorPass::PushConstants), 96U);
}

TEST_F(RtIndoorPassTest, BuildAllocatesVisibilityResourcesOnRtDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — build-path skipped";
    }
    RtIndoorPass pass(*m_device);
    EXPECT_TRUE(pass.Build(64u, 48u, ShaderDir()));
    EXPECT_TRUE(pass.IsValid());
    EXPECT_NE(pass.GetVisibilityImage(), VK_NULL_HANDLE);
    EXPECT_NE(pass.GetVisibilityImageView(), VK_NULL_HANDLE);
    EXPECT_NE(pass.GetVisibilitySampler(), VK_NULL_HANDLE);
    EXPECT_NE(pass.GetDescriptorSetLayout(), VK_NULL_HANDLE);
    EXPECT_EQ(pass.GetWidth(), 64u);
    EXPECT_EQ(pass.GetHeight(), 48u);
}

TEST_F(RtIndoorPassTest, RebuildAtNewExtentReplacesImage) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — build-path skipped";
    }
    RtIndoorPass pass(*m_device);
    ASSERT_TRUE(pass.Build(32u, 32u, ShaderDir()));
    const VkImage first = pass.GetVisibilityImage();
    ASSERT_NE(first, VK_NULL_HANDLE);

    ASSERT_TRUE(pass.Build(64u, 64u, ShaderDir()));
    EXPECT_NE(pass.GetVisibilityImage(), first);
    EXPECT_EQ(pass.GetWidth(), 64u);
    EXPECT_EQ(pass.GetHeight(), 64u);
}

TEST_F(RtIndoorPassTest, DispatchRecordsAndSubmitsCleanlyOnRtDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — dispatch path skipped";
    }

    RtIndoorPass pass(*m_device);
    const uint32_t w = 16;
    const uint32_t h = 16;
    ASSERT_TRUE(pass.Build(w, h, ShaderDir()));

    AccelerationStructure accel(*m_device);
    auto blas = accel.BuildBlas(MakeTriangle());
    ASSERT_NE(blas, AccelerationStructure::InvalidHandle);
    TopLevelAS tlas(*m_device);
    TlasInstance inst{glm::mat4(1.0F), accel.GetDeviceAddress(blas), 0, 0xFF, 0};
    ASSERT_TRUE(tlas.Build({inst}));
    ASSERT_NE(tlas.GetHandle(), VK_NULL_HANDLE);

    VkImage depthImage = VK_NULL_HANDLE;
    VmaAllocation depthAlloc = VK_NULL_HANDLE;
    VkImageCreateInfo depthCi{};
    depthCi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthCi.imageType = VK_IMAGE_TYPE_2D;
    depthCi.format = VK_FORMAT_D32_SFLOAT;
    depthCi.extent = {w, h, 1};
    depthCi.mipLevels = 1;
    depthCi.arrayLayers = 1;
    depthCi.samples = VK_SAMPLE_COUNT_1_BIT;
    depthCi.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthCi.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo depthAi{};
    depthAi.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    ASSERT_EQ(vmaCreateImage(m_device->GetAllocator(), &depthCi, &depthAi,
                             &depthImage, &depthAlloc, nullptr),
              VK_SUCCESS);

    VkImageView depthView = VK_NULL_HANDLE;
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = depthImage;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_D32_SFLOAT;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    ASSERT_EQ(vkCreateImageView(m_device->GetDevice(), &vi, nullptr, &depthView),
              VK_SUCCESS);

    VkSampler depthSampler = VK_NULL_HANDLE;
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_NEAREST;
    si.minFilter = VK_FILTER_NEAREST;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.minLod = 0.0F;
    si.maxLod = 0.0F;
    ASSERT_EQ(vkCreateSampler(m_device->GetDevice(), &si, nullptr, &depthSampler),
              VK_SUCCESS);

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = m_device->GetGraphicsQueueFamily();
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ASSERT_EQ(vkCreateCommandPool(m_device->GetDevice(), &pci, nullptr, &pool),
              VK_SUCCESS);

    VkCommandBufferAllocateInfo cbi{};
    cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbi.commandPool = pool;
    cbi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbi.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    ASSERT_EQ(vkAllocateCommandBuffers(m_device->GetDevice(), &cbi, &cmd),
              VK_SUCCESS);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    ASSERT_EQ(vkBeginCommandBuffer(cmd, &bi), VK_SUCCESS);

    auto barrier = [&](VkImageLayout oldL, VkImageLayout newL,
                       VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                       VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                       VkImageAspectFlags aspect) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = oldL;
        b.newLayout = newL;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        b.image = depthImage;
        b.subresourceRange.aspectMask = aspect;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    };
    barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
    VkClearDepthStencilValue clear{0.5F, 0};
    VkImageSubresourceRange depthRange{};
    depthRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthRange.levelCount = 1;
    depthRange.layerCount = 1;
    vkCmdClearDepthStencilImage(cmd, depthImage,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear,
                                1, &depthRange);
    barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_IMAGE_ASPECT_DEPTH_BIT);

    const glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 2),
                                       glm::vec3(0, 0, 0),
                                       glm::vec3(0, 1, 0));
    const glm::mat4 proj = glm::perspective(glm::radians(45.0F),
                                            static_cast<float>(w) / static_cast<float>(h),
                                            0.1F, 100.0F);
    // Default overhead-fill travel direction from `Lighting.cpp` — same
    // ({0.2, -1, 0.3} normalised) that `PackSunLighting` seeds when
    // `indoorLightsEnabled = true`.
    const glm::vec3 fillDirWorld = glm::normalize(glm::vec3(0.2F, -1.0F, 0.3F));

    pass.Dispatch(cmd, tlas.GetHandle(), depthView, depthSampler,
                  view, proj, fillDirWorld);

    ASSERT_EQ(vkEndCommandBuffer(cmd), VK_SUCCESS);

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateFence(m_device->GetDevice(), &fi, nullptr, &fence), VK_SUCCESS);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    ASSERT_EQ(vkQueueSubmit(m_device->GetGraphicsQueue(), 1, &submit, fence), VK_SUCCESS);
    ASSERT_EQ(vkWaitForFences(m_device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX),
              VK_SUCCESS);

    vkDestroyFence(m_device->GetDevice(), fence, nullptr);
    vkDestroyCommandPool(m_device->GetDevice(), pool, nullptr);
    vkDestroySampler(m_device->GetDevice(), depthSampler, nullptr);
    vkDestroyImageView(m_device->GetDevice(), depthView, nullptr);
    vmaDestroyImage(m_device->GetAllocator(), depthImage, depthAlloc);
}
