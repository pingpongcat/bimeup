#include <vulkan/vulkan.h>

#include <core/EventBus.h>
#include <core/Events.h>
#include <core/Picking.h>
#include <core/SceneUploader.h>
#include <core/Selection.h>
#include <ifc/IfcHierarchy.h>
#include <ifc/IfcModel.h>
#include <platform/Input.h>
#include <platform/Window.h>
#include <renderer/Buffer.h>
#include <renderer/Camera.h>
#include <renderer/MeshBuffer.h>
#include <renderer/DescriptorSet.h>
#include <renderer/Device.h>
#include <renderer/Pipeline.h>
#include <renderer/RenderLoop.h>
#include <renderer/RenderMode.h>
#include <renderer/Shader.h>
#include <renderer/Swapchain.h>
#include <renderer/VulkanContext.h>
#include <scene/AABB.h>
#include <scene/Measurement.h>
#include <scene/Raycast.h>
#include <scene/Scene.h>
#include <scene/SceneBuilder.h>
#include <scene/SceneNode.h>
#include <tools/Log.h>
#include <ui/HierarchyPanel.h>
#include <ui/PropertyPanel.h>
#include <ui/SelectionBridge.h>
#include <ui/Theme.h>
#include <ui/Toolbar.h>
#include <ui/UIManager.h>
#include <ui/ViewportOverlay.h>

#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>
#include <span>
#include <string>
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
    glm::vec3 size = bounds.GetSize();
    float maxDim = std::max({size.x, size.y, size.z});
    camera.SetOrbitTarget(bounds.GetCenter());
    camera.SetDistance(std::max(maxDim * 1.5F, 0.5F));
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

    // Platform
    bimeup::platform::Window::InitGlfw();
    bimeup::platform::Window window({.width = 1280, .height = 720, .title = "Bimeup"});
    bimeup::platform::Input input(window);

    // Vulkan context
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::span<const char* const> requiredExts(glfwExts, glfwExtCount);
    bimeup::renderer::VulkanContext vulkanContext(true, requiredExts);

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
    bimeup::renderer::RenderLoop renderLoop(device, swapchain);

    // Shaders
    std::string shaderDir = BIMEUP_SHADER_DIR;
    bimeup::renderer::Shader vertShader(device, bimeup::renderer::ShaderStage::Vertex,
                                        shaderDir + "/basic.vert.spv");
    bimeup::renderer::Shader fragShader(device, bimeup::renderer::ShaderStage::Fragment,
                                        shaderDir + "/basic.frag.spv");

    // Descriptor set (camera UBO)
    bimeup::renderer::DescriptorSetLayout dsLayout(device, {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
    });
    bimeup::renderer::DescriptorPool dsPool(device, 1, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    });
    bimeup::renderer::DescriptorSet descriptorSet(device, dsPool, dsLayout);

    CameraUBO ubo{};
    bimeup::renderer::Buffer uboBuffer(device, bimeup::renderer::BufferType::Uniform,
                                       sizeof(CameraUBO), &ubo);
    descriptorSet.UpdateBuffer(0, uboBuffer);

    // MeshBuffer for all geometry
    bimeup::renderer::MeshBuffer meshBuffer(device);

    // Load IFC file (use CLI argument if given, otherwise the bundled sample).
    std::string ifcPath = (argc > 1) ? std::string(argv[1]) : std::string(BIMEUP_SAMPLE_IFC);
    LOG_INFO("Loading IFC file: {}", ifcPath);

    std::optional<bimeup::scene::BuildResult> sceneResult;
    bimeup::ifc::IfcModel ifcModel;

    if (!ifcModel.LoadFromFile(ifcPath)) {
        LOG_ERROR("Failed to load IFC file: {}", ifcPath);
        return 1;
    }
    LOG_INFO("IFC loaded: {} elements", ifcModel.GetElementCount());

    bimeup::scene::SceneBuilder builder;
    builder.SetBatchingEnabled(true);
    sceneResult = builder.Build(ifcModel);

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

    pipelineConfig.polygonMode = bimeup::renderer::GetPolygonMode(bimeup::renderer::RenderMode::Shaded);
    bimeup::renderer::Pipeline shadedPipeline(device, vertShader, fragShader, pipelineConfig);

    pipelineConfig.polygonMode = bimeup::renderer::GetPolygonMode(bimeup::renderer::RenderMode::Wireframe);
    bimeup::renderer::Pipeline wireframePipeline(device, vertShader, fragShader, pipelineConfig);

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

    auto hierarchy = std::make_unique<bimeup::ifc::IfcHierarchy>(ifcModel);

    // Input: orbit camera with mouse
    bool rightMouseDown = false;
    bool middleMouseDown = false;
    glm::dvec2 lastMousePos{0.0, 0.0};

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

    input.OnMouseButton([&](bimeup::platform::MouseButton btn, bool pressed) {
        if (btn == bimeup::platform::MouseButton::Right) {
            rightMouseDown = pressed;
            lastMousePos = input.GetMousePosition();
        }
        if (btn == bimeup::platform::MouseButton::Middle) {
            middleMouseDown = pressed;
            lastMousePos = input.GetMousePosition();
        }
        if (btn == bimeup::platform::MouseButton::Left && pressed && !imguiWantsMouse()) {
            auto mouse = input.GetMousePosition();
            auto [view, proj] = buildViewProj();
            const glm::vec2 sp(static_cast<float>(mouse.x), static_cast<float>(mouse.y));
            if (measureModeActive) {
                auto ray = bimeup::core::ScreenPointToRay(sp, windowSize(), view, proj);
                if (auto hit = bimeup::scene::RaycastScene(ray, sceneResult->scene,
                                                          sceneResult->meshes)) {
                    measureTool.AddPoint(hit->point);
                    if (auto& res = measureTool.GetResult(); res.has_value()) {
                        LOG_INFO("Measure: {:.3f} m  ({:.2f},{:.2f},{:.2f}) → "
                                 "({:.2f},{:.2f},{:.2f})",
                                 res->distance, res->pointA.x, res->pointA.y, res->pointA.z,
                                 res->pointB.x, res->pointB.y, res->pointB.z);
                    }
                }
            } else {
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

        if (rightMouseDown) {
            camera.Orbit(static_cast<float>(delta.x) * 0.005F,
                         static_cast<float>(delta.y) * 0.005F);
        }
        if (middleMouseDown) {
            camera.Pan(glm::vec2(static_cast<float>(-delta.x) * 0.005F,
                                 static_cast<float>(delta.y) * 0.005F));
        }

        if (!rightMouseDown && !middleMouseDown && !imguiWantsMouse()) {
            auto [view, proj] = buildViewProj();
            bimeup::core::HoverElement(
                glm::vec2(static_cast<float>(x), static_cast<float>(y)),
                windowSize(), view, proj, sceneResult->scene, sceneResult->meshes, eventBus);
        }
    });

    input.OnScroll([&](double /*xOffset*/, double yOffset) {
        camera.Zoom(static_cast<float>(-yOffset) * 0.5F);
    });

    input.OnKey([&](bimeup::platform::Key key, bool pressed) {
        if (key == bimeup::platform::Key::Escape && pressed) {
            glfwSetWindowShouldClose(window.GetHandle(), GLFW_TRUE);
        }
        if (key == bimeup::platform::Key::W && pressed) {
            renderMode = (renderMode == bimeup::renderer::RenderMode::Shaded)
                             ? bimeup::renderer::RenderMode::Wireframe
                             : bimeup::renderer::RenderMode::Shaded;
            LOG_INFO("Render mode: {}",
                     renderMode == bimeup::renderer::RenderMode::Shaded ? "Shaded" : "Wireframe");
        }
    });

    renderLoop.SetClearColor(0.15F, 0.15F, 0.18F);

    // UI
    bimeup::ui::UIManager uiManager;
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
    });

    bimeup::ui::Theme::Apply();

    // Construct panels and keep raw pointers so we can update them each frame.
    auto toolbarOwned = std::make_unique<bimeup::ui::Toolbar>();
    auto hierarchyOwned = std::make_unique<bimeup::ui::HierarchyPanel>();
    auto propertyOwned = std::make_unique<bimeup::ui::PropertyPanel>();
    auto overlayOwned = std::make_unique<bimeup::ui::ViewportOverlay>();

    auto* toolbar = toolbarOwned.get();
    auto* hierarchyPanel = hierarchyOwned.get();
    auto* propertyPanel = propertyOwned.get();
    auto* overlay = overlayOwned.get();

    hierarchyPanel->SetEventBus(&eventBus);
    toolbar->SetRenderMode(renderMode);

    hierarchyPanel->SetRoot(&hierarchy->GetRoot());
    auto selectionBridge = std::make_unique<bimeup::ui::SelectionBridge>(
        eventBus, *propertyPanel,
        [&ifcModel](uint32_t expressId) { return ifcModel.GetElement(expressId); });

    bool fitToViewRequested = false;

    uiManager.AddPanel(std::move(toolbarOwned));
    uiManager.AddPanel(std::move(hierarchyOwned));
    uiManager.AddPanel(std::move(propertyOwned));
    uiManager.AddPanel(std::move(overlayOwned));

    toolbar->SetOnRenderModeChanged([&](bimeup::renderer::RenderMode mode) {
        renderMode = mode;
        LOG_INFO("Render mode: {}",
                 renderMode == bimeup::renderer::RenderMode::Shaded ? "Shaded" : "Wireframe");
    });
    toolbar->SetOnFitToView([&] { fitToViewRequested = true; });
    toolbar->SetOnOpenFile([] { LOG_INFO("Toolbar: Open File (not implemented yet)"); });
    toolbar->SetOnMeasureModeChanged([&](bool active) {
        measureModeActive = active;
        measureTool.Reset();
        LOG_INFO("Measure mode: {}", active ? "on" : "off");
    });

    // FPS tracking
    double lastFrameTime = glfwGetTime();
    float smoothedFps = 0.0F;

    // Main loop
    while (!window.ShouldClose()) {
        window.PollEvents();

        double now = glfwGetTime();
        double dt = now - lastFrameTime;
        lastFrameTime = now;
        if (dt > 0.0) {
            float instantFps = static_cast<float>(1.0 / dt);
            smoothedFps = smoothedFps == 0.0F ? instantFps : (smoothedFps * 0.9F + instantFps * 0.1F);
        }

        // Fit-to-view request from toolbar: recenter + reset zoom to fit scene bounds.
        if (fitToViewRequested) {
            fitToViewRequested = false;
            FitCameraToBounds(camera, ComputeSceneBounds(sceneResult->scene));
        }

        // Sync overlay & toolbar state.
        overlay->SetFps(smoothedFps);
        overlay->SetCameraPosition(camera.GetPosition());
        overlay->SetCameraForward(camera.GetForward());
        {
            auto [view, proj] = buildViewProj();
            overlay->SetMeasurement(measureModeActive ? &measureTool : nullptr,
                                    view, proj, windowSize());
        }
        toolbar->SetRenderMode(renderMode);

        uiManager.BeginFrame();

        // Update camera UBO. Camera::SetPerspective already applies the Vulkan
        // Y-flip to m_projection[1][1]; do not flip again here.
        ubo.view = camera.GetViewMatrix();
        ubo.projection = camera.GetProjectionMatrix();

        auto* mapped = static_cast<CameraUBO*>(uboBuffer.Map());
        *mapped = ubo;
        uboBuffer.Unmap();

        if (!renderLoop.BeginFrame()) {
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

        auto& activePipeline = (renderMode == bimeup::renderer::RenderMode::Shaded)
                                    ? shadedPipeline
                                    : wireframePipeline;
        activePipeline.Bind(cmd);
        descriptorSet.Bind(cmd, activePipeline.GetLayout());

        meshBuffer.Bind(cmd);

        for (const auto& [handle, transform] : drawCalls) {
            vkCmdPushConstants(cmd, activePipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(glm::mat4), &transform);
            meshBuffer.Draw(cmd, handle);
        }

        uiManager.EndFrame(cmd);

        (void)renderLoop.EndFrame();
    }

    renderLoop.WaitIdle();
    selectionBridge.reset();  // unsubscribe before uiManager destroys the panel
    uiManager.ShutdownVulkanBackend();

    // Cleanup happens via destructors in reverse declaration order:
    // renderLoop → swapchain → device → surfaceGuard (destroys surface) → vulkanContext.

    bimeup::platform::Window::TerminateGlfw();

    LOG_INFO("Bimeup shutting down");
    bimeup::tools::Log::Shutdown();

    return 0;
}
