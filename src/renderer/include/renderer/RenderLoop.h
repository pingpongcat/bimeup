#pragma once

#include <renderer/SmaaBlendPipeline.h>
#include <renderer/SmaaEdgePipeline.h>
#include <renderer/SmaaWeightsPipeline.h>
#include <renderer/TonemapPipeline.h>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bimeup::renderer {

class Device;
class Swapchain;
class Shader;
class Buffer;
class TonemapPipeline;
class DepthLinearizePipeline;
class DepthMipPipeline;
class SsaoXeGtaoPipeline;
class SsaoBlurPipeline;
class RtShadowPass;
class RtAoPass;
class RtIndoorPass;
class RtSunCompositePipeline;
class RtIndoorCompositePipeline;

class RenderLoop {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    // HDR offscreen format used between the scene render pass and the tonemap
    // resolve pass. Matches the Stage RP plan — enough dynamic range to keep
    // bright lighting from clipping before the curve.
    static constexpr VkFormat HDR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    // Normal G-buffer format — octahedron-packed view-space normal, two
    // signed 16-bit channels. Attachment 1 of the main render pass. Clear
    // value (0,0) decodes to +Z so overlay pipelines that leave this
    // attachment untouched (disableSecondaryColorWrites) don't corrupt
    // SSAO sampling downstream.
    static constexpr VkFormat NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
    // Transparency stencil G-buffer format (RP.6c → RP.12b, slimmed in
    // RP.15.b). Single-channel unsigned 8-bit integer: bit 2 (value 4) is
    // the "transparent surface" flag written by `basic.frag` for the
    // transparent pipeline; other bits unused. Attachment 2 of the main
    // render pass. Sampled by `ssao_xegtao.comp` as a `usampler2D` so
    // XeGTAO can treat glass as transparent. Clear value is 0 so unwritten
    // pixels read as opaque.
    static constexpr VkFormat STENCIL_FORMAT = VK_FORMAT_R8_UINT;
    // Depth pyramid format — view-space linear depth, single-channel float.
    // Used by the RP.4c/d depth_linearize + depth_mip compute chain and
    // sampled by SSAO (RP.5/RP.12e).
    static constexpr VkFormat DEPTH_PYRAMID_FORMAT = VK_FORMAT_R32_SFLOAT;
    // Mip levels in the depth pyramid (mip 0 = full res + 3 downsampled
    // levels). Matches the PLAN — enough range for SSAO's adaptive-radius
    // mip offset, not so many that the tail mips become 1×1 noise.
    static constexpr uint32_t DEPTH_PYRAMID_MIPS = 4;
    // SSAO half-res AO output format (RP.5d). R8_UNORM — classic
    // Chapman/LearnOpenGL AO is a single scalar; 8 bits is enough after the
    // separable blur smooths away the hemisphere-sample noise. Sampled by
    // the tonemap fragment shader and multiplied into the HDR colour.
    static constexpr VkFormat AO_FORMAT = VK_FORMAT_R8_UNORM;

    /// Stage 9 — render-mode selector. Classical rasterised stays the
    /// default on every launch; Hybrid RT is opt-in and gated on the
    /// device's `VK_KHR_ray_tracing_pipeline` capability (see
    /// `Device::HasRayTracing()`). On non-RT devices, setting the mode to
    /// `HybridRt` is accepted as state but the per-frame RT dispatch
    /// short-circuits — the classical frame still cycles unchanged.
    /// Stage 9.Q.4 — `RayQuery` is the new pivot mode: still the
    /// forward-shaded raster path, but `basic.frag`'s sun-shadow PCF tap
    /// swaps for an inline `rayQueryEXT` trace. Gated on
    /// `Device::HasRayQuery()`. The renderer doesn't need any per-frame
    /// dispatch for this mode — it's purely a flag main.cpp reads to
    /// push `useRayQueryShadow = 1` per opaque draw.
    enum class RenderMode : uint8_t {
        Rasterised = 0,
        HybridRt = 1,
        RayQuery = 2,
    };

    RenderLoop(const Device& device, Swapchain& swapchain,
               const std::string& shaderDir);
    ~RenderLoop();

    RenderLoop(const RenderLoop&) = delete;
    RenderLoop& operator=(const RenderLoop&) = delete;
    RenderLoop(RenderLoop&&) = delete;
    RenderLoop& operator=(RenderLoop&&) = delete;

    [[nodiscard]] bool BeginFrame();
    [[nodiscard]] bool EndFrame();

    void WaitIdle();

    void SetClearColor(float r, float g, float b, float a = 1.0f);

    /// Register a callback invoked inside BeginFrame() after the command buffer is
    /// begun but BEFORE the main scene render pass starts — suitable for
    /// recording a depth-only shadow pass whose output is sampled by the main pass.
    using PreMainPassCallback = std::function<void(VkCommandBuffer)>;
    void SetPreMainPassCallback(PreMainPassCallback callback);

    /// Register a callback invoked while the tonemap/present pass is active,
    /// AFTER the internal fullscreen tonemap draw has been recorded. This is
    /// where UI (ImGui) draw data is emitted so it lands on top of the
    /// tonemapped image instead of being fed through the ACES curve twice.
    using InPresentPassCallback = std::function<void(VkCommandBuffer)>;
    void SetInPresentPassCallback(InPresentPassCallback callback);

    [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const;
    [[nodiscard]] uint32_t GetCurrentFrameIndex() const;
    [[nodiscard]] uint32_t GetCurrentImageIndex() const;
    /// Render pass for scene geometry — HDR colour + depth targeting the
    /// offscreen HDR image. Bind scene pipelines here. Sample count is
    /// always 1× (MSAA retired in RP.14.1.a).
    [[nodiscard]] VkRenderPass GetRenderPass() const;
    /// Render pass for the final HDR→LDR tonemap + UI overlay targeting the
    /// swapchain. Bind ImGui and any other post-tonemap pipelines here;
    /// sample count is always 1× (the swapchain is never multisampled).
    [[nodiscard]] VkRenderPass GetPresentRenderPass() const;
    /// View into the single-sample normal G-buffer for the given swap image
    /// index (same index domain as `GetCurrentImageIndex()`). Read by SSAO.
    /// Returns VK_NULL_HANDLE only if the index is out of range.
    [[nodiscard]] VkImageView GetNormalImageView(uint32_t imageIndex) const;
    /// View into the single-sample transparency stencil G-buffer (R8_UINT)
    /// for the given swap image index (same index domain as
    /// `GetCurrentImageIndex()`). Sampled by `ssao_xegtao.comp` as a
    /// `usampler2D` — bit 2 marks transparent surfaces (RP.12b / RP.15.b).
    /// Returns VK_NULL_HANDLE only if the index is out of range.
    [[nodiscard]] VkImageView GetStencilImageView(uint32_t imageIndex) const;
    /// Full-mip-chain sampled view of the depth pyramid for the given swap
    /// image index (R32_SFLOAT, mips 0..DEPTH_PYRAMID_MIPS-1 of view-space
    /// linear depth). Bind with a mipmap-aware sampler and select mips via
    /// `textureLod`; downstream SSAO will consume this in later RP
    /// tasks. Returns VK_NULL_HANDLE if the index is out of range.
    [[nodiscard]] VkImageView GetDepthPyramidView(uint32_t imageIndex) const;
    /// Final post-blur AO target (R8_UNORM, half-res) for the given swap
    /// image. Written by the RP.5d SSAO compute chain and sampled by the
    /// tonemap fragment shader. Returns VK_NULL_HANDLE if the index is out
    /// of range.
    [[nodiscard]] VkImageView GetAoImageView(uint32_t imageIndex) const;

    /// Camera projection matrix + near/far plane values. Projection feeds
    /// the SSAO UBO (mat4 proj + mat4 invProj) so view-space sample
    /// positions project back to UV; near/far feed the depth-linearize
    /// compute push constants so pyramid mip 0 matches the CPU mirror
    /// `renderer::LinearizeDepth`. Call each frame after the camera has
    /// been updated. Defaults are (identity, 0.1, 10000).
    void SetProjection(const glm::mat4& proj, float nearZ, float farZ);

    /// RP.11c / RP.19 — SMAA 1x post-process settings. The blend draw is the
    /// only path from the LDR intermediate to the swapchain, so it runs every
    /// frame; when `enabled` is false the edge + weights passes are skipped
    /// and the blend shader's push-constant gate short-circuits to a
    /// passthrough via `smaa_blend.frag`'s early-return branch — 3 draws →
    /// 1 draw and stale weights can't influence the output. `threshold` is
    /// the luma gate in `smaa_edge.frag`; `maxSearchSteps` +
    /// `maxSearchStepsDiag` drive `smaa_weights.frag`'s iryoku LOW/MED/HIGH
    /// search budget (previously hardcoded at HIGH = 16 / 8).
    void SetSmaaParams(bool enabled, float threshold, int maxSearchSteps,
                       int maxSearchStepsDiag);

    /// RP.20 — XeGTAO knobs fed into `ssao_xegtao.comp`'s push constants
    /// each frame. Defaults mirror the pre-RP.20 hardcoded values in
    /// `RunXeGtao`; a panel that never touches these sliders produces
    /// bit-compatible output.
    void SetSsaoParams(float radius, float falloff, float intensity, float shadowPower);

    /// Pre-ACES exposure multiplier applied to the composited HDR colour
    /// inside `tonemap.frag`. Default (from the push-constant ctor) is 1.0
    /// — identity. The panel seeds a scene-appropriate value so the three-
    /// point lighting (ambient + key + fill + rim can sum to ~2.x on a
    /// bright surface) doesn't saturate the ACES curve into near-white.
    void SetExposure(float exposure);

    [[nodiscard]] VkSampleCountFlagBits GetPresentSampleCount() const {
        return VK_SAMPLE_COUNT_1_BIT;
    }

    /// Rebuild HDR/depth images, framebuffers, and tonemap descriptor
    /// bindings to match the current swapchain extent. Call after
    /// `Swapchain::Recreate(...)` on a resize. The render pass handles are
    /// preserved (format/samples unchanged) so pipelines stay valid.
    void RecreateForSwapchain();

    /// Stage 9.4.b — select render mode. Flipping to `HybridRt` lazy-builds
    /// the `RtShadowPass` at the current swapchain extent (no-op when the
    /// device advertises no RT — `HybridRt` still reports via
    /// `GetRenderMode` so the UI state is honest, but the per-frame
    /// dispatch is skipped). Flipping back to `Rasterised` tears the RT
    /// resources down so the classical frame pays zero RT cost.
    void SetRenderMode(RenderMode mode);
    [[nodiscard]] RenderMode GetRenderMode() const { return m_renderMode; }

    /// Stage 9.4.b — per-frame inputs for the RT sun-shadow pass. The caller
    /// owns the BLAS/TLAS lifecycle (build + rebuild on scene edits) and
    /// hands the TLAS handle in here each frame alongside the current sun
    /// direction (world-space travel direction, matches
    /// `DirectionalLight::direction`) and the camera view matrix used to
    /// reconstruct world-space positions in the raygen. Pass
    /// `VK_NULL_HANDLE` for the TLAS to temporarily skip the dispatch
    /// without leaving Hybrid RT mode (e.g. between scene loads).
    void SetRtShadowInputs(VkAccelerationStructureKHR tlas,
                           const glm::vec3& sunDirWorld, const glm::mat4& view);

    /// Sampled view of the RT shadow-pass visibility image (R8_UNORM,
    /// `SHADER_READ_ONLY_OPTIMAL` after each dispatch). Stage 9.8 will
    /// wire this into the hybrid composite; before then it's observable
    /// but not consumed. Returns `VK_NULL_HANDLE` when RT mode is off or
    /// the device lacks RT support.
    [[nodiscard]] VkImageView GetRtShadowVisibilityView() const;

    /// Stage 9.5.b — per-frame input for the RT AO pass. TLAS and view
    /// matrix are re-used from `SetRtShadowInputs` (same scene, same
    /// camera), so this setter only takes the AO-specific knob: the
    /// world-space ray max-distance in metres. Default 1.0 matches the
    /// architectural scale in the Stage-9 plan. The per-pixel random
    /// seed's frame-index component is incremented internally on each
    /// dispatch so a future 9.5.c can drop a temporal accumulator on
    /// top without changing this API.
    void SetRtAoInputs(float radius);

    /// Sampled view of the RT AO pass image (R8_UNORM,
    /// `SHADER_READ_ONLY_OPTIMAL` after each dispatch). Stage 9.8.a wires
    /// this into the tonemap composite when `HybridRt` is selected on an
    /// RT-capable device; see `IsRtAoSourcedInTonemap()`. `VK_NULL_HANDLE`
    /// when RT mode is off or the device lacks RT support.
    [[nodiscard]] VkImageView GetRtAoImageView() const;

    /// Stage 9.8.a — true when the tonemap's AO sample comes from the RT
    /// AO pass instead of XeGTAO. Set by `SetRenderMode(HybridRt)` on an
    /// RT-capable device; cleared by the flip back to `Rasterised` (or
    /// when the device can't build the RT AO pass). The raster path is
    /// bit-compatible whenever this is false — `tonemap.frag`'s
    /// `useRtAo` push constant degenerates the mix to the XeGTAO sample.
    [[nodiscard]] bool IsRtAoSourcedInTonemap() const { return m_tonemapUseRtAo; }

    /// Stage 9.7.b — per-frame inputs for the RT indoor-fill pass. TLAS
    /// and view are re-used from `SetRtShadowInputs`. `fillDirWorld` is
    /// the overhead fill's travel direction (matches
    /// `DirectionalLight::direction` — points *away* from the virtual
    /// overhead lamp); the raygen inverts it to shoot shadow rays
    /// toward the light. `enabled` mirrors the scene's
    /// `indoorLightsEnabled` — when false the per-frame dispatch is
    /// skipped so zero RT cost is paid while the user has indoor lights
    /// off, and `basic.frag`'s cheap directional fill remains the sole
    /// fill source.
    void SetRtIndoorInputs(const glm::vec3& fillDirWorld, bool enabled);

    /// Sampled view of the RT indoor-fill visibility image (R8_UNORM,
    /// `SHADER_READ_ONLY_OPTIMAL` after each dispatch). Stage 9.8 wires
    /// this into the hybrid composite; before then it's observable but
    /// not consumed. `VK_NULL_HANDLE` when RT mode is off, the device
    /// lacks RT support, or the caller has `enabled = false` and the
    /// pass never built.
    [[nodiscard]] VkImageView GetRtIndoorVisibilityView() const;

    /// Stage 9.8.b.3 — caller-owned inputs for the Hybrid-RT sun composite:
    /// the RP.18 shadow-transmission attachment (view + sampler, owned by
    /// `ShadowMap`) and the scene's `LightingUbo` buffer. The pass samples
    /// these alongside the main-pass depth + normal G-buffers and the RT
    /// shadow-visibility image to re-apply the sun term additively into the
    /// HDR target (see `rt_sun_composite.comp`).
    ///
    /// Wiring is lazy: calling this while the render mode is `Rasterised`
    /// simply caches the handles so a subsequent `SetRenderMode(HybridRt)`
    /// update picks them up. Passing `VK_NULL_HANDLE` for the view or
    /// buffer clears the cache; the per-frame dispatch gate then skips
    /// until the caller re-supplies valid handles.
    void SetRtSunCompositeInputs(VkImageView shadowTransmissionView,
                                 VkSampler shadowTransmissionSampler,
                                 VkBuffer lightingUbo,
                                 VkDeviceSize lightingUboSize);

    /// Stage 9.8.b.3 — true when the Hybrid-RT sun composite pipeline +
    /// descriptors are allocated (i.e. Hybrid RT mode is active on an RT-
    /// capable device and the composite build succeeded). Mirrors
    /// `IsRtAoSourcedInTonemap` for the sun routing. Observable from tests
    /// that flip render mode to verify resources come and go in lockstep.
    [[nodiscard]] bool IsRtSunCompositeBuilt() const {
        return m_rtSunCompositePipeline != nullptr;
    }

    /// Stage 9.8.c.3 — wire the `LightingUBO` for the Hybrid-RT indoor-fill
    /// composite. The indoor composite samples the same buffer the raster
    /// path uses (`SunLightingScene`-packed); the RT indoor-fill visibility
    /// view is sourced internally from `m_rtIndoorPass` and needs no caller
    /// plumbing. Wiring is lazy: calling this while the render mode is
    /// `Rasterised` caches the handle so a later `SetRenderMode(HybridRt)`
    /// picks it up. Passing `VK_NULL_HANDLE` / size 0 clears the cache;
    /// the per-frame dispatch gate then skips until a valid buffer is
    /// re-supplied. Separate from `SetRtSunCompositeInputs` so that
    /// enabling one composite doesn't silently couple the other's inputs.
    void SetRtIndoorCompositeInputs(VkBuffer lightingUbo,
                                    VkDeviceSize lightingUboSize);

    /// Stage 9.8.c.3 — true when the Hybrid-RT indoor-fill composite
    /// pipeline + descriptors are allocated. Mirrors
    /// `IsRtSunCompositeBuilt`. Observable from tests that flip render
    /// mode to verify resources come and go in lockstep.
    [[nodiscard]] bool IsRtIndoorCompositeBuilt() const {
        return m_rtIndoorCompositePipeline != nullptr;
    }

private:
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CreateRenderPass();
    void CreatePresentRenderPass();
    void CreateHdrResources();
    void CreateDepthResources();
    void CreateFramebuffers();
    void CreateTonemapPipeline();
    void CreateTonemapDescriptors();
    void UpdateTonemapDescriptors();
    void CreateDepthPyramidDescriptors();
    void CreateDepthPyramidPipelines();
    void CreateDepthPyramidResources();
    void UpdateDepthPyramidDescriptors();
    void CleanupDepthPyramidResources();
    void CleanupDepthPyramidDescriptors();
    void BuildDepthPyramid(VkCommandBuffer cmd);
    void CreateSsaoDescriptors();
    void CreateSsaoPipelines();
    void CreateSsaoResources();
    void UpdateSsaoDescriptors();
    void ClearAoImagesToWhite();
    void UploadSsaoUbo(uint32_t imageIndex);
    void RunSsao(VkCommandBuffer cmd);
    void CleanupSsaoResources();
    void CleanupSsaoDescriptors();
    void CreatePostRenderPass();
    void CreateLdrResources();
    void CreateSmaaRenderPass();
    void CreateSmaaLuts();
    void CreateSmaaDescriptors();
    void CreateSmaaPipelines();
    void CreateSmaaResources();
    void UpdateSmaaDescriptors();
    void ClearSmaaChainToZero();
    void RunSmaaEdgeAndWeights(VkCommandBuffer cmd);
    void CleanupSmaaResources();
    void CleanupSmaaLuts();
    void CleanupSmaaDescriptors();
    void CleanupFrameResources();
    void CleanupTonemapDescriptors();
    void Cleanup();

    static VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice);

    const Device& m_device;
    Swapchain& m_swapchain;
    std::string m_shaderDir;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_commandBuffers{};

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
    // One render-finished semaphore per swapchain image (presentation engine may still hold the
    // previous one until that image is re-acquired — reusing a per-frame semaphore triggers
    // VUID-vkQueueSubmit-pSignalSemaphores-00067).
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    // "Post" pass runs the tonemap fullscreen tri into a per-swap-image LDR
    // intermediate target (RP.8c / RP.11c). The intermediate is then sampled
    // by the SMAA edge pass (writing the edges target) and by the SMAA blend
    // pass in `m_presentRenderPass` which writes the swapchain.
    // `m_postRenderPass` is render-pass-compatible with `m_presentRenderPass`
    // (same swapchain format, same sample count, same attachment layout).
    VkRenderPass m_postRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_presentRenderPass = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    // Per-swapchain-image HDR colour target (R16G16B16A16_SFLOAT). Written by the main pass,
    // sampled by the tonemap pass.
    std::vector<VkImage> m_hdrImages;
    std::vector<VkImageView> m_hdrImageViews;
    std::vector<VmaAllocation> m_hdrAllocations;
    // Per-swapchain-image normal G-buffer target (R16G16_SNORM, oct-packed
    // view-space normal). Written by `basic.frag` at layout(location=1);
    // sampled by SSAO.
    std::vector<VkImage> m_normalImages;
    std::vector<VkImageView> m_normalImageViews;
    std::vector<VmaAllocation> m_normalAllocations;
    // Per-swapchain-image transparency stencil G-buffer target (R8_UINT).
    // Written by `basic.frag` at layout(location=2) — bit 2 marks
    // transparent surfaces (RP.12b). Sampled by `ssao_xegtao.comp`.
    std::vector<VkImage> m_stencilImages;
    std::vector<VkImageView> m_stencilImageViews;
    std::vector<VmaAllocation> m_stencilAllocations;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkFramebuffer> m_postFramebuffers;
    std::vector<VkFramebuffer> m_presentFramebuffers;
    // Per-swap-image LDR intermediate target (RP.8c / RP.11c). Format
    // matches the swapchain so the SMAA blend pass can sample it and write
    // the final swapchain image through a compatible pipeline layout.
    // Usage = COLOR_ATTACHMENT | SAMPLED; final layout after the post pass
    // is SHADER_READ_ONLY_OPTIMAL so both the SMAA edge pass and the blend
    // pass sample it without a manual barrier.
    std::vector<VkImage> m_ldrImages;
    std::vector<VkImageView> m_ldrImageViews;
    std::vector<VmaAllocation> m_ldrAllocations;

    // Tonemap pass: shaders loaded once in the ctor, pipeline rebuilt on size changes.
    // Descriptor set layout + pool built once; descriptor sets recreated on swapchain resize
    // so they point at the fresh per-image HDR views.
    std::unique_ptr<Shader> m_tonemapVert;
    std::unique_ptr<Shader> m_tonemapFrag;
    std::unique_ptr<TonemapPipeline> m_tonemapPipeline;
    VkSampler m_hdrSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_tonemapSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_tonemapDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_tonemapDescriptorSets;

    // Depth pyramid (RP.4c/d). Per swap image: one 4-mip R32_SFLOAT image,
    // one full-chain sampled view (for downstream SSAO), and four
    // single-mip views (used as both sampled source and storage target by
    // the compute dispatches). Descriptor sets are laid out per-image as
    // [0] = linearize (depth → mip 0), [N≥1] = mip downsample (mip N-1 →
    // mip N). Sampler/layout/pool live for the RenderLoop's lifetime; the
    // images and views are torn down + rebuilt alongside the HDR/normal
    // targets on resize.
    std::vector<VkImage> m_depthPyramidImages;
    std::vector<VmaAllocation> m_depthPyramidAllocations;
    std::vector<VkImageView> m_depthPyramidSampledViews;
    std::vector<std::array<VkImageView, DEPTH_PYRAMID_MIPS>> m_depthPyramidMipViews;
    VkSampler m_depthPyramidSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_depthPyramidSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_depthPyramidDescriptorPool = VK_NULL_HANDLE;
    std::vector<std::array<VkDescriptorSet, DEPTH_PYRAMID_MIPS>> m_depthPyramidSets;
    std::unique_ptr<Shader> m_depthLinearizeShader;
    std::unique_ptr<Shader> m_depthMipShader;
    std::unique_ptr<DepthLinearizePipeline> m_depthLinearizePipeline;
    std::unique_ptr<DepthMipPipeline> m_depthMipPipeline;
    VkExtent2D m_depthPyramidExtent = {0, 0};
    float m_nearZ = 0.1F;
    float m_farZ = 10000.0F;
    glm::mat4 m_proj{1.0F};

    // SSAO (RP.5d; XeGTAO pass from RP.12e). Per swap image: two half-res
    // R8_UNORM AO images ping-ponged by the blur (A = main output +
    // vertical-blur output, B = horizontal-blur intermediate); one
    // XeGtaoUbo with only proj/invProj (RP.12e dropped the 64-sample
    // hemisphere kernel — horizon-based integration walks screen-space
    // slices instead). The main descriptor set binds AO A as its
    // storage-image output; the H-blur set reads A and writes B; the
    // V-blur set reads B and writes A. A is sampled by the tonemap
    // fragment shader at the end of each frame, so its final layout after
    // `RunSsao` is SHADER_READ_ONLY_OPTIMAL; B stays in GENERAL between
    // frames. AO A is cleared to 1.0 at creation so the tonemap multiply
    // starts as a no-op before the first SSAO dispatch lands.
    std::vector<VkImage> m_aoImagesA;
    std::vector<VkImage> m_aoImagesB;
    std::vector<VmaAllocation> m_aoAllocationsA;
    std::vector<VmaAllocation> m_aoAllocationsB;
    std::vector<VkImageView> m_aoViewsA;
    std::vector<VkImageView> m_aoViewsB;
    VkExtent2D m_aoExtent = {0, 0};
    VkSampler m_aoSampler = VK_NULL_HANDLE;
    // RP.12d — NEAREST sampler for the stencil-G-buffer binding (binding 4
    // on the SSAO main set). Integer formats (R8_UINT) don't support linear
    // filtering, and the SSAO per-tap gate reads the raw texel value.
    VkSampler m_ssaoStencilSampler = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<Buffer>> m_ssaoUbos;
    VkDescriptorSetLayout m_ssaoMainSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ssaoBlurSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_ssaoDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_ssaoMainSets;
    std::vector<VkDescriptorSet> m_ssaoBlurSetsH;  // A → B
    std::vector<VkDescriptorSet> m_ssaoBlurSetsV;  // B → A
    std::unique_ptr<Shader> m_ssaoMainShader;
    std::unique_ptr<Shader> m_ssaoBlurShader;
    std::unique_ptr<SsaoXeGtaoPipeline> m_ssaoPipeline;
    std::unique_ptr<SsaoBlurPipeline> m_ssaoBlurPipeline;

    // SMAA 1x post-process (RP.11c). 3-pass chain replacing RP.8c FXAA:
    //   1. edge pass — samples the LDR intermediate, writes an RGBA8 edges
    //      target (per-swap); driven by `m_smaaEdgePipeline`.
    //   2. weights pass — samples edges + the vendored AreaTex/SearchTex
    //      LUTs, writes an RGBA8 weights target; `m_smaaWeightsPipeline`.
    //   3. blend pass — samples the LDR intermediate + weights, writes the
    //      swapchain inside `m_presentRenderPass`; `m_smaaBlendPipeline`.
    // The blend draw is always bound each frame (it's the only path from
    // LDR to the swapchain). When `m_smaaEnabled` is false, the edge +
    // weights passes are skipped entirely and the blend shader's
    // push-constant gate (`enabled` at offset 8) short-circuits before
    // the weights sample.
    //
    // The AreaTex (160×560 RG8) + SearchTex (64×16 R8) LUTs live for the
    // RenderLoop's lifetime — uploaded once in `CreateSmaaLuts` from the
    // vendored byte arrays in `renderer::SmaaAreaTex::Data()` +
    // `renderer::SmaaSearchTex::Data()` (RP.11b.1). The per-swap edges +
    // weights images / framebuffers / descriptor sets rebuild on
    // swapchain resize alongside the HDR / LDR chains.
    std::unique_ptr<Shader> m_smaaVertShader;
    std::unique_ptr<Shader> m_smaaEdgeFragShader;
    std::unique_ptr<Shader> m_smaaWeightsFragShader;
    std::unique_ptr<Shader> m_smaaBlendFragShader;
    std::unique_ptr<SmaaEdgePipeline> m_smaaEdgePipeline;
    std::unique_ptr<SmaaWeightsPipeline> m_smaaWeightsPipeline;
    std::unique_ptr<SmaaBlendPipeline> m_smaaBlendPipeline;

    // Shared RGBA8 render pass for the edge + weights draws. Attachments
    // use LOAD_OP_CLEAR + STORE_OP_STORE; initial layout UNDEFINED, final
    // layout SHADER_READ_ONLY_OPTIMAL so the downstream pass samples the
    // result without a manual barrier. Framebuffers switch targets per
    // pass.
    VkRenderPass m_smaaRenderPass = VK_NULL_HANDLE;
    VkSampler m_smaaSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_smaaEdgeSetLayout = VK_NULL_HANDLE;    // 1× CIS
    VkDescriptorSetLayout m_smaaWeightsSetLayout = VK_NULL_HANDLE; // 3× CIS
    VkDescriptorSetLayout m_smaaBlendSetLayout = VK_NULL_HANDLE;   // 2× CIS
    VkDescriptorPool m_smaaDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_smaaEdgeDescriptorSets;
    std::vector<VkDescriptorSet> m_smaaWeightsDescriptorSets;
    std::vector<VkDescriptorSet> m_smaaBlendDescriptorSets;

    // Per-swap 2-channel edges target (RGBA8 image; shader only writes
    // R/G via `vec2 outEdges`, but the shared render pass's single
    // attachment format keeps both passes pipeline-compatible).
    std::vector<VkImage> m_smaaEdgesImages;
    std::vector<VkImageView> m_smaaEdgesImageViews;
    std::vector<VmaAllocation> m_smaaEdgesAllocations;
    std::vector<VkFramebuffer> m_smaaEdgesFramebuffers;

    // Per-swap 4-channel weights target (RGBA8). Sampled by both the
    // weights pass (read-back isn't needed here; only blend reads it) and
    // the blend pass (reads all four channels per SMAA's packed layout).
    std::vector<VkImage> m_smaaWeightsImages;
    std::vector<VkImageView> m_smaaWeightsImageViews;
    std::vector<VmaAllocation> m_smaaWeightsAllocations;
    std::vector<VkFramebuffer> m_smaaWeightsFramebuffers;

    // SMAA LUTs — uploaded once, sampled by `smaa_weights.frag` bindings
    // 1 + 2. Both are R8-per-channel, linear-filtered, CLAMP_TO_EDGE via
    // the shared `m_smaaSampler`.
    VkImage m_smaaAreaImage = VK_NULL_HANDLE;
    VkImageView m_smaaAreaImageView = VK_NULL_HANDLE;
    VmaAllocation m_smaaAreaAllocation = VK_NULL_HANDLE;
    VkImage m_smaaSearchImage = VK_NULL_HANDLE;
    VkImageView m_smaaSearchImageView = VK_NULL_HANDLE;
    VmaAllocation m_smaaSearchAllocation = VK_NULL_HANDLE;

    bool m_smaaEnabled = true;
    // RP.19 — knobs pushed into `smaa_edge.frag` + `smaa_weights.frag` each
    // frame. Defaults mirror the pre-RP.19 hardcoded values so a panel that
    // never touches the sliders produces bit-compatible output.
    float m_smaaThreshold = 0.1F;
    int m_smaaMaxSearchSteps = 16;
    int m_smaaMaxSearchStepsDiag = 8;
    // RP.20 — XeGTAO push-constant knobs; defaults mirror the pre-RP.20
    // literals in `RunXeGtao`.
    float m_ssaoRadius = 0.35F;
    float m_ssaoFalloff = 0.6F;
    float m_ssaoIntensity = 0.5F;
    float m_ssaoShadowPower = 1.5F;

    // RP.13b — tonemap push-constant state fed into `tonemap.frag` each
    // frame. Holds `exposure` + (Stage 9.8.a) `useRtAo`; `SetExposure`
    // writes the former and `SetRenderMode` writes the latter. Struct is
    // uploaded once per tonemap draw via vkCmdPushConstants.
    TonemapPipeline::PushConstants m_exposurePush{};

    // Stage 9.8.a — mirror of `m_exposurePush.useRtAo` as a bool, so the
    // `IsRtAoSourcedInTonemap()` accessor can return a clean bool without
    // float-compare noise. Single source of truth: `RefreshTonemapRtRouting`
    // writes both this and the push-constant field together.
    bool m_tonemapUseRtAo = false;

    uint32_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    VkClearColorValue m_clearColor = {{0.1f, 0.1f, 0.1f, 1.0f}};
    bool m_frameStarted = false;

    PreMainPassCallback m_preMainPass;
    InPresentPassCallback m_inPresentPass;

    // Stage 9.4.b — RT sun-shadow wire. The pass is only allocated while
    // `m_renderMode == HybridRt` on an RT-capable device (guarded by
    // `Device::HasRayTracing()`); otherwise it stays null and the per-frame
    // dispatch path short-circuits. The depth sampler is a dedicated
    // NEAREST/clamp sampler used exclusively for depth reads in the
    // raygen — the depth-pyramid sampler uses LINEAR filtering which is
    // the wrong semantics for this reconstruction.
    //
    // The BLAS/TLAS live outside this class (the caller extracts them
    // from the scene); `m_rtTlas` is just the non-owning handle the
    // dispatch binds into descriptor slot 0 each frame. `SetRtShadowInputs`
    // clears the handle (pass `VK_NULL_HANDLE`) to temporarily skip the
    // dispatch without leaving Hybrid RT mode — e.g. between scene loads.
    RenderMode m_renderMode = RenderMode::Rasterised;
    std::unique_ptr<RtShadowPass> m_rtShadowPass;
    VkSampler m_rtDepthSampler = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_rtTlas = VK_NULL_HANDLE;
    glm::vec3 m_rtSunDir{0.0F, -1.0F, 0.0F};
    glm::mat4 m_rtView{1.0F};

    void BuildRtShadowPass();
    void DestroyRtShadowPass();
    void DispatchRtShadow(VkCommandBuffer cmd);

    // Stage 9.8.a — recompute the tonemap's AO routing from the current
    // `m_renderMode` + `m_rtAoPass` state. Sets `m_tonemapUseRtAo`,
    // `m_exposurePush.useRtAo`, and re-runs `UpdateTonemapDescriptors()`
    // so binding 2 points at the RT AO view when routing is active or
    // falls back to the XeGTAO AO view otherwise (raster path remains
    // bit-compatible because the shader's mix degenerates at useRtAo=0).
    void RefreshTonemapRtRouting();

    // Stage 9.5.b — RT AO wire. Lifecycle mirrors `m_rtShadowPass`: only
    // allocated while `m_renderMode == HybridRt` on an RT-capable device,
    // torn down when the user flips back. The AO pass shares the same
    // TLAS + depth sampler + view that drive the shadow pass; no parallel
    // scene state. `m_rtAoFrameIndex` is incremented each dispatch so the
    // raygen's PCG hash produces different samples per frame — the
    // foundation for the 9.5.c temporal accumulator without changing the
    // public API.
    std::unique_ptr<RtAoPass> m_rtAoPass;
    float m_rtAoRadius = 1.0F;
    uint32_t m_rtAoFrameIndex = 0U;

    void BuildRtAoPass();
    void DestroyRtAoPass();
    void DispatchRtAo(VkCommandBuffer cmd);

    // Stage 9.7.b — RT indoor-fill wire. Lifecycle mirrors the shadow /
    // AO passes: only allocated while `m_renderMode == HybridRt` on an
    // RT-capable device AND `m_rtIndoorEnabled` is true. The pass
    // shares `m_rtTlas` + `m_rtDepthSampler` + `m_rtView` with the
    // shadow / AO paths — no parallel scene state.
    std::unique_ptr<RtIndoorPass> m_rtIndoorPass;
    glm::vec3 m_rtIndoorDir{0.0F, -1.0F, 0.0F};
    bool m_rtIndoorEnabled = false;

    void BuildRtIndoorPass();
    void DestroyRtIndoorPass();
    void DispatchRtIndoor(VkCommandBuffer cmd);

    // Stage 9.8.b.3 — RT sun composite wire. The compute pipeline + its
    // descriptor pool / per-swap descriptor sets are allocated on the flip
    // to `HybridRt` (guarded by `Device::HasRayTracing()`) and torn down
    // on the flip back to `Rasterised`. Pipeline layout doesn't depend on
    // swap extent, but the descriptor pool + sets do (HDR / normal views
    // are per-swap-image), so resizing goes through a full rebuild like
    // the other RT passes.
    //
    // The transmission view + sampler + `LightingUbo` buffer are owned by
    // the caller (main.cpp's ShadowMap + LightingUbo Buffer). Caching them
    // here lets the first `SetRenderMode(HybridRt)` pick them up whether
    // they were wired before or after the mode flip.
    std::unique_ptr<Shader> m_rtSunCompositeShader;
    std::unique_ptr<RtSunCompositePipeline> m_rtSunCompositePipeline;
    VkDescriptorSetLayout m_rtSunCompositeSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_rtSunCompositeDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_rtSunCompositeDescriptorSets;
    VkImageView m_rtSunTransmissionView = VK_NULL_HANDLE;
    VkSampler m_rtSunTransmissionSampler = VK_NULL_HANDLE;
    VkBuffer m_rtSunLightingUbo = VK_NULL_HANDLE;
    VkDeviceSize m_rtSunLightingUboSize = 0;

    void BuildRtSunComposite();
    void DestroyRtSunComposite();
    void UpdateRtSunCompositeDescriptors();
    void DispatchRtSunComposite(VkCommandBuffer cmd);

    // Stage 9.8.c.3 — RT indoor-fill composite wire. Shape identical to
    // the sun composite above: lazy build on flip to `HybridRt`, rebuild
    // on swapchain resize, teardown on flip to `Rasterised`. Five
    // descriptor bindings (depth / normal / RT indoor visibility / UBO /
    // HDR storage) vs. the sun composite's six — no shadow-transmission
    // sample since architectural indoor fill doesn't model window tint.
    std::unique_ptr<Shader> m_rtIndoorCompositeShader;
    std::unique_ptr<RtIndoorCompositePipeline> m_rtIndoorCompositePipeline;
    VkDescriptorSetLayout m_rtIndoorCompositeSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_rtIndoorCompositeDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_rtIndoorCompositeDescriptorSets;
    VkBuffer m_rtIndoorCompositeLightingUbo = VK_NULL_HANDLE;
    VkDeviceSize m_rtIndoorCompositeLightingUboSize = 0;

    void BuildRtIndoorComposite();
    void DestroyRtIndoorComposite();
    void UpdateRtIndoorCompositeDescriptors();
    void DispatchRtIndoorComposite(VkCommandBuffer cmd);
};

}  // namespace bimeup::renderer
