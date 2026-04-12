#include <vulkan/vulkan.h>

#include <core/EventBus.h>
#include <core/Events.h>
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

    // Load IFC file or fall back to cube
    std::optional<bimeup::scene::BuildResult> sceneResult;
    bimeup::ifc::IfcModel ifcModel;
    bool hasScene = false;

    if (argc > 1) {
        std::string ifcPath = argv[1];
        LOG_INFO("Loading IFC file: {}", ifcPath);

        if (ifcModel.LoadFromFile(ifcPath)) {
            LOG_INFO("IFC loaded: {} elements", ifcModel.GetElementCount());

            bimeup::scene::SceneBuilder builder;
            builder.SetBatchingEnabled(true);
            sceneResult = builder.Build(ifcModel);
            bimeup::core::SceneUploader::Upload(*sceneResult, meshBuffer);
            hasScene = true;

            LOG_INFO("Scene built: {} nodes, {} meshes uploaded",
                     sceneResult->scene.GetNodeCount(), meshBuffer.MeshCount());
        } else {
            LOG_ERROR("Failed to load IFC file: {}", ifcPath);
        }
    }

    // If no IFC loaded, upload a fallback cube
    bimeup::renderer::MeshHandle cubeHandle = bimeup::renderer::MeshBuffer::InvalidHandle;
    if (!hasScene) {
        LOG_INFO("No IFC file loaded — showing demo cube");
        bimeup::renderer::MeshData cubeMeshData;
        MakeCube(cubeMeshData.vertices, cubeMeshData.indices);
        cubeHandle = meshBuffer.Upload(cubeMeshData);
    }

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

    if (hasScene) {
        auto bounds = ComputeSceneBounds(sceneResult->scene);
        if (bounds.IsValid()) {
            glm::vec3 center = bounds.GetCenter();
            glm::vec3 size = bounds.GetSize();
            float maxDim = std::max({size.x, size.y, size.z});
            camera.SetOrbitTarget(center);
            // Zoom out from default distance (5.0) to fit the model
            float targetDistance = maxDim * 1.5F;
            camera.Zoom(targetDistance - 5.0F);
            LOG_INFO("Scene bounds: center=({:.1f},{:.1f},{:.1f}) size=({:.1f},{:.1f},{:.1f})",
                     center.x, center.y, center.z, size.x, size.y, size.z);
        }
    } else {
        camera.SetOrbitTarget(glm::vec3(0.0F));
    }

    // Collect draw calls for the scene
    std::vector<std::pair<bimeup::renderer::MeshHandle, glm::mat4>> drawCalls;
    if (hasScene) {
        drawCalls = CollectDrawCalls(sceneResult->scene);
        LOG_INFO("Draw calls: {}", drawCalls.size());
    }

    // Input: orbit camera with mouse
    bool rightMouseDown = false;
    bool middleMouseDown = false;
    glm::dvec2 lastMousePos{0.0, 0.0};

    input.OnMouseButton([&](bimeup::platform::MouseButton btn, bool pressed) {
        if (btn == bimeup::platform::MouseButton::Right) {
            rightMouseDown = pressed;
            lastMousePos = input.GetMousePosition();
        }
        if (btn == bimeup::platform::MouseButton::Middle) {
            middleMouseDown = pressed;
            lastMousePos = input.GetMousePosition();
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

    // Event bus & selection
    bimeup::core::EventBus eventBus;
    bimeup::core::Selection selection(eventBus);

    // IFC hierarchy (only when a model was loaded)
    std::unique_ptr<bimeup::ifc::IfcHierarchy> hierarchy;
    if (hasScene) {
        hierarchy = std::make_unique<bimeup::ifc::IfcHierarchy>(ifcModel);
    }

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

    // Bridge selection events → property panel (only meaningful with a model).
    std::unique_ptr<bimeup::ui::SelectionBridge> selectionBridge;
    if (hierarchy) {
        hierarchyPanel->SetRoot(&hierarchy->GetRoot());
        selectionBridge = std::make_unique<bimeup::ui::SelectionBridge>(
            eventBus, *propertyPanel,
            [&ifcModel](uint32_t expressId) { return ifcModel.GetElement(expressId); });
    }

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

        // Fit-to-view request from toolbar: recenter on scene bounds (or origin for cube).
        if (fitToViewRequested) {
            fitToViewRequested = false;
            if (hasScene) {
                auto bounds = ComputeSceneBounds(sceneResult->scene);
                if (bounds.IsValid()) {
                    camera.SetOrbitTarget(bounds.GetCenter());
                }
            } else {
                camera.SetOrbitTarget(glm::vec3(0.0F));
            }
        }

        // Sync overlay & toolbar state.
        overlay->SetFps(smoothedFps);
        overlay->SetCameraPosition(camera.GetPosition());
        overlay->SetCameraForward(camera.GetForward());
        toolbar->SetRenderMode(renderMode);

        uiManager.BeginFrame();

        // Update camera UBO
        ubo.view = camera.GetViewMatrix();
        ubo.projection = camera.GetProjectionMatrix();
        ubo.projection[1][1] *= -1;  // Vulkan Y-flip

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

        if (hasScene) {
            // Draw all scene meshes
            for (const auto& [handle, transform] : drawCalls) {
                vkCmdPushConstants(cmd, activePipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                                   0, sizeof(glm::mat4), &transform);
                meshBuffer.Draw(cmd, handle);
            }
        } else {
            // Draw fallback cube
            glm::mat4 model(1.0F);
            vkCmdPushConstants(cmd, activePipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(glm::mat4), &model);
            meshBuffer.Draw(cmd, cubeHandle);
        }

        uiManager.EndFrame(cmd);

        (void)renderLoop.EndFrame();
    }

    renderLoop.WaitIdle();
    selectionBridge.reset();  // unsubscribe before uiManager destroys the panel
    uiManager.ShutdownVulkanBackend();

    // Cleanup happens via destructors in reverse order
    // Explicit surface cleanup needed since it's not wrapped
    vkDestroySurfaceKHR(vulkanContext.GetInstance(), surface, nullptr);

    bimeup::platform::Window::TerminateGlfw();

    LOG_INFO("Bimeup shutting down");
    bimeup::tools::Log::Shutdown();

    return 0;
}
