#include <gtest/gtest.h>
#include <renderer/AccelerationStructure.h>
#include <renderer/Device.h>
#include <renderer/MeshBuffer.h>
#include <renderer/RenderLoop.h>
#include <renderer/Swapchain.h>
#include <renderer/TopLevelAS.h>
#include <renderer/VulkanContext.h>

#include <glm/gtc/matrix_transform.hpp>

#include <span>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using bimeup::renderer::AccelerationStructure;
using bimeup::renderer::Device;
using bimeup::renderer::MeshData;
using bimeup::renderer::RenderLoop;
using bimeup::renderer::Swapchain;
using bimeup::renderer::TlasInstance;
using bimeup::renderer::TopLevelAS;
using bimeup::renderer::VulkanContext;

// Vulkan context + device + swapchain are shared across all tests in this
// suite. Setup is the dominant cost (~1.5 s validation-layer load + device
// init), so paying it once instead of per-test cuts the suite from ~1 min to
// well under 15 s when the per-fixture ctest entry runs the binary in batch.
// Per-test state lives entirely on m_renderLoop, which TearDown resets.
class RenderLoopTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        s_window = glfwCreateWindow(800, 600, "RenderLoopTest", nullptr, nullptr);
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
    }

    static void TearDownTestSuite() {
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
    }

    void TearDown() override {
        m_renderLoop.reset();
    }

    Device* m_device = nullptr;
    Swapchain* m_swapchain = nullptr;
    std::unique_ptr<RenderLoop> m_renderLoop;

    static GLFWwindow* s_window;
    static VkSurfaceKHR s_surface;
    static std::unique_ptr<VulkanContext> s_context;
    static std::unique_ptr<Device> s_device;
    static std::unique_ptr<Swapchain> s_swapchain;
};

GLFWwindow* RenderLoopTest::s_window = nullptr;
VkSurfaceKHR RenderLoopTest::s_surface = VK_NULL_HANDLE;
std::unique_ptr<VulkanContext> RenderLoopTest::s_context;
std::unique_ptr<Device> RenderLoopTest::s_device;
std::unique_ptr<Swapchain> RenderLoopTest::s_swapchain;

// RP.14.1.a — symmetric guard against MSAA resurrection. SMAA covers
// architectural AA and MSAA gated XeGTAO / depth-pyramid off, which is the
// wrong tradeoff for a BIM viewer. If someone adds SetSampleCount /
// GetSampleCount back to RenderLoop, this concept becomes satisfied and the
// static_assert breaks the build.
template <typename T>
concept RenderLoopExposesMsaaAccessors =
    requires(T& mutableLoop, const T& constLoop) {
        constLoop.GetSampleCount();
        mutableLoop.SetSampleCount(VK_SAMPLE_COUNT_1_BIT);
    };
static_assert(!RenderLoopExposesMsaaAccessors<RenderLoop>,
              "RP.14.1.a — RenderLoop::SetSampleCount/GetSampleCount retired; "
              "do not bring MSAA back without revisiting the XeGTAO / "
              "depth-pyramid gates.");

// RP.15.a — symmetric guard against selection-outline resurrection. The
// screen-space outline pass made the model "blink" on hover and was retired in
// favour of the simpler vertex-colour fill that already runs from
// `Selection::SetOnChanged`. If someone re-adds `SetOutlineParams`, this concept
// is satisfied and the static_assert breaks the build.
template <typename T>
concept RenderLoopExposesOutlineParams =
    requires(T& loop) { loop.SetOutlineParams({}, true); };
static_assert(!RenderLoopExposesOutlineParams<RenderLoop>,
              "RP.15.a — RenderLoop::SetOutlineParams retired with the outline "
              "pass; selection visualisation is the vertex-colour fill applied "
              "in main.cpp via Selection::SetOnChanged.");

TEST_F(RenderLoopTest, CreatesSuccessfully) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
}

TEST_F(RenderLoopTest, SingleFrameCycle) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);

    EXPECT_NE(m_renderLoop->GetCurrentCommandBuffer(), VK_NULL_HANDLE);

    bool ended = m_renderLoop->EndFrame();
    EXPECT_TRUE(ended);

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, MultipleFrames) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    for (int i = 0; i < 5; ++i) {
        bool began = m_renderLoop->BeginFrame();
        ASSERT_TRUE(began);
        bool ended = m_renderLoop->EndFrame();
        EXPECT_TRUE(ended);
    }

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, FrameIndexCycles) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    for (uint32_t i = 0; i < 4; ++i) {
        EXPECT_EQ(m_renderLoop->GetCurrentFrameIndex(),
                  i % RenderLoop::MAX_FRAMES_IN_FLIGHT);

        bool began = m_renderLoop->BeginFrame();
        ASSERT_TRUE(began);
        EXPECT_TRUE(m_renderLoop->EndFrame());
    }

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, PreMainPassCallbackInvokedEachFrame) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    int calls = 0;
    VkCommandBuffer capturedCmd = VK_NULL_HANDLE;
    m_renderLoop->SetPreMainPassCallback([&](VkCommandBuffer cmd) {
        ++calls;
        capturedCmd = cmd;
    });

    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);
    // Callback must have run during BeginFrame with the frame's active command buffer.
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(capturedCmd, m_renderLoop->GetCurrentCommandBuffer());

    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, SetClearColor) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);

    m_renderLoop->SetClearColor(1.0f, 0.0f, 0.0f);

    bool began = m_renderLoop->BeginFrame();
    ASSERT_TRUE(began);
    bool ended = m_renderLoop->EndFrame();
    EXPECT_TRUE(ended);

    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, DestructorCleansUp) {
    {
        RenderLoop loop(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
        bool began = loop.BeginFrame();
        ASSERT_TRUE(began);
        EXPECT_TRUE(loop.EndFrame());
        loop.WaitIdle();
    }
    // Validation layers + sanitizers would catch leaks
}

// RP.3c — MRT normal G-buffer: the main render pass has a second R16G16_SNORM
// colour attachment per swap image, rebuilt in lockstep with the HDR target.
TEST_F(RenderLoopTest, NormalFormatIsR16G16Snorm) {
    EXPECT_EQ(RenderLoop::NORMAL_FORMAT, VK_FORMAT_R16G16_SNORM);
}

TEST_F(RenderLoopTest, NormalGBufferImageViewsProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetNormalImageView(i), VK_NULL_HANDLE)
            << "normal G-buffer view missing for swap image " << i;
    }
}

// RP.6c → RP.15.b — MRT transparency stencil G-buffer: the main render
// pass has a third R8_UINT colour attachment per swap image; bit 2 (value
// 4) marks transparent surfaces (RP.12b), other bits unused. Sampled by
// `ssao_xegtao.comp` as the transparency gate. Layout mirrors the normal
// G-buffer — per-swap-image single-sample target (RP.14.1.a retired the
// transient MSAA sibling along with the rest of the MSAA path).
TEST_F(RenderLoopTest, StencilFormatIsR8Uint) {
    EXPECT_EQ(RenderLoop::STENCIL_FORMAT, VK_FORMAT_R8_UINT);
}

TEST_F(RenderLoopTest, StencilGBufferImageViewsProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetStencilImageView(i), VK_NULL_HANDLE)
            << "stencil G-buffer view missing for swap image " << i;
    }
}

// RP.12b (slimmed in RP.15.b) — bit 2 (value 4) = "transparent surface" is
// the only live bit in the stencil G-buffer post-outline-retirement; the
// only values are {0, 4}. R8_UINT trivially holds that, but pin the
// contract so a future format downgrade trips this test rather than
// silently breaking the XeGTAO transparency gate.
//
// Limits of this test: it does NOT exercise the basic.frag push or the
// main.cpp transparent-pipeline `transparentBit = 4` push — those are
// caught by Vulkan validation layers on any draw and by visual smoke
// testing of glass rendering.
TEST_F(RenderLoopTest, StencilFormatHoldsTransparentBit) {
    EXPECT_EQ(RenderLoop::STENCIL_FORMAT, VK_FORMAT_R8_UINT);
    static_assert(0x4U <= 0xFFU,
                  "RP.15.b stencil range {0, 4} must fit in R8");
}

// RP.4d — Linear depth + depth pyramid: a 4-mip R32_SFLOAT pyramid per swap
// image, built by a compute pass between the main HDR pass and the tonemap
// pass. Runs every frame now (RP.14.1.a retired the MSAA gate along with the
// rest of the MSAA path).
TEST_F(RenderLoopTest, DepthPyramidFormatIsR32Sfloat) {
    EXPECT_EQ(RenderLoop::DEPTH_PYRAMID_FORMAT, VK_FORMAT_R32_SFLOAT);
}

TEST_F(RenderLoopTest, DepthPyramidMipCountIsFour) {
    EXPECT_EQ(RenderLoop::DEPTH_PYRAMID_MIPS, 4u);
}

TEST_F(RenderLoopTest, DepthPyramidViewProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetDepthPyramidView(i), VK_NULL_HANDLE)
            << "depth pyramid view missing for swap image " << i;
    }
}

TEST_F(RenderLoopTest, DepthPyramidBuiltDuringFrame) {
    // Exercises the full per-frame compute dispatch (linearize + 3 mip levels).
    // Validation layers + sanitizers guard barrier/sync mistakes.
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// RP.5d — SSAO compute pass wired into RenderLoop: per-swap-image half-res
// R8 AO target, main + 2 blur dispatches between the depth-pyramid build
// and the tonemap pass, AO sampled by tonemap.frag and multiplied into the
// HDR colour. Runs every frame now (RP.14.1.a retired the MSAA gate along
// with the rest of the MSAA path); AO image is still cleared to 1.0 at
// creation so the tonemap multiply starts as a no-op before the first
// SSAO dispatch lands.
TEST_F(RenderLoopTest, AoFormatIsR8Unorm) {
    EXPECT_EQ(RenderLoop::AO_FORMAT, VK_FORMAT_R8_UNORM);
}

TEST_F(RenderLoopTest, AoViewProvidedPerSwapImage) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const uint32_t imageCount = m_swapchain->GetImageCount();
    ASSERT_GT(imageCount, 0u);
    for (uint32_t i = 0; i < imageCount; ++i) {
        EXPECT_NE(m_renderLoop->GetAoImageView(i), VK_NULL_HANDLE)
            << "AO view missing for swap image " << i;
    }
}

TEST_F(RenderLoopTest, SsaoDispatchedDuringFrame) {
    // Exercises the full per-frame compute chain (pyramid + SSAO main +
    // 2 blur passes) under Vulkan validation.
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// RP.11c — SMAA 3-pass chain replaces FXAA. Edge + weights passes render
// into per-swap RGBA8 intermediates via a shared SMAA render pass; the
// blend pass writes the final AA'd LDR pixel into the swapchain inside the
// existing present pass. The edge/weights intermediates plus the vendored
// AreaTex + SearchTex LUTs are owned by RenderLoop; ImGui still draws on
// top of the blend result in the in-present callback.
TEST_F(RenderLoopTest, SmaaAppliedDuringFrame) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    m_renderLoop->SetSmaaParams(/*enabled=*/true, 0.1F, 16, 8);
    // Full frame under Vulkan validation with SMAA enabled — exercises the
    // 3-pass chain (edge → weights → blend), the shared LUT bindings in
    // `smaa_weights.frag` (AreaTex + SearchTex), and the descriptor
    // updates that tie edges/weights views to the per-swap targets.
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

TEST_F(RenderLoopTest, SmaaDisabledStillCyclesFrame) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    m_renderLoop->SetSmaaParams(/*enabled=*/false, 0.1F, 16, 8);
    // Disabled path — edge + weights passes are skipped but the blend pass
    // still runs in the present pass as a passthrough (push-constant
    // `enabled` flag short-circuits before the weights sample in
    // `smaa_blend.frag`). A regression that gated the blend draw off
    // entirely would leave the swapchain image undefined and trip
    // validation on present; a regression that left the shader sampling
    // stale weights would still work here but the flag's unit-level
    // contract is pinned by `SmaaBlendPipelinePushConstants`.
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// Stage 9.4.b — RT render mode plumbing. Default is Rasterised: every
// existing test above implicitly relies on this, and the classical renderer
// must stay the out-of-the-box experience (`CLAUDE.md` + Stage-9 ground
// rules). Switching to Hybrid RT flips on the BLAS/TLAS + RtShadowPass
// lifecycle; switching back tears them down. Visibility stays un-composited
// in this task — 9.8 wires the actual contribution into the tonemap.
TEST_F(RenderLoopTest, DefaultRenderModeIsRasterised) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    EXPECT_EQ(m_renderLoop->GetRenderMode(), RenderLoop::RenderMode::Rasterised);
}

TEST_F(RenderLoopTest, SetRenderModeRoundTrips) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetRenderMode(RenderLoop::RenderMode::HybridRt);
    EXPECT_EQ(m_renderLoop->GetRenderMode(), RenderLoop::RenderMode::HybridRt);
    m_renderLoop->SetRenderMode(RenderLoop::RenderMode::Rasterised);
    EXPECT_EQ(m_renderLoop->GetRenderMode(), RenderLoop::RenderMode::Rasterised);
}

// No TLAS provided: the per-frame dispatch must safely short-circuit so the
// classical frame still cycles. Runs on every device — exercises the no-TLAS
// branch of the dispatch gate.
TEST_F(RenderLoopTest, HybridRtModeCyclesFrameWithoutTlas) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);
    m_renderLoop->SetRenderMode(RenderLoop::RenderMode::HybridRt);
    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// Full RT path: build a minimal BLAS/TLAS, hand them to the RenderLoop, and
// exercise the per-frame shadow dispatch under Vulkan validation. Skipped on
// non-RT devices — matches the Stage-9 rule that classical renderer stays
// live when RT is unavailable.
TEST_F(RenderLoopTest, HybridRtModeWithTlasCyclesFrameOnRtDevice) {
    if (!m_device->HasRayTracing()) {
        GTEST_SKIP() << "Device does not advertise RT — dispatch path skipped";
    }

    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    const glm::mat4 proj = glm::perspective(glm::radians(60.0F), 800.0F / 600.0F, 0.1F, 100.0F);
    const glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 2), glm::vec3(0), glm::vec3(0, 1, 0));
    m_renderLoop->SetProjection(proj, 0.1F, 100.0F);

    MeshData tri;
    tri.vertices = {
        {{0.0F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {1, 0, 0, 1}},
        {{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0, 1, 0, 1}},
        {{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}, {0, 0, 1, 1}},
    };
    tri.indices = {0, 1, 2};

    AccelerationStructure accel(*m_device);
    auto blas = accel.BuildBlas(tri);
    ASSERT_NE(blas, AccelerationStructure::InvalidHandle);
    TopLevelAS tlas(*m_device);
    TlasInstance inst{glm::mat4(1.0F), accel.GetDeviceAddress(blas), 0, 0xFF};
    ASSERT_TRUE(tlas.Build({inst}));

    m_renderLoop->SetRenderMode(RenderLoop::RenderMode::HybridRt);
    m_renderLoop->SetRtShadowInputs(tlas.GetHandle(),
                                    glm::normalize(glm::vec3(-1, -2, -1)), view);
    m_renderLoop->SetRtAoInputs(1.0F);
    // Stage 9.7.b — indoor-fill pass is live once HybridRt is selected
    // (view is allocated); the per-frame dispatch gate on `enabled`
    // decides whether a trace actually runs. With enabled=true plus a
    // valid TLAS, the dispatch records under validation during the
    // EndFrame below.
    m_renderLoop->SetRtIndoorInputs(glm::normalize(glm::vec3(0.2F, -1.0F, 0.3F)),
                                    /*enabled=*/true);
    EXPECT_NE(m_renderLoop->GetRtShadowVisibilityView(), VK_NULL_HANDLE);
    EXPECT_NE(m_renderLoop->GetRtAoImageView(), VK_NULL_HANDLE);
    EXPECT_NE(m_renderLoop->GetRtIndoorVisibilityView(), VK_NULL_HANDLE);

    ASSERT_TRUE(m_renderLoop->BeginFrame());
    EXPECT_TRUE(m_renderLoop->EndFrame());
    m_renderLoop->WaitIdle();
}

// Flipping Hybrid RT → Rasterised must release the RT resources (visibility
// image, descriptors, pipeline) so the classical frame pays no RT cost —
// mirrors the Stage-9 "BLAS/TLAS lifecycle gated on mode" ground rule.
TEST_F(RenderLoopTest, SwitchingBackToRasterisedReleasesRtResources) {
    m_renderLoop = std::make_unique<RenderLoop>(*m_device, *m_swapchain, BIMEUP_SHADER_DIR);
    m_renderLoop->SetRenderMode(RenderLoop::RenderMode::HybridRt);
    m_renderLoop->SetRenderMode(RenderLoop::RenderMode::Rasterised);
    EXPECT_EQ(m_renderLoop->GetRtShadowVisibilityView(), VK_NULL_HANDLE);
    EXPECT_EQ(m_renderLoop->GetRtAoImageView(), VK_NULL_HANDLE);
    EXPECT_EQ(m_renderLoop->GetRtIndoorVisibilityView(), VK_NULL_HANDLE);
}

