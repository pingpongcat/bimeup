#include <gtest/gtest.h>
#include <renderer/ShadowPass.h>
#include <renderer/Device.h>
#include <renderer/VulkanContext.h>

#include <glm/glm.hpp>

#include <memory>

using bimeup::renderer::ComputeLightSpaceMatrix;
using bimeup::renderer::Device;
using bimeup::renderer::ShadowMap;
using bimeup::renderer::VulkanContext;

namespace {

glm::vec2 ProjectToShadowUv(const glm::mat4& ls, const glm::vec3& worldPoint) {
    glm::vec4 clip = ls * glm::vec4(worldPoint, 1.0F);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2(ndc) * 0.5F + 0.5F;
}

float ProjectToShadowDepth(const glm::mat4& ls, const glm::vec3& worldPoint) {
    glm::vec4 clip = ls * glm::vec4(worldPoint, 1.0F);
    return (clip.z / clip.w);
}

}  // namespace

TEST(ShadowPassTest, SceneCenterProjectsToMiddleOfShadowMap) {
    const glm::vec3 lightDir = glm::normalize(glm::vec3(-1.0F, -2.0F, -1.0F));
    const glm::vec3 center(0.0F, 0.0F, 0.0F);
    const float radius = 10.0F;

    const glm::mat4 ls = ComputeLightSpaceMatrix(lightDir, center, radius);
    const glm::vec2 uv = ProjectToShadowUv(ls, center);

    EXPECT_NEAR(uv.x, 0.5F, 1e-5F);
    EXPECT_NEAR(uv.y, 0.5F, 1e-5F);
}

TEST(ShadowPassTest, SceneCenterProjectsToMiddleDepth) {
    const glm::vec3 lightDir = glm::normalize(glm::vec3(0.0F, -1.0F, -1.0F));
    const glm::vec3 center(5.0F, -2.0F, 3.0F);
    const float radius = 8.0F;

    const glm::mat4 ls = ComputeLightSpaceMatrix(lightDir, center, radius);
    const float depth = ProjectToShadowDepth(ls, center);

    // Vulkan depth range is [0, 1]; center sits halfway between near (0) and far (2r).
    EXPECT_NEAR(depth, 0.5F, 1e-5F);
}

TEST(ShadowPassTest, ProjectsRightEdgePointToUvXOne) {
    // Light shines along -Z. The light-space +X axis (right) aligns with world +X,
    // so a point radius units to the +X side of the center lands at UV.x = 1.
    const glm::vec3 lightDir(0.0F, 0.0F, -1.0F);
    const glm::vec3 center(0.0F, 0.0F, 0.0F);
    const float radius = 10.0F;

    const glm::mat4 ls = ComputeLightSpaceMatrix(lightDir, center, radius);
    const glm::vec2 uv = ProjectToShadowUv(ls, center + glm::vec3(radius, 0.0F, 0.0F));

    EXPECT_NEAR(uv.x, 1.0F, 1e-5F);
    EXPECT_NEAR(uv.y, 0.5F, 1e-5F);
}

TEST(ShadowPassTest, PointNearLightHasSmallerDepthThanPointFarFromLight) {
    const glm::vec3 lightDir(0.0F, -1.0F, 0.0F);  // light shines straight down
    const glm::vec3 center(0.0F, 0.0F, 0.0F);
    const float radius = 10.0F;

    const glm::mat4 ls = ComputeLightSpaceMatrix(lightDir, center, radius);

    const float high = ProjectToShadowDepth(ls, glm::vec3(0.0F, 5.0F, 0.0F));   // closer to light (above)
    const float low = ProjectToShadowDepth(ls, glm::vec3(0.0F, -5.0F, 0.0F));   // farther from light

    EXPECT_LT(high, low);
    EXPECT_GE(high, 0.0F);
    EXPECT_LE(low, 1.0F);
}

class ShadowMapTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>(true);
        m_device = std::make_unique<Device>(m_context->GetInstance());
    }

    void TearDown() override {
        m_map.reset();
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<Device> m_device;
    std::unique_ptr<ShadowMap> m_map;
};

TEST_F(ShadowMapTest, CreatesAllHandles) {
    m_map = std::make_unique<ShadowMap>(*m_device, 1024U);

    EXPECT_NE(m_map->GetImage(), VK_NULL_HANDLE);
    EXPECT_NE(m_map->GetImageView(), VK_NULL_HANDLE);
    EXPECT_NE(m_map->GetSampler(), VK_NULL_HANDLE);
    EXPECT_NE(m_map->GetRenderPass(), VK_NULL_HANDLE);
    EXPECT_NE(m_map->GetFramebuffer(), VK_NULL_HANDLE);
}

TEST_F(ShadowMapTest, StoresRequestedResolution) {
    m_map = std::make_unique<ShadowMap>(*m_device, 2048U);

    EXPECT_EQ(m_map->GetResolution(), 2048U);
}

TEST_F(ShadowMapTest, UsesDepthFormat) {
    m_map = std::make_unique<ShadowMap>(*m_device, 1024U);

    const VkFormat fmt = m_map->GetFormat();
    EXPECT_TRUE(fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D16_UNORM)
        << "Unexpected shadow map format: " << fmt;
}

TEST_F(ShadowMapTest, DestructorCleansUp) {
    {
        ShadowMap map(*m_device, 512U);
        EXPECT_NE(map.GetFramebuffer(), VK_NULL_HANDLE);
    }
    // Validation layers + sanitizers would catch leaks
}
