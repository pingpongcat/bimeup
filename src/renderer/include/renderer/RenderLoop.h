#pragma once

#include <renderer/OutlinePipeline.h>
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
class SsaoPipeline;
class SsaoBlurPipeline;
class SsilPipeline;
class SsilBlurPipeline;

class RenderLoop {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    // HDR offscreen format used between the scene render pass and the tonemap
    // resolve pass. Matches the Stage RP plan — enough dynamic range to keep
    // bright lighting / future SSIL bounces from clipping before the curve.
    static constexpr VkFormat HDR_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
    // Normal G-buffer format — octahedron-packed view-space normal, two
    // signed 16-bit channels. Attachment 1 of the main render pass. Clear
    // value (0,0) decodes to +Z so overlay pipelines that leave this
    // attachment untouched (disableSecondaryColorWrites) don't corrupt
    // SSAO/SSIL sampling downstream.
    static constexpr VkFormat NORMAL_FORMAT = VK_FORMAT_R16G16_SNORM;
    // Outline stencil G-buffer format (RP.6c). Single-channel unsigned 8-bit
    // integer — 0 = background, 1 = selected, 2 = hovered. Attachment 2 of
    // the main render pass. Sampled by `outline.frag` (RP.6b) as a
    // `usampler2D`. Clear value is 0 so unwritten pixels fall into the
    // background category.
    static constexpr VkFormat STENCIL_FORMAT = VK_FORMAT_R8_UINT;
    // Depth pyramid format — view-space linear depth, single-channel float.
    // Used by the RP.4c/d depth_linearize + depth_mip compute chain and
    // sampled by downstream SSAO/SSIL in RP.5/RP.7.
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
    // SSIL half-res indirect colour format (RP.7d). RGBA16F — the pass carries
    // bounced colour (not just a scalar like SSAO), and the values need enough
    // headroom for a composite-add into the HDR target before ACES.
    static constexpr VkFormat SSIL_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

    RenderLoop(const Device& device, Swapchain& swapchain,
               const std::string& shaderDir,
               VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
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
    /// Render pass for scene geometry — HDR colour (+ depth, + optional MSAA
    /// resolve) targeting the offscreen HDR image. Bind scene pipelines here.
    [[nodiscard]] VkRenderPass GetRenderPass() const;
    [[nodiscard]] VkSampleCountFlagBits GetSampleCount() const;
    /// Render pass for the final HDR→LDR tonemap + UI overlay targeting the
    /// swapchain. Bind ImGui and any other post-tonemap pipelines here;
    /// sample count is always 1× (the swapchain is never multisampled).
    [[nodiscard]] VkRenderPass GetPresentRenderPass() const;
    /// View into the single-sample normal G-buffer for the given swap image
    /// index (same index domain as `GetCurrentImageIndex()`). Will be read by
    /// SSAO / SSIL / outline passes in later RP tasks. Returns VK_NULL_HANDLE
    /// only if the index is out of range.
    [[nodiscard]] VkImageView GetNormalImageView(uint32_t imageIndex) const;
    /// View into the single-sample outline-stencil G-buffer (R8_UINT) for the
    /// given swap image index (same index domain as `GetCurrentImageIndex()`).
    /// Bound by the RP.6b outline pass as a `usampler2D`. Returns VK_NULL_HANDLE
    /// only if the index is out of range.
    [[nodiscard]] VkImageView GetStencilImageView(uint32_t imageIndex) const;
    /// Full-mip-chain sampled view of the depth pyramid for the given swap
    /// image index (R32_SFLOAT, mips 0..DEPTH_PYRAMID_MIPS-1 of view-space
    /// linear depth). Bind with a mipmap-aware sampler and select mips via
    /// `textureLod`; downstream SSAO/SSIL will consume this in later RP
    /// tasks. Returns VK_NULL_HANDLE if the index is out of range.
    [[nodiscard]] VkImageView GetDepthPyramidView(uint32_t imageIndex) const;
    /// Final post-blur AO target (R8_UNORM, half-res) for the given swap
    /// image. Written by the RP.5d SSAO compute chain and sampled by the
    /// tonemap fragment shader. Returns VK_NULL_HANDLE if the index is out
    /// of range.
    [[nodiscard]] VkImageView GetAoImageView(uint32_t imageIndex) const;
    /// Final post-blur SSIL target (RGBA16F, half-res) for the given swap
    /// image. Written by the RP.7d SSIL compute chain and sampled by the
    /// tonemap fragment shader as an additive indirect-colour contribution.
    /// Returns VK_NULL_HANDLE if the index is out of range.
    [[nodiscard]] VkImageView GetSsilImageView(uint32_t imageIndex) const;

    /// Camera projection matrix + near/far plane values. Projection feeds
    /// the SSAO UBO (mat4 proj + mat4 invProj) so view-space sample
    /// positions project back to UV; near/far feed the depth-linearize
    /// compute push constants so pyramid mip 0 matches the CPU mirror
    /// `renderer::LinearizeDepth`. Call each frame after the camera has
    /// been updated. Defaults are (identity, 0.1, 10000).
    void SetProjection(const glm::mat4& proj, float nearZ, float farZ);

    /// Camera view matrix for the current frame. Fed into the SSIL reprojection
    /// (RP.7d): the ssil_main.comp UBO carries
    /// `prevViewProj * currInvViewProj`, so `SetView` must be called before
    /// `EndFrame` any frame SSIL is enabled. Defaults to identity.
    void SetView(const glm::mat4& view);

    /// RP.7d — SSIL (screen-space indirect lighting) parameters.
    /// `radius`  : view-space sample radius (metres)
    /// `intensity` : scale on the accumulated indirect colour
    /// `normalRejection` : exponent on the `dot(nCurr, nSampled)` lobe (higher
    ///                     narrows the acceptance cone; 0 disables rejection)
    /// `enabled` : when false (or when MSAA is on — SSIL inherits the depth-
    ///             pyramid gate) the compute dispatches are skipped and the
    ///             target stays at its cleared 0 so the tonemap add is a no-op.
    void SetSsilParams(float radius, float intensity, float normalRejection,
                       bool enabled);

    /// RP.6d — selection/hover outline pass parameters. The outline draw is
    /// recorded inside the present pass between the tonemap fullscreen tri
    /// and the in-present-pass callback (where ImGui lands), so the outline
    /// composes over the tonemapped image but underneath UI overlays. When
    /// `enabled` is false (or when MSAA is on — the depth-pyramid input is
    /// gated off in that mode) the draw is skipped and the swapchain image
    /// is unaffected. Push constants drive the panel-tweakable knobs
    /// (selected/hover colours, tap thickness in pixels, depth-edge cutoff);
    /// the texelSize field should be (1/width, 1/height) in pixels — caller
    /// updates per-frame so resize lands cleanly.
    void SetOutlineParams(const OutlinePipeline::PushConstants& push, bool enabled);

    /// RP.11c — SMAA 1x post-process enable. The blend draw is the only
    /// path from the LDR intermediate to the swapchain, so it runs every
    /// frame; when `enabled` is false the edge + weights passes are
    /// skipped and the blend shader's push-constant gate short-circuits
    /// to a passthrough via `smaa_blend.frag`'s early-return branch —
    /// 3 draws → 1 draw and stale weights can't influence the output.
    /// LDR intermediate is always 1-sample so SMAA needs no MSAA gate.
    void SetSmaaParams(bool enabled);

    /// RP.9b — linear distance fog parameters pushed into `tonemap.frag` via
    /// the tonemap pipeline's push constants. When `enabled` is false the
    /// push constant's enable flag is zeroed so the shader skips the depth
    /// tap + mix. Under MSAA the depth pyramid isn't built (pyramid
    /// compute is `sampler2D`-only), so the tonemap sampler at binding 3
    /// would read undefined contents — this method forces the enable flag
    /// off in that mode regardless of `enabled`, matching the
    /// SSAO/SSIL/outline gating pattern.
    void SetFogParams(const glm::vec3& color, float start, float end, bool enabled);

    /// Pre-ACES exposure multiplier applied to the composited HDR colour
    /// inside `tonemap.frag`. Default (from the push-constant ctor) is 1.0
    /// — identity. The panel seeds a scene-appropriate value so the three-
    /// point lighting (ambient + key + fill + rim can sum to ~2.x on a
    /// bright surface) doesn't saturate the ACES curve into near-white.
    /// Does not interact with MSAA — applied unconditionally each frame.
    void SetExposure(float exposure);

    [[nodiscard]] VkSampleCountFlagBits GetPresentSampleCount() const {
        return VK_SAMPLE_COUNT_1_BIT;
    }

    /// Change the MSAA sample count. Waits for GPU idle, tears down the main
    /// render pass / HDR & depth attachments / framebuffers, and rebuilds them.
    /// Both render pass handles are recreated — any pipeline bound to the
    /// previous passes must be rebuilt by the caller before the next BeginFrame().
    void SetSampleCount(VkSampleCountFlagBits samples);

    /// Rebuild HDR/depth/MSAA images, framebuffers, and tonemap descriptor
    /// bindings to match the current swapchain extent. Call after
    /// `Swapchain::Recreate(...)` on a resize. The render pass handles are
    /// preserved (format/samples unchanged) so pipelines stay valid.
    void RecreateForSwapchain();

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
    void CreateSsilDescriptors();
    void CreateSsilPipelines();
    void CreateSsilResources();
    void UpdateSsilDescriptors();
    void ClearSsilTargetsToZero();
    void UploadSsilUbo(uint32_t imageIndex);
    void RunSsil(VkCommandBuffer cmd);
    void CopyHdrToPrevHdr(VkCommandBuffer cmd);
    void CleanupSsilResources();
    void CleanupSsilDescriptors();
    void CreateOutlineDescriptors();
    void CreateOutlinePipeline();
    void UpdateOutlineDescriptors();
    void CleanupOutlineDescriptors();
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
    // "Post" pass runs tonemap + outline into a per-swap-image LDR intermediate
    // target (RP.8c / RP.11c). The intermediate is then sampled by the SMAA
    // edge pass (writing the edges target) and by the SMAA blend pass in
    // `m_presentRenderPass` which writes the swapchain. `m_postRenderPass`
    // is render-pass-compatible with `m_presentRenderPass` (same swapchain
    // format, same sample count, same attachment layout) so tonemap +
    // outline pipelines built against the present pass bind to the post
    // pass without rebuild.
    VkRenderPass m_postRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_presentRenderPass = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
    VkImage m_depthImage = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    // Multisampled HDR colour attachment (only used when m_samples > 1x; resolved into the
    // single-sample HDR image each frame).
    VkImage m_colorImage = VK_NULL_HANDLE;
    VkImageView m_colorImageView = VK_NULL_HANDLE;
    VmaAllocation m_colorAllocation = VK_NULL_HANDLE;
    // Per-swapchain-image HDR colour target (R16G16B16A16_SFLOAT). Written by the main pass,
    // sampled by the tonemap pass.
    std::vector<VkImage> m_hdrImages;
    std::vector<VkImageView> m_hdrImageViews;
    std::vector<VmaAllocation> m_hdrAllocations;
    // Per-swapchain-image normal G-buffer target (R16G16_SNORM, oct-packed
    // view-space normal). Written by `basic.frag` at layout(location=1); will
    // be sampled by SSAO/SSIL/outline passes later in Stage RP.
    std::vector<VkImage> m_normalImages;
    std::vector<VkImageView> m_normalImageViews;
    std::vector<VmaAllocation> m_normalAllocations;
    // Multisampled normal attachment (transient, only when m_samples > 1x).
    VkImage m_normalMsaaImage = VK_NULL_HANDLE;
    VkImageView m_normalMsaaImageView = VK_NULL_HANDLE;
    VmaAllocation m_normalMsaaAllocation = VK_NULL_HANDLE;
    // Per-swapchain-image outline-stencil G-buffer target (R8_UINT).
    // Written by `basic.frag` at layout(location=2) and sampled by the RP.6b
    // outline pass. Same shape as the normal G-buffer: single-sample target,
    // transient MSAA sibling when m_samples > 1x.
    std::vector<VkImage> m_stencilImages;
    std::vector<VkImageView> m_stencilImageViews;
    std::vector<VmaAllocation> m_stencilAllocations;
    VkImage m_stencilMsaaImage = VK_NULL_HANDLE;
    VkImageView m_stencilMsaaImageView = VK_NULL_HANDLE;
    VmaAllocation m_stencilMsaaAllocation = VK_NULL_HANDLE;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
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

    // Tonemap pass: shaders loaded once in the ctor, pipeline rebuilt on MSAA/size changes.
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
    // one full-chain sampled view (for downstream SSAO/SSIL), and four
    // single-mip views (used as both sampled source and storage target by
    // the compute dispatches). Descriptor sets are laid out per-image as
    // [0] = linearize (depth → mip 0), [N≥1] = mip downsample (mip N-1 →
    // mip N). Sampler/layout/pool live for the RenderLoop's lifetime; the
    // images and views are torn down + rebuilt alongside the HDR/normal
    // targets on resize or MSAA change.
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

    // SSAO (RP.5d). Per swap image: two half-res R8_UNORM AO images
    // ping-ponged by the blur (A = main output + vertical-blur output,
    // B = horizontal-blur intermediate); one SsaoUbo with proj/invProj and
    // the hemisphere kernel. The main descriptor set binds AO A as its
    // storage-image output; the H-blur set reads A and writes B; the
    // V-blur set reads B and writes A. A is sampled by the tonemap
    // fragment shader at the end of each frame, so its final layout after
    // `RunSsao` is SHADER_READ_ONLY_OPTIMAL; B stays in GENERAL between
    // frames. Under MSAA the pyramid gate above applies — SSAO never
    // dispatches; AO A stays at the init-time (1.0) clear so the tonemap
    // multiply is a no-op.
    std::vector<VkImage> m_aoImagesA;
    std::vector<VkImage> m_aoImagesB;
    std::vector<VmaAllocation> m_aoAllocationsA;
    std::vector<VmaAllocation> m_aoAllocationsB;
    std::vector<VkImageView> m_aoViewsA;
    std::vector<VkImageView> m_aoViewsB;
    VkExtent2D m_aoExtent = {0, 0};
    VkSampler m_aoSampler = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<Buffer>> m_ssaoUbos;
    VkDescriptorSetLayout m_ssaoMainSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ssaoBlurSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_ssaoDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_ssaoMainSets;
    std::vector<VkDescriptorSet> m_ssaoBlurSetsH;  // A → B
    std::vector<VkDescriptorSet> m_ssaoBlurSetsV;  // B → A
    std::unique_ptr<Shader> m_ssaoMainShader;
    std::unique_ptr<Shader> m_ssaoBlurShader;
    std::unique_ptr<SsaoPipeline> m_ssaoPipeline;
    std::unique_ptr<SsaoBlurPipeline> m_ssaoBlurPipeline;

    // SSIL (RP.7d). Mirrors the SSAO resource layout with two key additions:
    // (1) a previous-frame HDR ring sampled by `ssil_main.comp` through the
    //     binding-2 UBO — one image per swap slot; HDR is copied into it at the
    //     end of each frame so the next time that swap slot renders, the
    //     previous contents are available. On resize / MSAA-change we rebuild
    //     + clear to 0 so the first frame's SSIL contribution is "no bounce".
    // (2) a cached `prevViewProj` per swap image; combined with the current
    //     frame's inverse viewProj it produces the `reprojection` matrix that
    //     `ssil_main.comp` uses to find a tap's UV in the previous frame.
    // Target A/B ping-pong layout matches SSAO exactly: main writes A, H-blur
    // reads A writes B, V-blur reads B writes A; A is sampled by the tonemap
    // pass at the end of the frame.
    glm::mat4 m_view{1.0F};
    std::vector<glm::mat4> m_prevViewProj;
    std::vector<VkImage> m_ssilImagesA;
    std::vector<VkImage> m_ssilImagesB;
    std::vector<VmaAllocation> m_ssilAllocationsA;
    std::vector<VmaAllocation> m_ssilAllocationsB;
    std::vector<VkImageView> m_ssilViewsA;
    std::vector<VkImageView> m_ssilViewsB;
    std::vector<VkImage> m_prevHdrImages;
    std::vector<VmaAllocation> m_prevHdrAllocations;
    std::vector<VkImageView> m_prevHdrImageViews;
    VkExtent2D m_ssilExtent = {0, 0};
    VkSampler m_ssilSampler = VK_NULL_HANDLE;
    // RP.12c.2 — NEAREST sampler for the stencil-G-buffer binding (binding 5
    // on the SSIL main set). Integer formats (R8_UINT) don't support linear
    // filtering, and the SSIL per-tap gate just wants the raw texel value.
    VkSampler m_ssilStencilSampler = VK_NULL_HANDLE;
    std::vector<std::unique_ptr<Buffer>> m_ssilUbos;
    VkDescriptorSetLayout m_ssilMainSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ssilBlurSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_ssilDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_ssilMainSets;
    std::vector<VkDescriptorSet> m_ssilBlurSetsH;
    std::vector<VkDescriptorSet> m_ssilBlurSetsV;
    std::unique_ptr<Shader> m_ssilMainShader;
    std::unique_ptr<Shader> m_ssilBlurShader;
    std::unique_ptr<SsilPipeline> m_ssilPipeline;
    std::unique_ptr<SsilBlurPipeline> m_ssilBlurPipeline;
    float m_ssilRadius = 0.5F;
    float m_ssilIntensity = 1.0F;
    float m_ssilNormalRejection = 2.0F;
    // RP.12c: placeholder until c.3 wires the panel "Max luminance" slider.
    // Pushed every frame so the shader's clamp is always armed.
    float m_ssilMaxLuminance = 0.5F;
    bool m_ssilEnabled = false;
    uint32_t m_ssilFrameCounter = 0;

    // Outline pass (RP.6d). Per swap image: one combined descriptor set
    // sampling (binding 0 = stencil id R8_UINT usampler2D, binding 1 =
    // depth pyramid mip 0 sampler2D). One sampler shared across both
    // bindings (linear, CLAMP_TO_EDGE, maxLod = 0.25 — clamped to mip 0
    // for the depth tap). Pipeline targets the present pass; rebuilt on
    // sample-count change alongside the tonemap pipeline. Push-constant
    // values + the enable toggle come from the panel via SetOutlineParams.
    // The pass is skipped under MSAA — the depth pyramid is gated off in
    // that mode and the outline shader's depth-discontinuity fallback
    // would otherwise sample an undefined image.
    std::unique_ptr<Shader> m_outlineVertShader;
    std::unique_ptr<Shader> m_outlineFragShader;
    std::unique_ptr<OutlinePipeline> m_outlinePipeline;
    VkSampler m_outlineSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_outlineSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_outlineDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_outlineDescriptorSets;
    OutlinePipeline::PushConstants m_outlinePush{};
    bool m_outlineEnabled = false;

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
    // the weights sample. LDR is always 1-sample, so SMAA runs under MSAA
    // without a gate (unlike fog / outline).
    //
    // The AreaTex (160×560 RG8) + SearchTex (64×16 R8) LUTs live for the
    // RenderLoop's lifetime — uploaded once in `CreateSmaaLuts` from the
    // vendored byte arrays in `renderer::SmaaAreaTex::Data()` +
    // `renderer::SmaaSearchTex::Data()` (RP.11b.1). The per-swap edges +
    // weights images / framebuffers / descriptor sets rebuild on MSAA
    // change or swapchain resize alongside the HDR / LDR chains.
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

    // RP.9b — shared tonemap push-constant state fed into `tonemap.frag`
    // each frame. `SetFogParams` updates the fog fields; the struct is
    // uploaded once per tonemap draw via vkCmdPushConstants. The fog
    // enable flag is carried in `fogColorEnabled.w` so the shader
    // short-circuits the depth tap + mix when disabled (or MSAA-gated —
    // `SetFogParams` force-clears w when m_samples > 1x).
    TonemapPipeline::PushConstants m_tonemapPush{};

    uint32_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    VkClearColorValue m_clearColor = {{0.1f, 0.1f, 0.1f, 1.0f}};
    bool m_frameStarted = false;

    PreMainPassCallback m_preMainPass;
    InPresentPassCallback m_inPresentPass;
};

}  // namespace bimeup::renderer
