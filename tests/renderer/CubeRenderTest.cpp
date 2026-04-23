#include <gtest/gtest.h>
#include <renderer/Buffer.h>
#include <renderer/Camera.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Pipeline.h>
#include <renderer/RenderLoop.h>
#include <renderer/Shader.h>
#include <renderer/Swapchain.h>
#include <renderer/VulkanContext.h>

#include <array>
#include <filesystem>
#include <span>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace bimeup::renderer;

namespace {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
};

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

// Unit cube with per-face colors
void MakeCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
    // clang-format off
    vertices = {
        // Front (red)
        {{-0.5F, -0.5F,  0.5F}, { 0, 0, 1}, {1, 0, 0, 1}},
        {{ 0.5F, -0.5F,  0.5F}, { 0, 0, 1}, {1, 0, 0, 1}},
        {{ 0.5F,  0.5F,  0.5F}, { 0, 0, 1}, {1, 0, 0, 1}},
        {{-0.5F,  0.5F,  0.5F}, { 0, 0, 1}, {1, 0, 0, 1}},
        // Back (green)
        {{ 0.5F, -0.5F, -0.5F}, { 0, 0,-1}, {0, 1, 0, 1}},
        {{-0.5F, -0.5F, -0.5F}, { 0, 0,-1}, {0, 1, 0, 1}},
        {{-0.5F,  0.5F, -0.5F}, { 0, 0,-1}, {0, 1, 0, 1}},
        {{ 0.5F,  0.5F, -0.5F}, { 0, 0,-1}, {0, 1, 0, 1}},
        // Top (blue)
        {{-0.5F,  0.5F,  0.5F}, { 0, 1, 0}, {0, 0, 1, 1}},
        {{ 0.5F,  0.5F,  0.5F}, { 0, 1, 0}, {0, 0, 1, 1}},
        {{ 0.5F,  0.5F, -0.5F}, { 0, 1, 0}, {0, 0, 1, 1}},
        {{-0.5F,  0.5F, -0.5F}, { 0, 1, 0}, {0, 0, 1, 1}},
        // Bottom (yellow)
        {{-0.5F, -0.5F, -0.5F}, { 0,-1, 0}, {1, 1, 0, 1}},
        {{ 0.5F, -0.5F, -0.5F}, { 0,-1, 0}, {1, 1, 0, 1}},
        {{ 0.5F, -0.5F,  0.5F}, { 0,-1, 0}, {1, 1, 0, 1}},
        {{-0.5F, -0.5F,  0.5F}, { 0,-1, 0}, {1, 1, 0, 1}},
        // Right (cyan)
        {{ 0.5F, -0.5F,  0.5F}, { 1, 0, 0}, {0, 1, 1, 1}},
        {{ 0.5F, -0.5F, -0.5F}, { 1, 0, 0}, {0, 1, 1, 1}},
        {{ 0.5F,  0.5F, -0.5F}, { 1, 0, 0}, {0, 1, 1, 1}},
        {{ 0.5F,  0.5F,  0.5F}, { 1, 0, 0}, {0, 1, 1, 1}},
        // Left (magenta)
        {{-0.5F, -0.5F, -0.5F}, {-1, 0, 0}, {1, 0, 1, 1}},
        {{-0.5F, -0.5F,  0.5F}, {-1, 0, 0}, {1, 0, 1, 1}},
        {{-0.5F,  0.5F,  0.5F}, {-1, 0, 0}, {1, 0, 1, 1}},
        {{-0.5F,  0.5F, -0.5F}, {-1, 0, 0}, {1, 0, 1, 1}},
    };
    // clang-format on

    indices = {
        0,  1,  2,  2,  3,  0,   // front
        4,  5,  6,  6,  7,  4,   // back
        8,  9,  10, 10, 11, 8,   // top
        12, 13, 14, 14, 15, 12,  // bottom
        16, 17, 18, 18, 19, 16,  // right
        20, 21, 22, 22, 23, 20,  // left
    };
}

}  // namespace

class CubeRenderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        s_window = glfwCreateWindow(800, 600, "CubeRenderTest", nullptr, nullptr);
        ASSERT_NE(s_window, nullptr);

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
        s_context = std::make_unique<VulkanContext>(true, requiredExts);

        VkResult result = glfwCreateWindowSurface(
            s_context->GetInstance(), s_window, nullptr, &s_surface);
        ASSERT_EQ(result, VK_SUCCESS);

        s_device = std::make_unique<Device>(s_context->GetInstance(), s_surface);
        s_swapchain = std::make_unique<Swapchain>(*s_device, s_surface, VkExtent2D{800, 600});
        s_renderLoop = std::make_unique<RenderLoop>(*s_device, *s_swapchain, BIMEUP_SHADER_DIR);
    }

    static void TearDownTestSuite() {
        s_renderLoop.reset();
        s_swapchain.reset();
        s_device.reset();
        if (s_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(s_context->GetInstance(), s_surface, nullptr);
            s_surface = VK_NULL_HANDLE;
        }
        s_context.reset();
        if (s_window != nullptr) {
            glfwDestroyWindow(s_window);
            s_window = nullptr;
        }
        glfwTerminate();
    }

    void SetUp() override {
        m_device = s_device.get();
        m_swapchain = s_swapchain.get();
        m_renderLoop = s_renderLoop.get();
    }

    Device* m_device = nullptr;
    Swapchain* m_swapchain = nullptr;
    RenderLoop* m_renderLoop = nullptr;

    static GLFWwindow* s_window;
    static VkSurfaceKHR s_surface;
    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static std::unique_ptr<Swapchain> s_swapchain;
    static std::unique_ptr<RenderLoop> s_renderLoop;
};

GLFWwindow* CubeRenderTest::s_window = nullptr;
VkSurfaceKHR CubeRenderTest::s_surface = VK_NULL_HANDLE;
std::unique_ptr<VulkanContext> CubeRenderTest::s_context;
std::unique_ptr<Device> CubeRenderTest::s_device;
std::unique_ptr<Swapchain> CubeRenderTest::s_swapchain;
std::unique_ptr<RenderLoop> CubeRenderTest::s_renderLoop;

TEST_F(CubeRenderTest, RenderLoopProvidesRenderPass) {
    EXPECT_NE(m_renderLoop->GetRenderPass(), VK_NULL_HANDLE);
}

TEST_F(CubeRenderTest, RenderCubeFrameWithoutErrors) {
    VkRenderPass renderPass = m_renderLoop->GetRenderPass();
    ASSERT_NE(renderPass, VK_NULL_HANDLE);

    // Load shaders
    std::string vertPath = std::string(BIMEUP_SHADER_DIR) + "/basic.vert.spv";
    std::string fragPath = std::string(BIMEUP_SHADER_DIR) + "/basic.frag.spv";
    ASSERT_TRUE(std::filesystem::exists(vertPath));
    ASSERT_TRUE(std::filesystem::exists(fragPath));

    Shader vertShader(*m_device, ShaderStage::Vertex, vertPath);
    Shader fragShader(*m_device, ShaderStage::Fragment, fragPath);

    // Descriptor set layout (camera UBO at binding 0)
    DescriptorSetLayout dsLayout(*m_device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
    });

    DescriptorPool dsPool(*m_device, 1, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    });

    DescriptorSet descriptorSet(*m_device, dsPool, dsLayout);

    // Camera UBO
    CameraUBO ubo{};
    ubo.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0), glm::vec3(0, 1, 0));
    ubo.projection = glm::perspective(glm::radians(45.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    ubo.projection[1][1] *= -1;  // Vulkan Y-flip

    Buffer uboBuffer(*m_device, BufferType::Uniform, sizeof(CameraUBO), &ubo);
    descriptorSet.UpdateBuffer(0, uboBuffer);

    // Vertex + index data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    MakeCube(vertices, indices);

    Buffer vertexBuffer(*m_device, BufferType::Vertex,
                        vertices.size() * sizeof(Vertex), vertices.data());
    Buffer indexBuffer(*m_device, BufferType::Index,
                       indices.size() * sizeof(uint32_t), indices.data());

    // Pipeline config
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrs(3);
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)};

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4);

    // RP.12b — basic.frag declares a fragment-stage push constant block
    // with one uint at offset 64 (`transparentBit`). Pipelines binding
    // basic.frag must declare the matching range or validation flags the
    // shader/layout mismatch. The cube test stands in as the renderer-side
    // regression guard for the layout shape main.cpp uses.
    VkPushConstantRange fragPushRange{};
    fragPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragPushRange.offset = 64;
    fragPushRange.size = sizeof(uint32_t);

    PipelineConfig pipelineConfig{};
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.vertexBindings = {binding};
    pipelineConfig.vertexAttributes = {attrs.begin(), attrs.end()};
    pipelineConfig.descriptorSetLayouts = {dsLayout.GetLayout()};
    pipelineConfig.pushConstantRanges = {pushRange, fragPushRange};
    pipelineConfig.depthTestEnable = true;
    pipelineConfig.depthWriteEnable = true;
    // RenderLoop's main pass is MRT (HDR + normal G-buffer + transparency stencil).
    // basic.frag writes all three, so keep the secondary writes enabled.
    pipelineConfig.colorAttachmentCount = 3;
    pipelineConfig.disableSecondaryColorWrites = false;

    Pipeline pipeline(*m_device, vertShader, fragShader, pipelineConfig);

    // Render one frame
    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);

    VkCommandBuffer cmd = m_renderLoop->GetCurrentCommandBuffer();
    VkExtent2D extent = m_swapchain->GetExtent();

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    pipeline.Bind(cmd);
    descriptorSet.Bind(cmd, pipeline.GetLayout());

    glm::mat4 model(1.0F);
    vkCmdPushConstants(cmd, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &model);
    // Push the RP.12b fragment-stage range: transparentBit=4 (transparent
    // pipeline path — exercises the non-zero path so a missing fragment
    // range in the pipeline layout surfaces under validation).
    const uint32_t fragPush = 0x4U;
    vkCmdPushConstants(cmd, pipeline.GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT,
                       64, sizeof(fragPush), &fragPush);

    VkBuffer vb = vertexBuffer.GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    bool ended = m_renderLoop->EndFrame();
    EXPECT_TRUE(ended);

    m_renderLoop->WaitIdle();
}
