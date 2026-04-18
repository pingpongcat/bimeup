#include <vulkan/vulkan.h>

#include <core/EventBus.h>
#include <core/Events.h>
#include <core/Picking.h>
#include <core/SceneUploader.h>
#include <core/Selection.h>
#include <ifc/AsyncLoader.h>
#include <ifc/IfcHierarchy.h>
#include <ifc/IfcModel.h>
#include <platform/Input.h>
#include <platform/Window.h>
#include <renderer/Buffer.h>
#include <renderer/Camera.h>
#include <renderer/ClipPlaneManager.h>
#include <renderer/MeshBuffer.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/DiskMarker.h>
#include <renderer/DiskMarkerPipeline.h>
#include <renderer/FirstPersonController.h>
#include <renderer/Lighting.h>
#include <renderer/Msaa.h>
#include <renderer/Pipeline.h>
#include <renderer/RenderLoop.h>
#include <renderer/RenderMode.h>
#include <renderer/SectionFillPipeline.h>
#include <renderer/Shader.h>
#include <renderer/ShadowPass.h>
#include <renderer/Swapchain.h>
#include <renderer/ViewportNavigator.h>
#include <renderer/VulkanContext.h>
#include <scene/AABB.h>
#include <scene/AxisSectionController.h>
#include <scene/Measurement.h>
#include <scene/Raycast.h>
#include <scene/Scene.h>
#include <scene/SceneBuilder.h>
#include <scene/SceneNode.h>
#include <scene/SectionCapGeometry.h>
#include <tools/Log.h>
#include <ui/AxisSectionPanel.h>
#include <ui/FirstPersonExitPanel.h>
#include <ui/PlanViewPanel.h>
#include <ui/HierarchyPanel.h>
#include <ui/MeasurementsPanel.h>
#include <ui/PropertyPanel.h>
#include <ui/RenderQualityPanel.h>
#include <ui/TypeVisibilityPanel.h>
#include <ui/SelectionBridge.h>
#include <ui/Theme.h>
#include <ui/Toolbar.h>
#include <ui/UIManager.h>
#include <ui/ViewportOverlay.h>

#include <imgui.h>
#include <imoguizmo.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <future>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>

namespace {

using bimeup::renderer::Vertex;

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
};

/// Compute scene bounding box from all nodes that have geometry.
bimeup::scene::AABB ComputeSceneBounds(const bimeup::scene::Scene& scene) {
    bimeup::scene::AABB bounds;
    for (size_t i = 0; i < scene.GetNodeCount(); ++i) {
        const auto& node = scene.GetNode(static_cast<bimeup::scene::NodeId>(i));
        if (node.bounds.IsValid()) {
            bounds = bimeup::scene::AABB::Merge(bounds, node.bounds);
        }
    }
    return bounds;
}

/// Fit the camera so the given AABB is fully visible, using a simple radius-based heuristic.
void FitCameraToBounds(bimeup::renderer::Camera& camera, const bimeup::scene::AABB& bounds) {
    if (!bounds.IsValid()) {
        return;
    }
    camera.Frame(bounds.GetMin(), bounds.GetMax());
}

/// Collect unique mesh handles from visible scene nodes (avoids drawing shared batched meshes multiple times).
std::vector<std::pair<bimeup::renderer::MeshHandle, glm::mat4>>
CollectDrawCalls(const bimeup::scene::Scene& scene) {
    std::vector<std::pair<bimeup::renderer::MeshHandle, glm::mat4>> draws;
    // Track which handles we've already added (batched meshes may be shared by multiple nodes)
    std::vector<bimeup::renderer::MeshHandle> seen;

    for (size_t i = 0; i < scene.GetNodeCount(); ++i) {
        const auto& node = scene.GetNode(static_cast<bimeup::scene::NodeId>(i));
        if (!node.visible || !node.mesh.has_value()) {
            continue;
        }
        auto handle = node.mesh.value();
        if (std::find(seen.begin(), seen.end(), handle) != seen.end()) {
            continue;
        }
        seen.push_back(handle);
        draws.emplace_back(handle, node.transform);
    }
    return draws;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::printf("bimeup v%s\n", BIMEUP_VERSION);

    bimeup::tools::Log::Init("bimeup");
    LOG_INFO("Bimeup v{} starting", BIMEUP_VERSION);

    // Platform. GlfwGuard is declared first so it destructs LAST — after every
    // Vulkan object below. Without this, glfwTerminate() (which on Wayland calls
    // wl_display_disconnect) would run while the swapchain is still alive, and
    // vkDestroySwapchainKHR would dereference the freed Wayland display
    // (heap-use-after-free caught by ASan).
    struct GlfwGuard {
        GlfwGuard() { bimeup::platform::Window::InitGlfw(); }
        ~GlfwGuard() { bimeup::platform::Window::TerminateGlfw(); }
        GlfwGuard(const GlfwGuard&) = delete;
        GlfwGuard& operator=(const GlfwGuard&) = delete;
    };
    GlfwGuard glfwGuard;
    bimeup::platform::Window window(
        {.width = 1280, .height = 720, .title = "Bimeup", .maximized = true});
    bimeup::platform::Input input(window);

    // Vulkan context
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
#ifdef NDEBUG
    constexpr bool kEnableValidation = false;
#else
    constexpr bool kEnableValidation = true;
#endif
    bimeup::renderer::VulkanContext vulkanContext(kEnableValidation, requiredExts);

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(vulkanContext.GetInstance(), window.GetHandle(), nullptr,
                                &surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create window surface");
        return 1;
    }

    // RAII guard ensures the surface is destroyed AFTER swapchain/device (declared below) so
    // VUID-vkDestroySurfaceKHR-surface-01266 is not violated at shutdown.
    struct SurfaceGuard {
        VkInstance instance;
        VkSurfaceKHR surface;
        ~SurfaceGuard() {
            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance, surface, nullptr);
            }
        }
    };
    SurfaceGuard surfaceGuard{vulkanContext.GetInstance(), surface};

    bimeup::renderer::Device device(vulkanContext.GetInstance(), surface);
    auto fbSize = window.GetFramebufferSize();
    bimeup::renderer::Swapchain swapchain(
        device, surface, VkExtent2D{static_cast<uint32_t>(fbSize.x), static_cast<uint32_t>(fbSize.y)});
    VkSampleCountFlags supportedSamples =
        bimeup::renderer::GetUsableSampleCounts(device.GetPhysicalDevice());
    VkSampleCountFlagBits currentSamples = VK_SAMPLE_COUNT_1_BIT;
    bimeup::renderer::RenderLoop renderLoop(device, swapchain, currentSamples);

    // Shaders
    std::string shaderDir = BIMEUP_SHADER_DIR;
    bimeup::renderer::Shader vertShader(device, bimeup::renderer::ShaderStage::Vertex,
                                        shaderDir + "/basic.vert.spv");
    bimeup::renderer::Shader fragShader(device, bimeup::renderer::ShaderStage::Fragment,
                                        shaderDir + "/basic.frag.spv");
    bimeup::renderer::Shader shadowVertShader(device, bimeup::renderer::ShaderStage::Vertex,
                                              shaderDir + "/shadow.vert.spv");
    bimeup::renderer::Shader shadowFragShader(device, bimeup::renderer::ShaderStage::Fragment,
                                              shaderDir + "/shadow.frag.spv");
    bimeup::renderer::Shader sectionFillVertShader(device, bimeup::renderer::ShaderStage::Vertex,
                                                   shaderDir + "/section_fill.vert.spv");
    bimeup::renderer::Shader sectionFillFragShader(device, bimeup::renderer::ShaderStage::Fragment,
                                                   shaderDir + "/section_fill.frag.spv");
    bimeup::renderer::Shader diskMarkerVertShader(device, bimeup::renderer::ShaderStage::Vertex,
                                                  shaderDir + "/disk_marker.vert.spv");
    bimeup::renderer::Shader diskMarkerFragShader(device, bimeup::renderer::ShaderStage::Fragment,
                                                  shaderDir + "/disk_marker.frag.spv");

    // Descriptor set: camera UBO (vertex) + lighting UBO (fragment) + shadow map sampler (fragment)
    //                 + clip planes UBO (fragment)
    bimeup::renderer::DescriptorSetLayout dsLayout(device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT},
    });
    bimeup::renderer::DescriptorPool dsPool(device, 1, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    });
    bimeup::renderer::DescriptorSet descriptorSet(device, dsPool, dsLayout);

    CameraUBO ubo{};
    bimeup::renderer::Buffer uboBuffer(device, bimeup::renderer::BufferType::Uniform,
                                       sizeof(CameraUBO), &ubo);
    descriptorSet.UpdateBuffer(0, uboBuffer);

    bimeup::renderer::LightingUbo lightingUbo = bimeup::renderer::PackLighting(
        bimeup::renderer::MakeDefaultLighting());
    bimeup::renderer::Buffer lightingBuffer(device, bimeup::renderer::BufferType::Uniform,
                                            sizeof(bimeup::renderer::LightingUbo), &lightingUbo);
    descriptorSet.UpdateBuffer(1, lightingBuffer);

    bimeup::renderer::ClipPlaneManager clipPlaneManager;
    bimeup::scene::AxisSectionController axisSectionController;
    bimeup::renderer::ClipPlanesUbo clipPlanesUbo = bimeup::renderer::PackClipPlanes(clipPlaneManager);
    bimeup::renderer::Buffer clipPlanesBuffer(device, bimeup::renderer::BufferType::Uniform,
                                              sizeof(bimeup::renderer::ClipPlanesUbo),
                                              &clipPlanesUbo);
    descriptorSet.UpdateBuffer(3, clipPlanesBuffer);

    // Shadow map — created lazily on first enable / resolution change from the panel.
    std::unique_ptr<bimeup::renderer::ShadowMap> shadowMap;
    std::uint32_t shadowMapResolution = 0;

    // Placeholder 1×1 shadow map used before the first real build, so the descriptor
    // is always valid (Vulkan validation flags uninitialized COMBINED_IMAGE_SAMPLER reads).
    bimeup::renderer::ShadowMap placeholderShadow(device, 1U);
    descriptorSet.UpdateImage(2, placeholderShadow.GetImageView(), placeholderShadow.GetSampler(),
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // MeshBuffer for all geometry
    bimeup::renderer::MeshBuffer meshBuffer(device);

    renderLoop.SetClearColor(0.15F, 0.15F, 0.18F);

    // UI — initialised before the IFC load so the loading modal can render
    // each frame while the worker thread parses the file.
    bimeup::ui::UIManager uiManager;
    auto initImGui = [&] {
        uiManager.InitVulkanBackend({
            .window = window.GetHandle(),
            .instance = vulkanContext.GetInstance(),
            .physicalDevice = device.GetPhysicalDevice(),
            .device = device.GetDevice(),
            .queueFamily = device.GetGraphicsQueueFamily(),
            .queue = device.GetGraphicsQueue(),
            .renderPass = renderLoop.GetRenderPass(),
            .minImageCount = swapchain.GetImageCount(),
            .imageCount = swapchain.GetImageCount(),
            .apiVersion = VK_API_VERSION_1_2,
            .msaaSamples = renderLoop.GetSampleCount(),
        });
    };
    initImGui();

    bimeup::ui::Theme::Apply();

    // Minimal swapchain recreate usable during loading (no scene / camera deps).
    auto recreateSwapchainForLoading = [&] {
        auto sz = window.GetFramebufferSize();
        while (sz.x == 0 || sz.y == 0) {
            glfwWaitEvents();
            if (window.ShouldClose()) return;
            sz = window.GetFramebufferSize();
        }
        renderLoop.WaitIdle();
        swapchain.Recreate(
            surface,
            VkExtent2D{static_cast<uint32_t>(sz.x), static_cast<uint32_t>(sz.y)});
        renderLoop.RecreateForSwapchain();
    };

    // Load IFC file (use CLI argument if given, otherwise the bundled sample).
    std::string ifcPath = (argc > 1) ? std::string(argv[1]) : std::string(BIMEUP_SAMPLE_IFC);
    LOG_INFO("Loading IFC file: {}", ifcPath);

    bimeup::ifc::AsyncLoader asyncLoader;
    std::atomic<float> loadPercent{0.0F};
    std::mutex loadPhaseMtx;
    std::string loadPhase = "starting";

    auto loadFuture = asyncLoader.LoadAsync(
        ifcPath, [&](float pct, std::string_view phase) {
            loadPercent.store(pct, std::memory_order_release);
            std::lock_guard lk(loadPhaseMtx);
            loadPhase.assign(phase.data(), phase.size());
        });

    bool windowClosedDuringLoad = false;
    while (loadFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        glfwPollEvents();
        if (window.ShouldClose()) {
            asyncLoader.Cancel();
            windowClosedDuringLoad = true;
            loadFuture.wait();
            break;
        }

        if (!renderLoop.BeginFrame()) {
            recreateSwapchainForLoading();
            continue;
        }

        uiManager.BeginFrame();
        {
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always,
                                    ImVec2(0.5F, 0.5F));
            ImGui::Begin("Loading IFC", nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoSavedSettings);
            std::string phaseSnap;
            {
                std::lock_guard lk(loadPhaseMtx);
                phaseSnap = loadPhase;
            }
            ImGui::TextUnformatted(ifcPath.c_str());
            ImGui::Spacing();
            ImGui::Text("Phase: %s", phaseSnap.c_str());
            ImGui::ProgressBar(loadPercent.load(std::memory_order_acquire) / 100.0F,
                               ImVec2(360.0F, 0.0F));
            ImGui::Spacing();
            const bool cancelled = asyncLoader.IsCancelled();
            ImGui::BeginDisabled(cancelled);
            if (ImGui::Button("Cancel", ImVec2(120.0F, 0.0F))) {
                asyncLoader.Cancel();
            }
            ImGui::EndDisabled();
            if (cancelled) {
                ImGui::SameLine();
                ImGui::TextUnformatted("(cancelling…)");
            }
            ImGui::End();
        }
        uiManager.EndFrame(renderLoop.GetCurrentCommandBuffer());

        if (!renderLoop.EndFrame()) {
            recreateSwapchainForLoading();
        }
    }

    auto loadedModel = loadFuture.get();
    if (windowClosedDuringLoad || asyncLoader.IsCancelled() || !loadedModel) {
        if (windowClosedDuringLoad) {
            LOG_INFO("IFC load aborted (window closed)");
        } else if (asyncLoader.IsCancelled()) {
            LOG_INFO("IFC load cancelled by user");
        } else {
            LOG_ERROR("Failed to load IFC file: {}", ifcPath);
        }
        renderLoop.WaitIdle();
        uiManager.ShutdownVulkanBackend();
        bimeup::tools::Log::Shutdown();
        return (windowClosedDuringLoad || asyncLoader.IsCancelled()) ? 0 : 1;
    }
    auto& ifcModel = *loadedModel;
    LOG_INFO("IFC loaded: {} elements", ifcModel.GetElementCount());

    std::optional<bimeup::scene::BuildResult> sceneResult;

    bimeup::scene::SceneBuilder builder;
    builder.SetBatchingEnabled(true);
    sceneResult = builder.Build(ifcModel);

    // Baseline per-type alpha (currently empty — hook for future defaults).
    for (const auto& [ifcType, alpha] : bimeup::scene::DefaultTypeAlphaOverrides()) {
        sceneResult->scene.SetTypeAlphaOverride(ifcType, alpha);
    }
    // Tag translucent sub-meshes (e.g. IfcWindow glass panes carry native
    // alpha<1 from IFC surface styles; frames are opaque) with a per-node
    // baseline so glass fades but frames stay solid.
    bimeup::scene::ApplyTranslucentDefaults(sceneResult->scene,
                                            sceneResult->meshes, 0.4F);

    // Snapshot node→sceneMeshIdx before Upload overwrites node.mesh with the
    // renderer handle — we still need the scene-mesh-index to find triangle
    // ownership when highlighting selected elements.
    std::unordered_map<bimeup::scene::NodeId, size_t> nodeToSceneMeshIdx;
    for (size_t i = 0; i < sceneResult->scene.GetNodeCount(); ++i) {
        auto nodeId = static_cast<bimeup::scene::NodeId>(i);
        const auto& node = sceneResult->scene.GetNode(nodeId);
        if (node.mesh.has_value()) {
            nodeToSceneMeshIdx[nodeId] = node.mesh.value();
        }
    }

    bimeup::core::SceneUploader::Upload(*sceneResult, meshBuffer);

    LOG_INFO("Scene built: {} nodes, {} meshes uploaded",
             sceneResult->scene.GetNodeCount(), meshBuffer.MeshCount());

    // Pipeline
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

    bimeup::renderer::PipelineConfig pipelineConfig{};
    pipelineConfig.renderPass = renderLoop.GetRenderPass();
    pipelineConfig.vertexBindings = {binding};
    pipelineConfig.vertexAttributes = {attrs.begin(), attrs.end()};
    pipelineConfig.descriptorSetLayouts = {dsLayout.GetLayout()};
    pipelineConfig.pushConstantRanges = {pushRange};
    pipelineConfig.depthTestEnable = true;
    pipelineConfig.depthWriteEnable = true;
    // Double-sided rendering so clip-plane-exposed interior faces are visible.
    pipelineConfig.cullMode = VK_CULL_MODE_NONE;

    auto buildPipelines = [&](std::unique_ptr<bimeup::renderer::Pipeline>& shaded,
                              std::unique_ptr<bimeup::renderer::Pipeline>& wire,
                              std::unique_ptr<bimeup::renderer::Pipeline>& transparent) {
        pipelineConfig.renderPass = renderLoop.GetRenderPass();
        pipelineConfig.rasterizationSamples = renderLoop.GetSampleCount();
        pipelineConfig.polygonMode =
            bimeup::renderer::GetPolygonMode(bimeup::renderer::RenderMode::Shaded);
        pipelineConfig.alphaBlendEnable = false;
        pipelineConfig.depthWriteEnable = true;
        shaded = std::make_unique<bimeup::renderer::Pipeline>(device, vertShader, fragShader,
                                                              pipelineConfig);
        pipelineConfig.polygonMode =
            bimeup::renderer::GetPolygonMode(bimeup::renderer::RenderMode::Wireframe);
        wire = std::make_unique<bimeup::renderer::Pipeline>(device, vertShader, fragShader,
                                                            pipelineConfig);
        // Transparent pass: alpha blend on, depth test on, depth write off.
        // Drawn after opaque so blended fragments composite against the opaque layer.
        pipelineConfig.polygonMode =
            bimeup::renderer::GetPolygonMode(bimeup::renderer::RenderMode::Shaded);
        pipelineConfig.alphaBlendEnable = true;
        pipelineConfig.depthWriteEnable = false;
        transparent = std::make_unique<bimeup::renderer::Pipeline>(device, vertShader, fragShader,
                                                                   pipelineConfig);
        pipelineConfig.alphaBlendEnable = false;
        pipelineConfig.depthWriteEnable = true;
    };

    std::unique_ptr<bimeup::renderer::Pipeline> shadedPipeline;
    std::unique_ptr<bimeup::renderer::Pipeline> wireframePipeline;
    std::unique_ptr<bimeup::renderer::Pipeline> transparentPipeline;
    buildPipelines(shadedPipeline, wireframePipeline, transparentPipeline);

    // Section-fill pipeline draws pre-triangulated cap geometry over the scene.
    // Reuses the main descriptor set layout — only binding 0 (camera UBO) is
    // referenced by section_fill.vert; unused bindings are harmless.
    std::unique_ptr<bimeup::renderer::SectionFillPipeline> sectionFillPipeline;
    auto buildSectionFillPipeline = [&] {
        sectionFillPipeline = std::make_unique<bimeup::renderer::SectionFillPipeline>(
            device, sectionFillVertShader, sectionFillFragShader,
            renderLoop.GetRenderPass(), dsLayout.GetLayout(), renderLoop.GetSampleCount());
    };
    buildSectionFillPipeline();

    bimeup::scene::SectionCapGeometry sectionCapGeometry(device);

    // Disk marker pipeline + GPU buffer for the PoV hover preview. Rebuilt on
    // MSAA change alongside the other scene pipelines.
    std::unique_ptr<bimeup::renderer::DiskMarkerPipeline> diskMarkerPipeline;
    auto buildDiskMarkerPipeline = [&] {
        diskMarkerPipeline = std::make_unique<bimeup::renderer::DiskMarkerPipeline>(
            device, diskMarkerVertShader, diskMarkerFragShader,
            renderLoop.GetRenderPass(), dsLayout.GetLayout(),
            renderLoop.GetSampleCount());
    };
    buildDiskMarkerPipeline();
    bimeup::renderer::DiskMarkerBuffer diskMarkerBuffer(device);

    // Shadow pipeline — depth-only, no color attachments, light-space × model push constant.
    VkPushConstantRange shadowPushRange{};
    shadowPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    shadowPushRange.offset = 0;
    shadowPushRange.size = sizeof(glm::mat4);

    std::unique_ptr<bimeup::renderer::Pipeline> shadowPipeline;
    auto buildShadowPipeline = [&] {
        if (!shadowMap) return;
        bimeup::renderer::PipelineConfig cfg{};
        cfg.renderPass = shadowMap->GetRenderPass();
        cfg.vertexBindings = {binding};
        cfg.vertexAttributes = {attrs.begin(), attrs.end()};
        cfg.pushConstantRanges = {shadowPushRange};
        cfg.depthTestEnable = true;
        cfg.depthWriteEnable = true;
        cfg.cullMode = VK_CULL_MODE_FRONT_BIT;  // reduce self-shadow acne on front faces
        cfg.colorAttachmentCount = 0;
        cfg.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        shadowPipeline = std::make_unique<bimeup::renderer::Pipeline>(
            device, shadowVertShader, shadowFragShader, cfg);
    };

    auto renderMode = bimeup::renderer::RenderMode::Shaded;

    // Camera — set orbit target and distance based on scene bounds
    bimeup::renderer::Camera camera;
    camera.SetPerspective(45.0F, static_cast<float>(fbSize.x) / static_cast<float>(fbSize.y),
                          0.1F, 1000.0F);

    auto bounds = ComputeSceneBounds(sceneResult->scene);
    FitCameraToBounds(camera, bounds);
    if (bounds.IsValid()) {
        glm::vec3 center = bounds.GetCenter();
        glm::vec3 size = bounds.GetSize();
        LOG_INFO("Scene bounds: center=({:.1f},{:.1f},{:.1f}) size=({:.1f},{:.1f},{:.1f})",
                 center.x, center.y, center.z, size.x, size.y, size.z);
    }

    // Collect draw calls for the scene
    std::vector<std::pair<bimeup::renderer::MeshHandle, glm::mat4>> drawCalls =
        CollectDrawCalls(sceneResult->scene);
    LOG_INFO("Draw calls: {}", drawCalls.size());

    // Event bus + selection (must exist before input callbacks so picking can publish events).
    bimeup::core::EventBus eventBus;
    bimeup::core::Selection selection(eventBus);
    bimeup::scene::MeasureTool measureTool;
    bool measureModeActive = false;
    std::optional<glm::vec3> measureSnapPoint;
    bool measureSnapIsVertex = false;

    // Precompute per-node lists of global vertex indices in the MeshBuffer so
    // that selection highlighting only needs a set-lookup, not a full scan of
    // every mesh's triangle owners on each click.
    std::unordered_map<bimeup::scene::NodeId, std::vector<uint32_t>> nodeVertexIndices;
    std::unordered_map<uint32_t, std::vector<bimeup::scene::NodeId>> expressIdToNodes;
    for (size_t i = 0; i < sceneResult->scene.GetNodeCount(); ++i) {
        auto nodeId = static_cast<bimeup::scene::NodeId>(i);
        const auto& node = sceneResult->scene.GetNode(nodeId);
        if (node.expressId != 0) {
            expressIdToNodes[node.expressId].push_back(nodeId);
        }
        if (!node.mesh.has_value()) continue;
        auto it = nodeToSceneMeshIdx.find(nodeId);
        if (it == nodeToSceneMeshIdx.end()) continue;

        const auto& sceneMesh = sceneResult->meshes[it->second];
        const auto& owners = sceneMesh.GetTriangleOwners();
        const auto& indices = sceneMesh.GetIndices();
        auto params = meshBuffer.GetDrawParams(node.mesh.value());
        int32_t baseVertex = params.vertexOffset;

        std::vector<uint32_t>& out = nodeVertexIndices[nodeId];
        if (owners.empty()) {
            // Un-batched mesh — every vertex in this mesh belongs to this node.
            out.reserve(sceneMesh.GetVertexCount());
            for (size_t v = 0; v < sceneMesh.GetVertexCount(); ++v) {
                out.push_back(static_cast<uint32_t>(baseVertex + v));
            }
        } else {
            size_t triCount = indices.size() / 3;
            for (size_t tri = 0; tri < triCount && tri < owners.size(); ++tri) {
                if (owners[tri] != nodeId) continue;
                for (int k = 0; k < 3; ++k) {
                    out.push_back(static_cast<uint32_t>(baseVertex + indices[tri * 3 + k]));
                }
            }
        }
    }

    constexpr glm::vec4 kHighlightColor(1.0F, 0.85F, 0.2F, 1.0F);
    selection.SetOnChanged([&] {
        std::vector<uint32_t> verts;
        for (auto expressId : selection.Ids()) {
            auto it = expressIdToNodes.find(expressId);
            if (it == expressIdToNodes.end()) continue;
            for (auto nodeId : it->second) {
                auto vit = nodeVertexIndices.find(nodeId);
                if (vit == nodeVertexIndices.end()) continue;
                verts.insert(verts.end(), vit->second.begin(), vit->second.end());
            }
        }
        meshBuffer.SetVertexColorOverride(verts, kHighlightColor);
    });

    // 7.8d.4 — Per-vertex alpha override pipeline. Walks all mesh-bearing
    // nodes, reads each one's effective alpha (element override > type
    // override > none) from the scene, and uploads the union as a per-vertex
    // alpha override on the MeshBuffer. `handlesWithAlphaOverride` lets the
    // draw loop route forced-transparent meshes into the transparent pass.
    // Rebuild is hash-gated so we only re-upload when something actually changed.
    std::unordered_set<bimeup::renderer::MeshHandle> handlesWithAlphaOverride;
    std::size_t alphaOverrideHash = 0;
    auto rebuildAlphaOverrides = [&] {
        std::size_t hash = 0;
        std::vector<std::tuple<bimeup::scene::NodeId, float>> perNode;
        for (size_t i = 0; i < sceneResult->scene.GetNodeCount(); ++i) {
            auto nodeId = static_cast<bimeup::scene::NodeId>(i);
            const auto& node = sceneResult->scene.GetNode(nodeId);
            if (!node.mesh.has_value()) continue;
            auto eff = sceneResult->scene.GetEffectiveAlpha(nodeId);
            if (!eff.has_value()) continue;
            perNode.emplace_back(nodeId, *eff);
            // FNV-1a-ish hash mixing nodeId and quantized alpha (0..1000).
            auto q = static_cast<uint32_t>(*eff * 1000.0F);
            hash ^= std::hash<uint32_t>{}(nodeId) + 0x9E3779B9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<uint32_t>{}(q) + 0x9E3779B9 + (hash << 6) + (hash >> 2);
        }
        if (hash == alphaOverrideHash) return;
        alphaOverrideHash = hash;

        std::vector<std::pair<uint32_t, float>> pairs;
        handlesWithAlphaOverride.clear();
        for (const auto& [nodeId, alpha] : perNode) {
            auto vit = nodeVertexIndices.find(nodeId);
            if (vit == nodeVertexIndices.end()) continue;
            for (auto v : vit->second) pairs.emplace_back(v, alpha);
            const auto& node = sceneResult->scene.GetNode(nodeId);
            if (node.mesh.has_value()) handlesWithAlphaOverride.insert(*node.mesh);
        }
        meshBuffer.SetVertexAlphaOverride(pairs);
    };

    auto hierarchy = std::make_unique<bimeup::ifc::IfcHierarchy>(ifcModel);

    // Input: orbit camera with mouse
    bool middleMouseDown = false;
    glm::dvec2 lastMousePos{0.0, 0.0};

    // First-person mode: armed by Toolbar "Point of View" (`pointOfViewArmed`),
    // entered on click on an IfcSlab top face (`firstPersonActive`). While
    // active, FPC owns the camera. Mirrored from the toolbar callback so the
    // input handlers (declared before the toolbar) can read it.
    bimeup::renderer::FirstPersonController fpc;
    bool pointOfViewArmed = false;
    bool firstPersonActive = false;

    // While PoV is active we hide measurements instead of deleting them so
    // they reappear when the user exits. Snapshot of the per-item `visible`
    // flags taken at PoV-on, restored at PoV-off.
    std::vector<bool> measurementVisibilitySnapshot;

    // Assigned after the toolbar exists (line ~1010). Declared here so the
    // input handlers registered below can route Esc / the exit button
    // through the same path.
    std::function<void()> exitFirstPerson;

    // Hover disk preview: the mouse-move handler tags the last valid slab hit
    // while PoV is armed (and FPC isn't already driving the camera). The main
    // loop rebuilds the GPU buffer from this state and draws the disk.
    bool hoverDiskValid = false;
    glm::vec3 hoverDiskCenter(0.0F);
    glm::vec3 hoverDiskNormal(0.0F, 1.0F, 0.0F);
    constexpr float kHoverDiskRadius = 0.35F;
    const glm::vec4 kHoverDiskColor(0.70F, 0.30F, 0.95F, 0.60F);

    // Shared helpers for picking. These capture references to the renderer/scene state used each frame.
    auto buildViewProj = [&]() {
        glm::mat4 proj = camera.GetProjectionMatrix();
        proj[1][1] *= -1.0F;  // Undo Vulkan Y-flip so picking math matches the rasterized image.
        return std::make_pair(camera.GetViewMatrix(), proj);
    };
    auto windowSize = [&]() {
        auto fb = window.GetFramebufferSize();
        return glm::vec2(static_cast<float>(fb.x), static_cast<float>(fb.y));
    };
    auto imguiWantsMouse = [] { return ImGui::GetIO().WantCaptureMouse; };

    // Snap a hit point to the nearest of the hit triangle's three vertices,
    // using a screen-aware threshold (~3% of view distance) so close-up snaps
    // tightly while far-away snaps still feel forgiving.
    auto snapHit = [](const bimeup::scene::RayHit& hit) {
        const glm::vec3& p = hit.point;
        const glm::vec3* verts[3] = {&hit.triV0, &hit.triV1, &hit.triV2};
        float bestDistSq = std::numeric_limits<float>::infinity();
        const glm::vec3* best = nullptr;
        for (auto* v : verts) {
            const glm::vec3 d = *v - p;
            const float d2 = glm::dot(d, d);
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                best = v;
            }
        }
        const float threshold = std::max(0.05F, hit.t * 0.03F);
        if (best != nullptr && std::sqrt(bestDistSq) <= threshold) {
            return std::make_pair(*best, true);  // vertex snap
        }
        return std::make_pair(p, false);  // face snap
    };

    // Forward-declared so the mouse-move orbit handler can auto-exit plan
    // view; assigned after UIManager owns the panel.
    bimeup::ui::PlanViewPanel* planViewPanel = nullptr;

    input.OnMouseButton([&](bimeup::platform::MouseButton btn, bool pressed) {
        // Right-click while a measurement is pending → cancel.
        if (btn == bimeup::platform::MouseButton::Right && pressed && measureModeActive &&
            measureTool.GetFirstPoint().has_value() && !imguiWantsMouse()) {
            measureTool.Cancel();
            LOG_INFO("Measure: cancelled");
            return;
        }

        if (btn == bimeup::platform::MouseButton::Middle) {
            middleMouseDown = pressed;
            lastMousePos = input.GetMousePosition();
        }
        if (btn == bimeup::platform::MouseButton::Left && pressed && !imguiWantsMouse()) {
            auto mouse = input.GetMousePosition();
            const glm::vec2 sp(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
            if (measureModeActive) {
                // Commit the snap candidate updated each hover frame. If the cursor
                // isn't over geometry, no snap → no point dropped.
                if (measureSnapPoint.has_value()) {
                    measureTool.AddPoint(*measureSnapPoint);
                    if (auto& res = measureTool.GetResult(); res.has_value()) {
                        LOG_INFO("Measure: {:.3f} m  ({:.2f},{:.2f},{:.2f}) → "
                                 "({:.2f},{:.2f},{:.2f})",
                                 res->distance, res->pointA.x, res->pointA.y, res->pointA.z,
                                 res->pointB.x, res->pointB.y, res->pointB.z);
                    }
                }
            } else if (pointOfViewArmed) {
                // PoV-armed: click on an IfcSlab top face teleports to hit + (0,1.5,0),
                // gaze along +Z. Filter to IfcSlab so walls/other elements between
                // the camera and the floor don't block the hit. Non-top slab hits
                // (underside) still fall through as no-ops.
                auto [view, proj] = buildViewProj();
                auto ray = bimeup::core::ScreenPointToRay(sp, windowSize(), view, proj);
                bimeup::scene::NodeFilter slabOnly =
                    [](const bimeup::scene::SceneNode& n) { return n.ifcType == "IfcSlab"; };
                if (auto hit = bimeup::scene::RaycastScene(ray, sceneResult->scene,
                                                          sceneResult->meshes, slabOnly)) {
                    const auto& node = sceneResult->scene.GetNode(hit->nodeId);
                    glm::vec3 n = glm::cross(hit->triV1 - hit->triV0, hit->triV2 - hit->triV0);
                    const float nlen = glm::length(n);
                    if (nlen > 0.0F) n /= nlen;
                    const bool topFace = ray.direction.y < 0.0F && std::abs(n.y) > 0.7F;
                    if (node.ifcType == "IfcSlab" && topFace) {
                        fpc.SetPosition(hit->point + glm::vec3(0.0F, 1.5F, 0.0F));
                        fpc.SetYawPitch(glm::pi<float>(), 0.0F);
                        firstPersonActive = true;
                        hoverDiskValid = false;
                        // Teleport drops us into the scene; restore full opacity
                        // so the walking experience isn't see-through.
                        bimeup::scene::ClearPointOfViewAlpha(sceneResult->scene);
                        fpc.ApplyTo(camera);
                        LOG_INFO("PoV teleport: ({:.2f},{:.2f},{:.2f})",
                                 fpc.GetPosition().x, fpc.GetPosition().y, fpc.GetPosition().z);
                    }
                }
            } else {
                auto [view, proj] = buildViewProj();
                bimeup::core::PickElement(sp, windowSize(), view, proj, sceneResult->scene,
                                          sceneResult->meshes, eventBus,
                                          ImGui::GetIO().KeyCtrl);
            }
        }
    });

    input.OnMouseMove([&](double x, double y) {
        glm::dvec2 pos{x, y};
        glm::dvec2 delta = pos - lastMousePos;
        lastMousePos = pos;

        if (firstPersonActive && !imguiWantsMouse() && !middleMouseDown) {
            fpc.Look(glm::vec2(static_cast<float>(delta.x), static_cast<float>(delta.y)),
                     0.003F);
            fpc.ApplyTo(camera);
            return;
        }

        if (middleMouseDown) {
            bimeup::renderer::NavModifiers mods{
                ImGui::GetIO().KeyShift, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyAlt};
            auto action = bimeup::renderer::ClassifyDrag(
                bimeup::renderer::NavButton::Middle, mods);
            switch (action) {
                case bimeup::renderer::NavAction::Orbit:
                    camera.Orbit(static_cast<float>(delta.x) * 0.005F,
                                 static_cast<float>(delta.y) * 0.005F);
                    if (planViewPanel != nullptr && planViewPanel->ActiveLevel() >= 0) {
                        planViewPanel->Deactivate();
                    }
                    break;
                case bimeup::renderer::NavAction::Pan:
                    camera.Pan(glm::vec2(static_cast<float>(-delta.x) * 0.005F,
                                         static_cast<float>(delta.y) * 0.005F));
                    break;
                case bimeup::renderer::NavAction::Dolly:
                    camera.Zoom(static_cast<float>(delta.y) * 0.02F);
                    break;
                case bimeup::renderer::NavAction::None:
                    break;
            }
        }

        if (!middleMouseDown && !imguiWantsMouse()) {
            auto [view, proj] = buildViewProj();
            const glm::vec2 sp(static_cast<float>(x), static_cast<float>(y));
            if (measureModeActive) {
                auto ray = bimeup::core::ScreenPointToRay(sp, windowSize(), view, proj);
                if (auto hit = bimeup::scene::RaycastScene(ray, sceneResult->scene,
                                                          sceneResult->meshes)) {
                    auto [snap, isVertex] = snapHit(*hit);
                    measureSnapPoint = snap;
                    measureSnapIsVertex = isVertex;
                } else {
                    measureSnapPoint.reset();
                    measureSnapIsVertex = false;
                }
            } else if (pointOfViewArmed && !firstPersonActive) {
                // PoV preview: drop the disk on IfcSlab top faces under the
                // cursor. Filter to IfcSlab so walls/other occluders don't
                // stop the ray from landing on the floor behind them.
                auto ray = bimeup::core::ScreenPointToRay(sp, windowSize(), view, proj);
                hoverDiskValid = false;
                bimeup::scene::NodeFilter slabOnly =
                    [](const bimeup::scene::SceneNode& n) { return n.ifcType == "IfcSlab"; };
                if (auto hit = bimeup::scene::RaycastScene(ray, sceneResult->scene,
                                                          sceneResult->meshes, slabOnly)) {
                    const auto& node = sceneResult->scene.GetNode(hit->nodeId);
                    glm::vec3 n = glm::cross(hit->triV1 - hit->triV0,
                                             hit->triV2 - hit->triV0);
                    const float nlen = glm::length(n);
                    if (nlen > 0.0F) n /= nlen;
                    if (n.y < 0.0F) n = -n;  // orient up so the disk faces +Y.
                    const bool topFace = ray.direction.y < 0.0F && std::abs(n.y) > 0.7F;
                    if (node.ifcType == "IfcSlab" && topFace) {
                        hoverDiskValid = true;
                        hoverDiskCenter = hit->point;
                        hoverDiskNormal = n;
                    }
                }
            } else {
                bimeup::core::HoverElement(sp, windowSize(), view, proj, sceneResult->scene,
                                           sceneResult->meshes, eventBus);
            }
        }
    });

    input.OnScroll([&](double /*xOffset*/, double yOffset) {
        if (firstPersonActive) return;  // FPC owns the camera; ignore scroll.
        camera.Zoom(static_cast<float>(-yOffset) * 0.5F);
    });

    bool fitToViewRequested = false;

    input.OnKey([&](bimeup::platform::Key key, bool pressed) {
        if (key == bimeup::platform::Key::Escape && pressed) {
            if (firstPersonActive) {
                exitFirstPerson();
            } else {
                glfwSetWindowShouldClose(window.GetHandle(), GLFW_TRUE);
            }
            return;
        }
        // While walking, swallow every other key so W doesn't flip wireframe,
        // Home doesn't yank the camera, etc. Movement is polled separately.
        if (firstPersonActive) {
            return;
        }
        if (key == bimeup::platform::Key::W && pressed) {
            renderMode = (renderMode == bimeup::renderer::RenderMode::Shaded)
                             ? bimeup::renderer::RenderMode::Wireframe
                             : bimeup::renderer::RenderMode::Shaded;
            LOG_INFO("Render mode: {}",
                     renderMode == bimeup::renderer::RenderMode::Shaded ? "Shaded" : "Wireframe");
        }
        if (key == bimeup::platform::Key::Numpad5 && pressed) {
            camera.ToggleProjection();
            LOG_INFO("Projection: {}", camera.IsOrthographic() ? "Orthographic" : "Perspective");
        }
        // Frame-all: same as toolbar Fit-to-View.
        if (key == bimeup::platform::Key::Home && pressed) {
            fitToViewRequested = true;
        }
        // Frame-selected: fit camera to union-bounds of selected elements.
        // Falls back to frame-all when nothing is selected.
        if (key == bimeup::platform::Key::NumpadDecimal && pressed) {
            bimeup::scene::AABB selBounds;
            for (auto expressId : selection.Ids()) {
                auto it = expressIdToNodes.find(expressId);
                if (it == expressIdToNodes.end()) continue;
                for (auto nodeId : it->second) {
                    const auto& node = sceneResult->scene.GetNode(nodeId);
                    if (node.bounds.IsValid()) {
                        selBounds = bimeup::scene::AABB::Merge(selBounds, node.bounds);
                    }
                }
            }
            if (selBounds.IsValid()) {
                FitCameraToBounds(camera, selBounds);
            } else {
                fitToViewRequested = true;
            }
        }
        // Reset pivot: park the orbit target at the world origin (Blender Shift+C).
        if (key == bimeup::platform::Key::C && pressed &&
            input.IsKeyDown(bimeup::platform::Key::LeftShift)) {
            camera.SetOrbitTarget(glm::vec3(0.0F));
        }
        // Axis-aligned preset views (Blender convention, Y-up).
        // Numpad 1/3/7 → Front/Right/Top; hold Ctrl for the opposite side.
        if (pressed &&
            (key == bimeup::platform::Key::Numpad1 ||
             key == bimeup::platform::Key::Numpad3 ||
             key == bimeup::platform::Key::Numpad7)) {
            bool ctrl = input.IsKeyDown(bimeup::platform::Key::LeftControl) ||
                        input.IsKeyDown(bimeup::platform::Key::RightControl);
            bimeup::renderer::AxisView view = bimeup::renderer::AxisView::Front;
            if (key == bimeup::platform::Key::Numpad1) {
                view = ctrl ? bimeup::renderer::AxisView::Back
                            : bimeup::renderer::AxisView::Front;
            } else if (key == bimeup::platform::Key::Numpad3) {
                view = ctrl ? bimeup::renderer::AxisView::Left
                            : bimeup::renderer::AxisView::Right;
            } else {
                view = ctrl ? bimeup::renderer::AxisView::Bottom
                            : bimeup::renderer::AxisView::Top;
            }
            camera.SetAxisView(view);
        }
    });

    // Construct panels and keep raw pointers so we can update them each frame.
    auto toolbarOwned = std::make_unique<bimeup::ui::Toolbar>();
    auto hierarchyOwned = std::make_unique<bimeup::ui::HierarchyPanel>();
    auto propertyOwned = std::make_unique<bimeup::ui::PropertyPanel>();
    auto overlayOwned = std::make_unique<bimeup::ui::ViewportOverlay>();
    auto measurementsOwned = std::make_unique<bimeup::ui::MeasurementsPanel>();
    auto renderQualityOwned = std::make_unique<bimeup::ui::RenderQualityPanel>();
    auto axisSectionOwned = std::make_unique<bimeup::ui::AxisSectionPanel>();
    auto planViewOwned = std::make_unique<bimeup::ui::PlanViewPanel>();
    auto typeVisibilityOwned = std::make_unique<bimeup::ui::TypeVisibilityPanel>();
    auto firstPersonExitOwned = std::make_unique<bimeup::ui::FirstPersonExitPanel>();

    auto* toolbar = toolbarOwned.get();
    auto* hierarchyPanel = hierarchyOwned.get();
    auto* propertyPanel = propertyOwned.get();
    auto* overlay = overlayOwned.get();
    auto* measurementsPanel = measurementsOwned.get();
    auto* renderQualityPanel = renderQualityOwned.get();
    auto* axisSectionPanel = axisSectionOwned.get();
    planViewPanel = planViewOwned.get();
    auto* typeVisibilityPanel = typeVisibilityOwned.get();
    auto* firstPersonExitPanel = firstPersonExitOwned.get();
    typeVisibilityPanel->SetScene(&sceneResult->scene);
    typeVisibilityPanel->ApplyDefaults();
    measurementsPanel->SetTool(&measureTool);
    measurementsPanel->SetOnClearAll([&] { measureTool.ClearMeasurements(); });
    axisSectionPanel->SetController(&axisSectionController);
    {
        auto axisBounds = ComputeSceneBounds(sceneResult->scene);
        if (axisBounds.IsValid()) {
            const glm::vec3 mn = axisBounds.GetMin();
            const glm::vec3 mx = axisBounds.GetMax();
            const float lo = std::min({mn.x, mn.y, mn.z});
            const float hi = std::max({mx.x, mx.y, mx.z});
            // Pad 10% so extreme offsets can push the plane just past the model.
            const float pad = 0.1F * std::max(hi - lo, 1.0F);
            axisSectionPanel->SetOffsetRange(lo - pad, hi + pad);
        }
    }
    planViewPanel->SetClipPlaneManager(&clipPlaneManager);
    planViewPanel->SetCamera(&camera);
    planViewPanel->SetLevels({
        {"Ground Floor", 0.0F},
        {"Roof", 2.5F},
    });
    {
        auto planBounds = ComputeSceneBounds(sceneResult->scene);
        if (planBounds.IsValid()) {
            planViewPanel->SetSceneBounds(planBounds.GetMin(), planBounds.GetMax());
        }
        const auto initialSize = windowSize();
        if (initialSize.y > 0) {
            planViewPanel->SetViewportAspect(
                static_cast<float>(initialSize.x) / static_cast<float>(initialSize.y));
        }
    }

    hierarchyPanel->SetEventBus(&eventBus);
    toolbar->SetRenderMode(renderMode);

    hierarchyPanel->SetRoot(&hierarchy->GetRoot());

    {
        auto collectExpressIds = [](const bimeup::ifc::HierarchyNode& root) {
            std::vector<std::uint32_t> ids;
            std::function<void(const bimeup::ifc::HierarchyNode&)> walk =
                [&](const bimeup::ifc::HierarchyNode& n) {
                    ids.push_back(n.expressId);
                    for (const auto& c : n.children) walk(c);
                };
            walk(root);
            return ids;
        };
        auto& scene = sceneResult->scene;
        hierarchyPanel->SetVisibilityQuery(
            [&scene, collectExpressIds](const bimeup::ifc::HierarchyNode& n) {
                for (auto id : collectExpressIds(n)) {
                    for (auto nodeId : scene.FindByExpressId(id)) {
                        const auto& sn = scene.GetNode(nodeId);
                        if (sn.mesh.has_value() && sn.visible) return true;
                    }
                }
                return false;
            });
        hierarchyPanel->SetOnToggleVisibility(
            [&scene, collectExpressIds](const bimeup::ifc::HierarchyNode& n) {
                auto ids = collectExpressIds(n);
                bool currentlyVisible = false;
                for (auto id : ids) {
                    for (auto nodeId : scene.FindByExpressId(id)) {
                        const auto& sn = scene.GetNode(nodeId);
                        if (sn.mesh.has_value() && sn.visible) {
                            currentlyVisible = true;
                            break;
                        }
                    }
                    if (currentlyVisible) break;
                }
                const bool next = !currentlyVisible;
                for (auto id : ids) {
                    for (auto nodeId : scene.FindByExpressId(id)) {
                        if (scene.GetNode(nodeId).mesh.has_value()) {
                            scene.SetVisibility(nodeId, next);
                        }
                    }
                }
            });
        // Active isolation root expressId (if any). Clicking Isolate again on
        // the same row turns isolation back off (ShowAll).
        static std::optional<std::uint32_t> s_isolationRoot;
        s_isolationRoot.reset();

        hierarchyPanel->SetOnIsolate(
            [&scene, collectExpressIds, typeVisibilityPanel](
                const bimeup::ifc::HierarchyNode& n) {
                if (s_isolationRoot.has_value() && *s_isolationRoot == n.expressId) {
                    scene.ShowAll();
                    s_isolationRoot.reset();
                    // Re-apply Types-panel hidden list so IfcSpace etc. stay hidden.
                    typeVisibilityPanel->ReapplyToScene();
                } else {
                    auto ids = collectExpressIds(n);
                    std::unordered_set<std::uint32_t> keep(ids.begin(), ids.end());
                    scene.IsolateByExpressId(keep);
                    s_isolationRoot = n.expressId;
                }
            });
        hierarchyPanel->SetIsolationQuery(
            [](const bimeup::ifc::HierarchyNode& n) {
                return s_isolationRoot.has_value() && *s_isolationRoot == n.expressId;
            });
        hierarchyPanel->SetTypeVisibilityQuery(
            [typeVisibilityPanel](const std::string& ifcType) {
                return typeVisibilityPanel->IsTypeVisible(ifcType);
            });
    }

    auto selectionBridge = std::make_unique<bimeup::ui::SelectionBridge>(
        eventBus, *propertyPanel,
        [&ifcModel](uint32_t expressId) { return ifcModel.GetElement(expressId); });

    uiManager.AddPanel(std::move(toolbarOwned));
    uiManager.AddPanel(std::move(hierarchyOwned));
    uiManager.AddPanel(std::move(propertyOwned));
    uiManager.AddPanel(std::move(overlayOwned));
    uiManager.AddPanel(std::move(measurementsOwned));
    uiManager.AddPanel(std::move(renderQualityOwned));
    uiManager.AddPanel(std::move(axisSectionOwned));
    uiManager.AddPanel(std::move(planViewOwned));
    uiManager.AddPanel(std::move(typeVisibilityOwned));
    uiManager.AddPanel(std::move(firstPersonExitOwned));

    toolbar->SetOnRenderModeChanged([&](bimeup::renderer::RenderMode mode) {
        renderMode = mode;
        LOG_INFO("Render mode: {}",
                 renderMode == bimeup::renderer::RenderMode::Shaded ? "Shaded" : "Wireframe");
    });
    toolbar->SetOnFitToView([&] { fitToViewRequested = true; });
    toolbar->SetOnFrameSelected([&] {
        bimeup::scene::AABB selBounds;
        for (auto expressId : selection.Ids()) {
            auto it = expressIdToNodes.find(expressId);
            if (it == expressIdToNodes.end()) continue;
            for (auto nodeId : it->second) {
                const auto& node = sceneResult->scene.GetNode(nodeId);
                if (node.bounds.IsValid()) {
                    selBounds = bimeup::scene::AABB::Merge(selBounds, node.bounds);
                }
            }
        }
        if (selBounds.IsValid()) {
            FitCameraToBounds(camera, selBounds);
        } else {
            fitToViewRequested = true;
        }
    });
    toolbar->SetOnOpenFile([] { LOG_INFO("Toolbar: Open File (not implemented yet)"); });
    toolbar->SetOnMeasureModeChanged([&](bool active) {
        measureModeActive = active;
        measureTool.Cancel();  // drop in-progress; keep saved history
        measureSnapPoint.reset();
        measureSnapIsVertex = false;
        LOG_INFO("Measure mode: {}", active ? "on" : "off");
    });
    toolbar->SetOnMeasurementsVisibilityChanged([&](bool visible) {
        measureTool.SetAllVisibility(visible);
        LOG_INFO("Show Measurements: {}", visible ? "on" : "off");
    });
    toolbar->SetOnPointOfViewChanged([&](bool active) {
        if (!sceneResult) {
            return;
        }
        pointOfViewArmed = active;
        if (active) {
            // Hide measurements so they don't clutter the first-person
            // view, but preserve per-item visibility so exiting PoV brings
            // them back in the exact state the user left them in. The
            // Toolbar has already flipped Measure mode off via mutex.
            measureTool.Cancel();
            measureSnapPoint.reset();
            measureSnapIsVertex = false;
            const auto& items = measureTool.GetMeasurements();
            measurementVisibilitySnapshot.clear();
            measurementVisibilitySnapshot.reserve(items.size());
            for (const auto& r : items) {
                measurementVisibilitySnapshot.push_back(r.visible);
            }
            measureTool.SetAllVisibility(false);
            bimeup::scene::ApplyPointOfViewAlpha(sceneResult->scene, 0.2F);
        } else {
            bimeup::scene::ClearPointOfViewAlpha(sceneResult->scene);
            firstPersonActive = false;
            const auto& items = measureTool.GetMeasurements();
            for (std::size_t i = 0; i < items.size() &&
                                    i < measurementVisibilitySnapshot.size();
                 ++i) {
                measureTool.SetVisibility(i, measurementVisibilitySnapshot[i]);
            }
            measurementVisibilitySnapshot.clear();
        }
        hoverDiskValid = false;
        LOG_INFO("Point of View: {}", active ? "on" : "off");
    });

    // Exits first-person mode: flips the toolbar checkbox off (which clears
    // ghost alpha + firstPersonActive via its callback) and frames the scene
    // so the user sees the whole model again. No-op outside FPS. Stored in
    // the std::function declared up-top so the Esc handler can share it.
    exitFirstPerson = [&] {
        if (!firstPersonActive) {
            return;
        }
        toolbar->TriggerPointOfView(false);
        fitToViewRequested = true;
    };
    firstPersonExitPanel->SetOnExit(exitFirstPerson);

    // Depth-only shadow pass — recorded before the main render pass each frame
    // when shadows are enabled. Renders scene geometry from the key light's POV
    // into `shadowMap`; the main pass then samples it via sampler2DShadow.
    renderLoop.SetPreMainPassCallback([&](VkCommandBuffer cmd) {
        if (!shadowMap || !shadowPipeline ||
            !renderQualityPanel->GetSettings().lighting.shadow.enabled) {
            return;
        }

        VkClearValue clear{};
        clear.depthStencil = {1.0F, 0};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = shadowMap->GetRenderPass();
        rpInfo.framebuffer = shadowMap->GetFramebuffer();
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = {shadowMap->GetResolution(), shadowMap->GetResolution()};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clear;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.width = static_cast<float>(shadowMap->GetResolution());
        vp.height = static_cast<float>(shadowMap->GetResolution());
        vp.minDepth = 0.0F;
        vp.maxDepth = 1.0F;
        vkCmdSetViewport(cmd, 0, 1, &vp);

        VkRect2D sc{};
        sc.extent = {shadowMap->GetResolution(), shadowMap->GetResolution()};
        vkCmdSetScissor(cmd, 0, 1, &sc);

        shadowPipeline->Bind(cmd);
        meshBuffer.Bind(cmd);

        const glm::mat4& ls =
            renderQualityPanel->GetSettings().lighting.shadow.lightSpaceMatrix;
        for (const auto& [handle, transform] : drawCalls) {
            glm::mat4 lightSpaceModel = ls * transform;
            vkCmdPushConstants(cmd, shadowPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(glm::mat4), &lightSpaceModel);
            meshBuffer.Draw(cmd, handle);
        }

        vkCmdEndRenderPass(cmd);
    });

    // Rebuild swapchain + dependent resources when the framebuffer size changes
    // or the presentation engine returns OUT_OF_DATE/SUBOPTIMAL. Without this,
    // maximizing the window would stretch the original 1280x720 image across
    // the display (blurry UI + scene).
    auto recreateSwapchain = [&] {
        auto size = window.GetFramebufferSize();
        // Wait out minimization (zero framebuffer size).
        while (size.x == 0 || size.y == 0) {
            glfwWaitEvents();
            if (window.ShouldClose()) return;
            size = window.GetFramebufferSize();
        }
        renderLoop.WaitIdle();
        swapchain.Recreate(
            surface,
            VkExtent2D{static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y)});
        renderLoop.RecreateForSwapchain();
        const float aspect = static_cast<float>(size.x) / static_cast<float>(size.y);
        camera.SetAspect(aspect);
        planViewPanel->SetViewportAspect(aspect);
    };

    // FPS tracking
    double lastFrameTime = glfwGetTime();
    float smoothedFps = 0.0F;

    // Main loop
    while (!window.ShouldClose()) {
        window.PollEvents();

        // Rebuild the per-frame draw list so type-visibility toggles (and any
        // other runtime visibility changes) take effect immediately.
        drawCalls = CollectDrawCalls(sceneResult->scene);

        // Detect resize / DPI change from the window manager before we try to
        // acquire a swapchain image. (Some compositors don't report OUT_OF_DATE
        // on maximize, so polling the fb size is the reliable trigger.)
        {
            auto fb = window.GetFramebufferSize();
            VkExtent2D cur = swapchain.GetExtent();
            if (fb.x > 0 && fb.y > 0 &&
                (static_cast<uint32_t>(fb.x) != cur.width ||
                 static_cast<uint32_t>(fb.y) != cur.height)) {
                recreateSwapchain();
            }
        }

        double now = glfwGetTime();
        double dt = now - lastFrameTime;
        lastFrameTime = now;
        if (dt > 0.0) {
            float instantFps = static_cast<float>(1.0 / dt);
            smoothedFps = smoothedFps == 0.0F ? instantFps : (smoothedFps * 0.9F + instantFps * 0.1F);
        }

        // First-person walking: WASD + arrows feed FPC.Move; FPC drives camera.
        // Polled here (not key callbacks) so held keys produce continuous motion.
        if (firstPersonActive && !ImGui::GetIO().WantCaptureKeyboard) {
            glm::vec3 moveInput(0.0F);
            if (input.IsKeyDown(bimeup::platform::Key::A) ||
                input.IsKeyDown(bimeup::platform::Key::Left)) moveInput.x -= 1.0F;
            if (input.IsKeyDown(bimeup::platform::Key::D) ||
                input.IsKeyDown(bimeup::platform::Key::Right)) moveInput.x += 1.0F;
            if (input.IsKeyDown(bimeup::platform::Key::W) ||
                input.IsKeyDown(bimeup::platform::Key::Up)) moveInput.z += 1.0F;
            if (input.IsKeyDown(bimeup::platform::Key::S) ||
                input.IsKeyDown(bimeup::platform::Key::Down)) moveInput.z -= 1.0F;
            constexpr float kWalkSpeed = 3.0F;  // m/s
            if (glm::length(moveInput) > 0.0F) {
                fpc.Move(moveInput, static_cast<float>(dt), kWalkSpeed);
            }
            fpc.ApplyTo(camera);
        }

        // Fit-to-view request from toolbar: recenter + reset zoom to fit scene bounds.
        if (fitToViewRequested) {
            fitToViewRequested = false;
            FitCameraToBounds(camera, ComputeSceneBounds(sceneResult->scene));
        }

        // MSAA sample-count change from the Render Quality panel. Clamp request
        // against device support, then rebuild render pass / attachments /
        // pipelines / ImGui backend to match.
        {
            int requested = renderQualityPanel->GetSettings().msaaSamples;
            VkSampleCountFlagBits desired =
                bimeup::renderer::ClampSampleCount(requested, supportedSamples);
            if (desired != currentSamples) {
                renderLoop.WaitIdle();
                uiManager.ShutdownVulkanBackend();
                renderLoop.SetSampleCount(desired);
                currentSamples = desired;
                buildPipelines(shadedPipeline, wireframePipeline, transparentPipeline);
                buildSectionFillPipeline();
                buildDiskMarkerPipeline();
                initImGui();
                // Reflect the clamped value back to the panel so the UI doesn't
                // claim e.g. 8x when the device only supports 4x.
                renderQualityPanel->MutableSettings().msaaSamples = static_cast<int>(desired);
            }
        }

        // Sync overlay & toolbar state.
        overlay->SetFps(smoothedFps);
        overlay->SetCameraPosition(camera.GetPosition());
        overlay->SetCameraForward(camera.GetForward());
        {
            auto [view, proj] = buildViewProj();
            // Always show the measurement layer when there's saved history,
            // even outside measure mode — but only show snap preview while active.
            const bool hasAny = measureModeActive || !measureTool.GetMeasurements().empty();
            overlay->SetMeasurement(hasAny ? &measureTool : nullptr,
                                    view, proj, windowSize());
            overlay->SetSnapCandidate(measureModeActive ? measureSnapPoint : std::nullopt,
                                      measureSnapIsVertex);
        }
        toolbar->SetRenderMode(renderMode);
        {
            // Reflect whether any measurement is currently visible. An empty
            // list counts as "visible" (default-on) so the checkbox stays
            // checked until the user toggles it.
            const auto& items = measureTool.GetMeasurements();
            const bool anyVisible =
                items.empty() || std::any_of(items.begin(), items.end(),
                                             [](const auto& r) { return r.visible; });
            toolbar->SetMeasurementsVisible(anyVisible);
        }

        // 7.10d — First-person minimal UI. On entering FPS we snapshot every
        // main panel's current visibility and hide it; on exit we restore.
        // Only the first-person exit overlay is shown while FPS is active.
        {
            static bool wasFirstPersonActive = false;
            static std::vector<std::pair<bimeup::ui::Panel*, bool>> savedVisibility;
            bimeup::ui::Panel* mainPanels[] = {
                toolbar,            hierarchyPanel,   propertyPanel,
                overlay,            measurementsPanel, renderQualityPanel,
                axisSectionPanel,   planViewPanel,    typeVisibilityPanel,
            };
            if (firstPersonActive && !wasFirstPersonActive) {
                savedVisibility.clear();
                for (auto* p : mainPanels) {
                    savedVisibility.emplace_back(p, p->IsVisible());
                    p->SetVisible(false);
                }
                firstPersonExitPanel->Open();
            } else if (!firstPersonActive && wasFirstPersonActive) {
                for (auto& [p, v] : savedVisibility) {
                    p->SetVisible(v);
                }
                savedVisibility.clear();
                firstPersonExitPanel->Close();
            }
            wasFirstPersonActive = firstPersonActive;
        }

        // ImGuizmo expects GL-convention NDC (Y up); Camera::GetProjectionMatrix
        // pre-flips Y for Vulkan, so undo that before handing it to the gizmo code.
        glm::mat4 gizmoProj = camera.GetProjectionMatrix();
        gizmoProj[1][1] *= -1.0F;
        uiManager.SetCameraMatrices(camera.GetViewMatrix(), gizmoProj);
        axisSectionPanel->SetCameraMatrices(camera.GetViewMatrix(), gizmoProj);
        uiManager.BeginFrame();

        // Orientation gizmo (imoguizmo): top-right square. Click an axis
        // marker to snap the view along that axis; pivotDistance equals the
        // orbit distance so the pivot stays on the current target.
        // axisLengthScale tuned for our 45° fov — the default 0.33 assumes a
        // wider fov and pushes axis endpoints outside the rect.
        // Hidden during first-person mode so the exit button owns the corner.
        if (!firstPersonActive) {
            const auto ws = windowSize();
            constexpr float kCubeSize = 96.0F;
            ImOGuizmo::config.axisLengthScale = 0.15F;
            ImOGuizmo::config.positiveRadiusScale = 0.10F;
            ImOGuizmo::config.negativeRadiusScale = 0.07F;
            ImOGuizmo::SetRect(ws.x - kCubeSize - 10.0F, 10.0F, kCubeSize);
            ImOGuizmo::BeginFrame();
            glm::mat4 viewCopy = camera.GetViewMatrix();
            if (ImOGuizmo::DrawGizmo(&viewCopy[0][0], &gizmoProj[0][0],
                                     camera.GetDistance())) {
                glm::vec3 forward{-viewCopy[0][2], -viewCopy[1][2], -viewCopy[2][2]};
                const glm::vec2 yp = bimeup::renderer::YawPitchFromForward(forward);
                camera.SetYawPitch(yp.x, yp.y);
                if (planViewPanel != nullptr && planViewPanel->ActiveLevel() >= 0) {
                    planViewPanel->Deactivate();
                }
            }
        }

        // Update camera UBO. Camera::SetPerspective already applies the Vulkan
        // Y-flip to m_projection[1][1]; do not flip again here.
        ubo.view = camera.GetViewMatrix();
        ubo.projection = camera.GetProjectionMatrix();

        auto* mapped = static_cast<CameraUBO*>(uboBuffer.Map());
        *mapped = ubo;
        uboBuffer.Unmap();

        // Resolve shadow settings: compute light-space matrix from current scene
        // bounds + key light direction, and (re)build the shadow map if resolution
        // or enabled-state changed.
        auto& shadowSettings = renderQualityPanel->MutableSettings().lighting.shadow;
        {
            auto sceneBounds = ComputeSceneBounds(sceneResult->scene);
            if (sceneBounds.IsValid()) {
                glm::vec3 size = sceneBounds.GetSize();
                float radius = 0.5F * glm::length(size);
                shadowSettings.lightSpaceMatrix = bimeup::renderer::ComputeLightSpaceMatrix(
                    renderQualityPanel->GetSettings().lighting.key.direction,
                    sceneBounds.GetCenter(), std::max(radius, 1.0F));
            }

            if (shadowSettings.enabled &&
                (!shadowMap || shadowMapResolution != shadowSettings.mapResolution)) {
                renderLoop.WaitIdle();
                shadowMap = std::make_unique<bimeup::renderer::ShadowMap>(
                    device, shadowSettings.mapResolution);
                shadowMapResolution = shadowSettings.mapResolution;
                buildShadowPipeline();
                descriptorSet.UpdateImage(2, shadowMap->GetImageView(), shadowMap->GetSampler(),
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }

        // Update lighting UBO from the Render Quality panel.
        lightingUbo = bimeup::renderer::PackLighting(renderQualityPanel->GetSettings().lighting);
        auto* lightMapped = static_cast<bimeup::renderer::LightingUbo*>(lightingBuffer.Map());
        *lightMapped = lightingUbo;
        lightingBuffer.Unmap();

        // Push AxisSectionController slot state into the shared ClipPlaneManager
        // before we pack it for the shader — the controller's plane entries carry
        // sectionFill=true so the fill pipeline picks them up.
        axisSectionController.SyncTo(clipPlaneManager);

        // Re-pack clip planes each frame; cheap, and 7.3d will mutate the manager from UI.
        clipPlanesUbo = bimeup::renderer::PackClipPlanes(clipPlaneManager);
        auto* clipMapped = static_cast<bimeup::renderer::ClipPlanesUbo*>(clipPlanesBuffer.Map());
        *clipMapped = clipPlanesUbo;
        clipPlanesBuffer.Unmap();

        // 7.8d.4 — hash-gated rebuild picks up TypeVisibilityPanel slider edits
        // (the panel writes straight to scene; no callback for mutation events).
        rebuildAlphaOverrides();

        if (!renderLoop.BeginFrame()) {
            recreateSwapchain();
            continue;
        }

        VkCommandBuffer cmd = renderLoop.GetCurrentCommandBuffer();
        VkExtent2D extent = swapchain.GetExtent();

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

        const bool shaded = renderMode == bimeup::renderer::RenderMode::Shaded;
        // SectionOnly mode: only the section-fill caps render; shaded +
        // transparent scene passes are skipped entirely.
        const bool sectionOnly = axisSectionController.AnySectionOnly();
        auto& opaquePipeline = shaded ? *shadedPipeline : *wireframePipeline;
        if (!sectionOnly) {
            opaquePipeline.Bind(cmd);
            descriptorSet.Bind(cmd, opaquePipeline.GetLayout());
            meshBuffer.Bind(cmd);
        }

        // Pass 1: opaque. In wireframe mode, everything goes through the wire
        // pipeline (line rasterization has no meaningful alpha bucket).
        auto needsTransparentPass = [&](bimeup::renderer::MeshHandle h) {
            if (h < sceneResult->meshes.size() &&
                sceneResult->meshes[h].IsTransparent()) return true;
            return handlesWithAlphaOverride.count(h) > 0;
        };
        if (!sectionOnly) {
            for (const auto& [handle, transform] : drawCalls) {
                if (shaded && needsTransparentPass(handle)) {
                    continue;
                }
                vkCmdPushConstants(cmd, opaquePipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                                   0, sizeof(glm::mat4), &transform);
                meshBuffer.Draw(cmd, handle);
            }
        }

        // Section-fill caps: only rebuild/draw when at least one plane is
        // active with sectionFill enabled. Rebuild is hash-gated, so it's a
        // no-op when nothing has changed.
        {
            bool anySection = false;
            for (const auto& entry : clipPlaneManager.Planes()) {
                if (entry.plane.enabled && entry.plane.sectionFill) {
                    anySection = true;
                    break;
                }
            }
            if (anySection) {
                sectionCapGeometry.Rebuild(sceneResult->scene, sceneResult->meshes,
                                           clipPlaneManager);
                if (!sectionCapGeometry.IsEmpty()) {
                    sectionFillPipeline->Bind(cmd);
                    descriptorSet.Bind(cmd, sectionFillPipeline->GetLayout());
                    VkBuffer vb = sectionCapGeometry.GetVertexBuffer();
                    VkDeviceSize offset = 0;
                    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
                    vkCmdDraw(cmd, sectionCapGeometry.GetVertexCount(), 1, 0, 0);
                }
            }
        }

        // Hover disk preview — drawn while PoV is armed and the cursor is over
        // an IfcSlab top face (state set by the mouse-move handler). Lift the
        // disk ~2mm along the slab normal so it doesn't z-fight the slab.
        if (hoverDiskValid) {
            const glm::vec3 liftedCenter = hoverDiskCenter + hoverDiskNormal * 0.002F;
            diskMarkerBuffer.Rebuild(liftedCenter, hoverDiskNormal, kHoverDiskRadius,
                                     kHoverDiskColor);
            if (!diskMarkerBuffer.IsEmpty()) {
                diskMarkerPipeline->Bind(cmd);
                descriptorSet.Bind(cmd, diskMarkerPipeline->GetLayout());
                VkBuffer vb = diskMarkerBuffer.GetVertexBuffer();
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
                vkCmdDraw(cmd, diskMarkerBuffer.GetVertexCount(), 1, 0, 0);
            }
        }

        // Pass 2: transparent (shaded mode only). Depth-test on, write off — so
        // blended fragments respect opaque depth but don't occlude each other.
        // No back-to-front sort yet; acceptable for glass panes that rarely overlap.
        if (shaded && !sectionOnly) {
            transparentPipeline->Bind(cmd);
            descriptorSet.Bind(cmd, transparentPipeline->GetLayout());
            meshBuffer.Bind(cmd);
            for (const auto& [handle, transform] : drawCalls) {
                if (!needsTransparentPass(handle)) {
                    continue;
                }
                vkCmdPushConstants(cmd, transparentPipeline->GetLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
                meshBuffer.Draw(cmd, handle);
            }
        }

        uiManager.EndFrame(cmd);

        if (!renderLoop.EndFrame()) {
            recreateSwapchain();
        }
    }

    renderLoop.WaitIdle();
    selectionBridge.reset();  // unsubscribe before uiManager destroys the panel
    uiManager.ShutdownVulkanBackend();

    // Cleanup happens via destructors in reverse declaration order:
    // renderLoop → swapchain → device → surfaceGuard (destroys surface) →
    // vulkanContext → window → glfwGuard (terminates GLFW last).

    LOG_INFO("Bimeup shutting down");
    bimeup::tools::Log::Shutdown();

    return 0;
}
