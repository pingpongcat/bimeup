# Bimeup — Multi-Stage Development Plan

**BIM me up** — Open-source BIM viewer for IFC files
Vulkan + ImGui + OpenXR | Linux + Windows

---

## Architecture Overview

```
bimeup/
├── CMakeLists.txt              # Root CMake
├── cmake/                      # CMake modules & toolchain files
├── external/                   # Git submodules (all dependencies)
├── scripts/                    # Build & CI helper scripts
├── tests/                      # Integration tests (cross-module)
├── assets/                     # Shaders, test IFC files, fonts
│
├── src/
│   ├── tools/                  # Logging, config, math helpers, profiling
│   │   ├── CMakeLists.txt
│   │   ├── include/tools/
│   │   └── src/
│   │
│   ├── platform/               # GLFW windowing, OS abstraction, input
│   │   ├── CMakeLists.txt
│   │   ├── include/platform/
│   │   └── src/
│   │
│   ├── renderer/               # Vulkan renderer, pipeline, ray tracing
│   │   ├── CMakeLists.txt
│   │   ├── include/renderer/
│   │   └── src/
│   │
│   ├── ifc/                    # IFC parsing, scene graph, internal repr
│   │   ├── CMakeLists.txt
│   │   ├── include/ifc/
│   │   └── src/
│   │
│   ├── scene/                  # Internal optimized scene representation
│   │   ├── CMakeLists.txt
│   │   ├── include/scene/
│   │   └── src/
│   │
│   ├── ui/                     # ImGui layer, panels, overlays
│   │   ├── CMakeLists.txt
│   │   ├── include/ui/
│   │   └── src/
│   │
│   ├── vr/                     # OpenXR integration
│   │   ├── CMakeLists.txt
│   │   ├── include/vr/
│   │   └── src/
│   │
│   └── core/                   # Application logic, glue, event bus
│       ├── CMakeLists.txt
│       ├── include/core/
│       └── src/
│
└── app/                        # main.cpp — thin entry point
    └── CMakeLists.txt
```

### Module Dependency Graph

```
tools  ←── everything (logging, config, math)
platform ←── renderer, ui, vr, core
renderer ←── core, vr
ifc ←── core
scene ←── renderer, core, ifc (produces scene from IFC, consumed by renderer)
ui ←── core
vr ←── core
core ←── app (orchestrates everything)
```

### Key Design Principles

1. **Each module is a CMake library** with its own `CMakeLists.txt`, public headers in `include/`, private sources in `src/`, and unit tests in `tests/`.
2. **Modules communicate through interfaces** (abstract classes or typed callbacks), never through concrete implementations of other modules.
3. **The `scene/` module is the bridge** between IFC data and rendering — the renderer never sees IFC types, and the IFC module never sees Vulkan types.
4. **TDD at every step**: write the test, watch it fail, implement, refactor.

---

## External Dependencies

All stored in `external/` as git submodules:

| Library | Purpose | Module |
|---------|---------|--------|
| GLFW | Windowing, input | platform |
| Vulkan-Headers | Vulkan API | renderer |
| VulkanMemoryAllocator (VMA) | GPU memory management | renderer |
| glm | Math (vectors, matrices, quaternions) | tools (re-exported) |
| spdlog | Logging | tools |
| Dear ImGui | GUI | ui |
| IfcOpenShell (or web-ifc C++ core) | IFC parsing | ifc |
| OpenXR-SDK | VR runtime | vr |
| googletest | Testing framework | all |
| glslang / shaderc | Shader compilation | renderer (build-time) |
| stb_image | Texture loading | renderer |
| entt | ECS (optional, evaluated in Stage 5) | scene |

---

## Stage 1 — Project Bootstrap & Build System

**Goal**: Empty project that compiles on Linux and Windows, runs tests, produces a "hello world" log message.

**Sessions**: 1–2

### Modules involved
- `tools/` (logging wrapper)
- Root CMake

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 1.1 | Create root `CMakeLists.txt` with C++20, module structure, option flags (`BIMEUP_BUILD_TESTS`, `BIMEUP_ENABLE_VR`, `BIMEUP_ENABLE_RAYTRACING`) | CMake configures without errors | `CMakeLists.txt` |
| 1.2 | Add git submodules: googletest, spdlog, glm | Submodules clone and build | `external/`, `.gitmodules` |
| 1.3 | Create `tools/` module with `Log` wrapper around spdlog | Unit test: `Log::Init()` succeeds, `LOG_INFO("test")` doesn't crash | `src/tools/` |
| 1.4 | Create `tools/Config` — simple key-value config loader (INI or JSON) | Unit test: parse config string, get values by key, default values | `src/tools/include/tools/Config.h` |
| 1.5 | Create `app/main.cpp` — initializes logging, prints version | Builds and runs, outputs version to stdout | `app/main.cpp` |
| 1.6 | Create build scripts: `scripts/build_debug.sh`, `scripts/build_debug.bat`, `scripts/build_release.sh`, `scripts/build_release.bat` | Scripts run and produce binaries | `scripts/` |
| 1.7 | Set up GitHub Actions CI: Linux + Windows, Debug + Release, run tests | CI passes on push | `.github/workflows/ci.yml` |
| 1.8 | Enable clang-tidy and sanitizers (ASan, UBSan) in Debug | CI runs clang-tidy, sanitizers active in Debug tests | `cmake/Sanitizers.cmake`, `.clang-tidy` |

### Expected APIs after Stage 1

```cpp
// tools/include/tools/Log.h
namespace bimeup::tools {
    class Log {
    public:
        static void Init(const std::string& appName);
        static void Shutdown();
        // Macros: LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL
    };
}

// tools/include/tools/Config.h
namespace bimeup::tools {
    class Config {
    public:
        bool Load(const std::string& path);
        bool LoadFromString(const std::string& content);
        std::string GetString(const std::string& key, const std::string& defaultVal = "") const;
        int GetInt(const std::string& key, int defaultVal = 0) const;
        float GetFloat(const std::string& key, float defaultVal = 0.0f) const;
        bool GetBool(const std::string& key, bool defaultVal = false) const;
    };
}
```

---

## Stage 2 — Platform Layer & Window

**Goal**: Open a GLFW window with Vulkan surface, handle input events, render a cleared screen.

**Sessions**: 2–3

### Modules involved
- `platform/`
- `renderer/` (minimal — instance, device, swapchain, clear)
- `tools/`

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 2.1 | Add GLFW submodule, create `platform/Window` class | Unit test: create window (headless/offscreen where possible), query size, destroy | `src/platform/` |
| 2.2 | Create `platform/Input` — keyboard + mouse abstraction with event callbacks | Unit test: register callback, simulate key event, verify callback fires | `platform/include/platform/Input.h` |
| 2.3 | Create Vulkan instance + debug messenger | Unit test: instance creates successfully, validation layers active in Debug | `src/renderer/VulkanContext.h` |
| 2.4 | Create Vulkan physical/logical device selection | Unit test: device selected, queue families identified | `src/renderer/Device.h` |
| 2.5 | Create swapchain tied to GLFW surface | Integration test: swapchain creates, images acquired | `src/renderer/Swapchain.h` |
| 2.6 | Implement render loop: acquire image → clear to color → present | Visual test: window shows solid color, no validation errors | `src/renderer/RenderLoop.h` |
| 2.7 | Implement frame timing and FPS tracking in `tools/` | Unit test: timer returns monotonically increasing values, FPS computes correctly | `src/tools/include/tools/Timer.h` |

### Expected APIs after Stage 2

```cpp
// platform/include/platform/Window.h
namespace bimeup::platform {
    struct WindowConfig {
        int width = 1280;
        int height = 720;
        std::string title = "Bimeup";
        bool resizable = true;
    };

    class Window {
    public:
        explicit Window(const WindowConfig& config);
        ~Window();
        bool ShouldClose() const;
        void PollEvents();
        glm::ivec2 GetSize() const;
        glm::ivec2 GetFramebufferSize() const;
        GLFWwindow* GetHandle() const;  // internal use only
        VkSurfaceKHR CreateVulkanSurface(VkInstance instance) const;
    };
}

// platform/include/platform/Input.h
namespace bimeup::platform {
    enum class Key { /* ... */ };
    enum class MouseButton { Left, Right, Middle };

    using KeyCallback = std::function<void(Key, bool pressed)>;
    using MouseMoveCallback = std::function<void(double x, double y)>;
    using ScrollCallback = std::function<void(double xOffset, double yOffset)>;

    class Input {
    public:
        explicit Input(Window& window);
        void OnKey(KeyCallback cb);
        void OnMouseMove(MouseMoveCallback cb);
        void OnMouseButton(std::function<void(MouseButton, bool)> cb);
        void OnScroll(ScrollCallback cb);
        bool IsKeyDown(Key key) const;
        glm::dvec2 GetMousePosition() const;
    };
}

// renderer/include/renderer/RenderContext.h
namespace bimeup::renderer {
    class RenderContext {
    public:
        RenderContext(platform::Window& window, bool enableValidation);
        ~RenderContext();
        bool BeginFrame();   // acquire swapchain image
        void EndFrame();     // submit + present
        void WaitIdle();
        VkDevice GetDevice() const;
        VkCommandBuffer GetCurrentCommandBuffer() const;
        uint32_t GetCurrentFrameIndex() const;
    };
}
```

---

## Stage 3 — Basic Rendering Pipeline

**Goal**: Render a hardcoded triangle/cube using a Vulkan graphics pipeline. Camera orbit controls.

**Sessions**: 2–3

### Modules involved
- `renderer/` (pipeline, shaders, buffers, camera)
- `platform/` (input → camera)
- `tools/` (math helpers)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 3.1 | Implement `renderer/Buffer` — vertex + index buffer abstraction with VMA | Unit test: create buffer, upload data, verify size | `src/renderer/Buffer.h` |
| 3.2 | Implement `renderer/Shader` — load SPIR-V, create shader modules | Unit test: load valid SPIR-V binary, create module | `src/renderer/Shader.h` |
| 3.3 | Implement `renderer/Pipeline` — graphics pipeline creation | Unit test: create pipeline with vertex + fragment shader, verify handle valid | `src/renderer/Pipeline.h` |
| 3.4 | Implement `renderer/DescriptorSet` — UBO binding for camera matrices | Unit test: allocate descriptor set, update with buffer | `src/renderer/DescriptorSet.h` |
| 3.5 | Implement `renderer/Camera` — perspective projection, orbit controller | Unit test: verify projection matrix, orbit rotation produces expected view matrix | `src/renderer/Camera.h` |
| 3.6 | Write basic vertex + fragment shaders (position + flat color) | Shader compiles to valid SPIR-V | `assets/shaders/basic.vert`, `basic.frag` |
| 3.7 | Render a colored cube with orbit camera | Visual test: cube visible, orbit works, no validation errors | Integration in app/ |
| 3.8 | Implement `renderer/MeshBuffer` — manages GPU mesh data (vertices + indices) for multiple objects | Unit test: upload mesh, retrieve draw parameters | `src/renderer/MeshBuffer.h` |
| 3.9 | Add render mode switching: shaded / wireframe | Unit test: pipeline created with fill and line polygon modes | Pipeline configuration |

### Expected APIs after Stage 3

```cpp
// renderer/include/renderer/Camera.h
namespace bimeup::renderer {
    class Camera {
    public:
        void SetPerspective(float fovDeg, float aspect, float near, float far);
        void SetOrbitTarget(glm::vec3 target);
        void Orbit(float deltaYaw, float deltaPitch);
        void Zoom(float delta);
        void Pan(glm::vec2 delta);
        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix() const;
        glm::vec3 GetPosition() const;
        glm::vec3 GetForward() const;
    };
}

// renderer/include/renderer/Mesh.h
namespace bimeup::renderer {
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec4 color;
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    using MeshHandle = uint32_t;

    class MeshBuffer {
    public:
        MeshHandle Upload(const MeshData& data);
        void Remove(MeshHandle handle);
        void Bind(VkCommandBuffer cmd) const;
        void Draw(VkCommandBuffer cmd, MeshHandle handle) const;
    };
}
```

---

## Stage 4 — IFC Loading & Internal Scene Representation

**Goal**: Load an IFC file, convert it to an internal optimized scene representation, display the geometry in the viewer.

**Sessions**: 3–4 (this is a critical stage)

### Modules involved
- `ifc/` (IFC parsing via IfcOpenShell/web-ifc)
- `scene/` (internal representation)
- `renderer/` (mesh upload)
- `core/` (orchestration)

### Key design decisions

The `ifc/` module wraps an IFC parsing library and produces a **library-agnostic intermediate representation**. The `scene/` module converts this into a flat, GPU-friendly scene graph. The renderer only sees `scene/` types.

```
IFC file → ifc::IfcModel → scene::Scene → renderer (GPU buffers)
```

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 4.1 | Integrate IFC parsing library (IfcOpenShell or web-ifc C++ core) as submodule | Builds and links | `external/` |
| 4.2 | Create `ifc/IfcModel` — wraps parsed IFC data, exposes element iteration | Unit test: load test.ifc, iterate elements, read properties | `src/ifc/include/ifc/IfcModel.h` |
| 4.3 | Create `ifc/IfcElement` — single IFC element with type, name, GUID, properties | Unit test: element has correct type (IfcWall, IfcSlab…), name, globalId | `src/ifc/include/ifc/IfcElement.h` |
| 4.4 | Create `ifc/IfcGeometryExtractor` — extract triangulated mesh data per element | Unit test: extract geometry from IfcWall, verify vertex/index count > 0 | `src/ifc/include/ifc/IfcGeometryExtractor.h` |
| 4.5 | Create `ifc/IfcHierarchy` — extract spatial structure tree (Site→Building→Storey→Elements) | Unit test: hierarchy depth ≥ 3 for test file, element count matches | `src/ifc/include/ifc/IfcHierarchy.h` |
| 4.6 | Create `scene/SceneNode` and `scene/Scene` — flat scene graph with transforms | Unit test: create scene, add nodes, query by ID, parent-child relations | `src/scene/` |
| 4.7 | Create `scene/SceneMesh` — GPU-ready mesh data (positions, normals, colors, indices) | Unit test: create mesh, verify data layout matches renderer expectations | `src/scene/include/scene/SceneMesh.h` |
| 4.8 | Create `scene/AABB` — axis-aligned bounding box per node and for whole scene | Unit test: compute AABB from vertices, merge two AABBs | `src/scene/include/scene/AABB.h` |
| 4.9 | Create `scene/SceneBuilder` — converts `ifc::IfcModel` → `scene::Scene` | Integration test: load IFC → build scene → verify node count, mesh count | `src/scene/include/scene/SceneBuilder.h` |
| 4.10 | Implement batching in SceneBuilder — group small meshes by material/type | Unit test: 100 small elements produce fewer batched draw calls | Batching logic |
| 4.11 | Upload scene meshes to renderer, draw all elements | Visual test: IFC model visible in viewer | Integration in core/ |
| 4.12 | Implement per-element color from IFC material/style | Unit test: element with IfcSurfaceStyle gets correct color | Color extraction in ifc/ |

### Expected APIs after Stage 4

```cpp
// ifc/include/ifc/IfcModel.h
namespace bimeup::ifc {
    class IfcModel {
    public:
        bool LoadFromFile(const std::string& path);
        size_t GetElementCount() const;
        std::vector<IfcElement> GetElements() const;
        std::vector<IfcElement> GetElementsByType(const std::string& ifcType) const;
        IfcElement GetElementByGlobalId(const std::string& guid) const;
        IfcHierarchy GetSpatialHierarchy() const;
    };
}

// ifc/include/ifc/IfcGeometryExtractor.h
namespace bimeup::ifc {
    struct TriangulatedMesh {
        std::vector<glm::dvec3> positions;
        std::vector<glm::dvec3> normals;
        std::vector<uint32_t> indices;
        glm::dvec4 color;           // from IFC style
        glm::dmat4 transformation;  // placement
    };

    class IfcGeometryExtractor {
    public:
        explicit IfcGeometryExtractor(const IfcModel& model);
        std::optional<TriangulatedMesh> ExtractMesh(uint32_t elementId) const;
        std::vector<std::pair<uint32_t, TriangulatedMesh>> ExtractAll() const;
    };
}

// scene/include/scene/Scene.h
namespace bimeup::scene {
    using NodeId = uint32_t;

    struct SceneNode {
        NodeId id;
        std::string name;
        std::string ifcType;
        std::string globalId;
        glm::mat4 transform;
        AABB bounds;
        std::optional<MeshHandle> mesh;
        NodeId parent;
        std::vector<NodeId> children;
        bool visible = true;
        bool selected = false;
    };

    class Scene {
    public:
        NodeId AddNode(SceneNode node);
        SceneNode& GetNode(NodeId id);
        const SceneNode& GetNode(NodeId id) const;
        std::vector<NodeId> GetRoots() const;
        std::vector<NodeId> GetChildren(NodeId id) const;
        AABB GetBounds() const;
        size_t GetNodeCount() const;
        void SetVisibility(NodeId id, bool visible, bool recursive = false);
        void SetSelected(NodeId id, bool selected);
        std::vector<NodeId> GetSelected() const;
        std::vector<NodeId> FindByType(const std::string& ifcType) const;
    };
}
```

---

## Stage 5 — Core Application & Element Selection

**Goal**: Wire up the application loop, implement raycasting for element selection, element highlighting.

**Sessions**: 2–3

### Modules involved
- `core/` (application, event bus)
- `renderer/` (picking, highlighting)
- `scene/` (selection state)
- `platform/` (mouse input)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 5.1 | Create `core/Application` — owns all modules, runs main loop | Integration test: app starts, runs 1 frame, shuts down cleanly | `src/core/include/core/Application.h` |
| 5.2 | Create `core/EventBus` — typed publish/subscribe event system | Unit test: subscribe to event, publish, callback fires with correct data | `src/core/include/core/EventBus.h` |
| 5.3 | Define core events: `ElementSelected`, `ElementHovered`, `ModelLoaded`, `ViewChanged` | Unit test: construct each event, verify fields | `src/core/include/core/Events.h` |
| 5.4 | Implement CPU raycasting against scene AABBs, then triangle-level refinement | Unit test: ray hits known AABB, misses empty space; ray hits correct triangle | `src/scene/include/scene/Raycast.h` |
| 5.5 | Implement mouse click → ray → element selection pipeline | Integration test: click on element → `ElementSelected` event fires with correct ID | Wiring in core/ |
| 5.6 | Implement element highlighting in renderer (outline or color override) | Visual test: selected element visually distinct | `src/renderer/Highlight.h` |
| 5.7 | Implement hover highlighting (mouse move → raycast → highlight) | Visual test: element under cursor highlights on hover | Wiring in core/ |
| 5.8 | Implement multi-selection (Ctrl+click) and selection clearing (Escape) | Unit test: scene tracks multiple selected IDs, clear removes all | Selection logic |

### Expected APIs after Stage 5

```cpp
// core/include/core/EventBus.h
namespace bimeup::core {
    class EventBus {
    public:
        template<typename T>
        using Handler = std::function<void(const T&)>;

        template<typename T>
        uint32_t Subscribe(Handler<T> handler);

        template<typename T>
        void Unsubscribe(uint32_t id);

        template<typename T>
        void Publish(const T& event);
    };
}

// core/include/core/Application.h
namespace bimeup::core {
    struct AppConfig {
        platform::WindowConfig window;
        bool enableVR = false;
        bool enableRayTracing = false;
        std::string configPath;
    };

    class Application {
    public:
        explicit Application(const AppConfig& config);
        ~Application();
        void Run();             // blocking main loop
        void RequestShutdown();
        EventBus& GetEventBus();
        scene::Scene& GetScene();
        void LoadModel(const std::string& path);
    };
}
```

---

## Stage 6 — ImGui Integration & Basic UI

**Goal**: Full ImGui integration with Vulkan backend. Implement property panel, hierarchy tree, toolbar.

**Sessions**: 2–3

### Modules involved
- `ui/`
- `renderer/` (ImGui Vulkan backend)
- `scene/` (data source)
- `core/` (events)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 6.1 | Add Dear ImGui submodule, integrate Vulkan + GLFW backend | Visual test: ImGui demo window renders over scene | `external/imgui` |
| 6.2 | Create `ui/UIManager` — initializes ImGui, manages panels, renders each frame | Unit test (headless): manager creates, adds panel, destroys without crash | `src/ui/include/ui/UIManager.h` |
| 6.3 | Create `ui/Panel` interface — base class for all UI panels | Unit test: panel opens, closes, toggles visibility | `src/ui/include/ui/Panel.h` |
| 6.4 | Implement `ui/HierarchyPanel` — tree view of IFC spatial structure | Unit test: given scene with hierarchy, panel produces correct tree depth | `src/ui/include/ui/HierarchyPanel.h` |
| 6.5 | Implement `ui/PropertyPanel` — shows properties of selected element | Unit test: given selected element with properties, panel displays key-value pairs | `src/ui/include/ui/PropertyPanel.h` |
| 6.6 | Implement `ui/Toolbar` — file open, render mode toggle, view controls | Visual test: buttons work, file dialog opens | `src/ui/include/ui/Toolbar.h` |
| 6.7 | Implement `ui/ViewportOverlay` — FPS counter, camera info, axes gizmo | Visual test: overlays visible in viewport | `src/ui/include/ui/ViewportOverlay.h` |
| 6.8 | Wire hierarchy panel click → element selection → property panel update | Integration test: click tree node → scene selection → property panel shows data | Event wiring |
| 6.9 | Implement UI theme consistent with BIM tooling (professional, high-contrast) | Visual test: readable, no clipping | Theme configuration |

### Expected APIs after Stage 6

```cpp
// ui/include/ui/UIManager.h
namespace bimeup::ui {
    class Panel {
    public:
        virtual ~Panel() = default;
        virtual const char* GetName() const = 0;
        virtual void OnDraw() = 0;
        void SetVisible(bool v);
        bool IsVisible() const;
    };

    class UIManager {
    public:
        UIManager(renderer::RenderContext& ctx, platform::Window& window,
                  core::EventBus& events, scene::Scene& scene);
        ~UIManager();
        void AddPanel(std::unique_ptr<Panel> panel);
        void BeginFrame();
        void EndFrame();    // records ImGui draw commands
        void OnResize(int width, int height);
    };
}
```

---

## Stage 7 — BIM Viewer Features

**Goal**: Implement measurement tools, clipping planes, plan/section views, element visibility toggles.

**Sessions**: 3–4

### Modules involved
- `ui/` (tool panels)
- `renderer/` (clipping planes, section rendering)
- `scene/` (visibility, measurements)
- `core/` (tool state management)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 7.1 | Implement distance measurement tool — click two points, show distance | Unit test: distance between two known 3D points is correct | `src/ui/include/ui/MeasureTool.h` |
| 7.2 | Implement point snapping to vertices/edges/faces | Unit test: snap to nearest vertex within threshold | Snap logic in scene/ |
| 7.3 | Implement clipping planes (up to 6) controlled via UI | Unit test: point classified as in front/behind plane; Visual: clipped geometry | `src/renderer/ClipPlane.h` |
| 7.4 | Implement plan view (top-down orthographic at storey elevation) | Unit test: camera placed at correct position for given storey | `src/ui/include/ui/PlanView.h` |
| 7.5 | Implement section view (cut plane + 2D section fill rendering) | Visual test: clean section cut at specified plane | Section rendering in renderer/ |
| 7.6 | Implement element visibility by IFC type (toggle all walls, slabs, etc.) | Unit test: set type invisible → all elements of that type have visible=false | Visibility in scene/ |
| 7.7 | Implement element isolation (show only selected, hide rest) | Unit test: isolate 3 elements → only those visible | Isolation logic |
| 7.8 | Implement element transparency override | Visual test: transparent elements render correctly with depth sorting | Alpha rendering in renderer/ |
| 7.9 | Implement fit-to-view (zoom to selection or whole model) | Unit test: camera frames AABB with correct distance | Camera logic |
| 7.10 | Implement first-person navigation mode (WASD + mouse look) | Integration test: keys produce expected camera movement | Camera controller variant |

### Expected APIs after Stage 7

```cpp
// scene/include/scene/Measurement.h
namespace bimeup::scene {
    struct MeasureResult {
        glm::vec3 pointA;
        glm::vec3 pointB;
        float distance;
        glm::vec3 deltaXYZ;  // component distances
    };

    glm::vec3 SnapToGeometry(const Scene& scene, glm::vec3 worldPos, float threshold);
    MeasureResult Measure(glm::vec3 a, glm::vec3 b);
}

// renderer/include/renderer/ClipPlane.h
namespace bimeup::renderer {
    class ClipPlaneManager {
    public:
        uint32_t AddPlane(glm::vec4 equation);  // ax + by + cz + d = 0
        void RemovePlane(uint32_t id);
        void SetEnabled(uint32_t id, bool enabled);
        void UpdatePlane(uint32_t id, glm::vec4 equation);
        void Bind(VkCommandBuffer cmd) const;  // push to shader
    };
}
```

---

## Stage R — Render Quality

**Goal**: Lift the renderer from flat/unlit to a production-quality look: three-point lighting, MSAA, shadow mapping for the key light, and SSAO. Expose toggles in a floating ImGui "Render Quality" panel.

**Sessions**: 4 (one per task)

### Modules involved
- `renderer/` (pipeline, attachments, passes, shaders)
- `ui/` (RenderQualityPanel)
- `assets/shaders/` (new/updated GLSL)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| R.1 | Three-point lighting (key/fill/rim) in the main fragment shader, driven by a LightingUBO. Add `ui/RenderQualityPanel` with sliders for direction/intensity/color of each light and a master enable. | Unit test: LightingUBO packing matches shader layout; Lambert term with known normal+light direction matches expected value | `src/renderer/include/renderer/Lighting.h`, `src/ui/include/ui/RenderQualityPanel.h`, updated `assets/shaders/basic.{vert,frag}` |
| R.2 | MSAA with sample count selectable at runtime (1/2/4/8). New multisampled color+depth attachments, resolve into swapchain image. Panel toggle. | Unit test: selected sample count clamped to device max; Visual: no aliasing on cube edges at 4× | Pipeline/Swapchain changes in `renderer/` |
| R.3 | Shadow mapping for key light. Depth-only pass into a shadow map image, light-space matrix in UBO, PCF sampling in main frag. Panel toggle + resolution selector. | Unit test: light-space matrix projects a known world point into expected UV; Visual: ground plane receives cube shadow | `src/renderer/include/renderer/ShadowPass.h`, shadow shaders |
| R.4 | SSAO. Depth+normal prepass (or reuse G-buffer), SSAO pass with hemisphere kernel + noise texture, separable blur, composite into main pass. Panel toggle + radius/bias sliders. | Unit test: hemisphere kernel samples all have z ≥ 0 and length ≤ radius; Visual: contact darkening on corners | `src/renderer/include/renderer/SSAOPass.h`, SSAO shaders |

### Expected APIs after Stage R

```cpp
// renderer/include/renderer/Lighting.h
namespace bimeup::renderer {
    struct DirectionalLight {
        glm::vec3 direction;   // world space, pointing FROM light
        glm::vec3 color;
        float intensity;
        bool enabled;
    };

    struct LightingUBO {
        DirectionalLight key;
        DirectionalLight fill;
        DirectionalLight rim;
        glm::vec3 ambient;
    };
}

// ui/include/ui/RenderQualityPanel.h
namespace bimeup::ui {
    struct RenderQualitySettings {
        renderer::LightingUBO lighting;
        int msaaSamples;       // 1, 2, 4, 8
        bool shadowsEnabled;
        int shadowMapSize;     // 512, 1024, 2048, 4096
        bool ssaoEnabled;
        float ssaoRadius;
        float ssaoBias;
    };

    class RenderQualityPanel : public Panel {
    public:
        void Draw() override;
        const RenderQualitySettings& Settings() const;
    };
}
```

---

## Stage 8 — Loading Responsiveness & Memory

**Goal**: Open complex single-building IFC files (e.g. `Ifc2x3_SampleCastle.ifc` — 3.8k elements, 9k scene nodes) without OOM-killing the process, and keep the UI responsive during the multi-second parse + extract phase. The target use-case is compact buildings, not city-scale scenes — so this stage focuses on **load time, peak memory, and UI responsiveness**, not on draw-call throughput or view-dependent culling.

**Sessions**: 2

### Scope decisions (2026-04-17)

The original Stage 8 included BVH, frustum culling, LOD, and indirect drawing. These were dropped after re-scoping against the actual target (compact houses, not 100k-element campuses):

- **Frustum culling** — a single building is in-frustum almost always; the cull would save nothing.
- **BVH** — the scene-node count (≲10k) makes a linear raycast sub-millisecond; a tree adds complexity for no measurable picking gain. Frustum queries vanish with frustum culling.
- **LOD generation** — IFC house elements (walls/slabs/openings/fixtures) are already low-poly; nobody zooms out far enough for simplified versions to matter.
- **Indirect drawing** — Castle renders in 120 draw calls thanks to type+color batching (Stage 4.10). That's already cheap; indirect drawing matters past a few thousand draws.

If a future use-case introduces large-site or campus models, reopen this list — the dropped tasks aren't *wrong*, they're just not justified for the current target.

### Modules involved
- `ifc/` (async loading)
- `app/` (loading modal, progress wiring)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 8.1 | `ifc::AsyncLoader` — `LoadAsync(path, ProgressCallback)` + `Cancel()` returning `std::future<std::unique_ptr<IfcModel>>`; progress callback invoked from the worker thread, cancellation checked between phases | Unit test: load completes on background thread, progress callback fires monotonically, cancel returns early without crash | `src/ifc/include/ifc/AsyncLoader.h` |
| 8.2 | Loading modal in `app/` — ImGui overlay shown while the future is pending; displays % + current phase string + Cancel button; main render loop ticks (clear → present) so the window stays responsive | Manual test: open Castle, modal appears, progress advances, Cancel aborts the load and returns to empty scene | Loading UI in `src/app/main.cpp` (or new `ui::LoadingModal`) |

**Castle-scale perf work (skip-extraction-by-type, memory audit, thread pool,
benchmarks) was removed from Stage 8 on 2026-04-17.** The prior task list
(8.3–8.6) didn't close the loop on `Ifc2x3_SampleCastle.ifc`. Deferred for
now; when we revisit it we plan to try a different approach rather than
resume the old list.

### Expected APIs after Stage 8

```cpp
// ifc/include/ifc/AsyncLoader.h
namespace bimeup::ifc {
    using ProgressCallback = std::function<void(float percent, std::string_view phase)>;

    class AsyncLoader {
    public:
        std::future<std::unique_ptr<IfcModel>> LoadAsync(
            const std::string& path,
            ProgressCallback onProgress = nullptr);
        void Cancel();
        bool IsCancelled() const;
    };
}
```

### Out of scope (intentionally dropped from original Stage 8)

- BVH, frustum culling, LOD generation, indirect drawing — see "Scope decisions" above.
- Splash-screen-before-window — purely cosmetic; revisit in Stage 11 (Polish & Release) if still wanted after the loading modal lands.

### 8.3 Addendum — Axis Section Mode (added 2026-04-17)

A second-pass UX that combines **7.3 (clipping planes)** and **7.5 (section fill)** into a single BIM-oriented tool. The free-form 6-plane UI from 7.3d is retired in favour of at most **three axis-locked planes** (one per X / Y / Z), each with an explicit mode.

#### Data model

- `scene::AxisSectionController` owns ≤3 slots keyed by axis. Each slot is `{ offset: float, mode: CutFront | CutBack | SectionOnly }`.
- Each frame the controller syncs into the existing `renderer::ClipPlaneManager`:
  - CutFront → plane equation with normal = `+axis`, `d = -offset`
  - CutBack → normal = `−axis`, `d = +offset` (shader's front/back stays the same; just flipped)
  - SectionOnly → same equation as CutFront, **and** the controller sets a "hide scene" flag (`AnySectionOnly()`) that `main.cpp` reads to skip the shaded + transparent draws
- Section fill is implicitly on for every axis slot. The `ClipPlane::fillColor` picker is removed from the user-facing UI (field stays on the struct, defaulted so per-element tint from 7.5k is unchanged).

#### Rendering

- `main.cpp` draw loop:
  - if `controller.AnySectionOnly()` → skip opaque + transparent scene draws, keep section-fill pipeline draw
  - else → draw as today (CutFront/CutBack use the existing shader discard path)
- No new pipelines, shaders, or descriptor sets. `SectionCapGeometry` and `section_fill.{vert,frag}` (7.5g–h) are reused as-is.

#### Gizmo

- `ImGuizmo::OPERATION::TRANSLATE_X/Y/Z` scoped to the active slot's axis — bidirectional by default (drag ±).
- Small directional arrow marker (separate scene-space overlay, not an ImGuizmo widget) indicating which side is "kept" so CutFront vs CutBack reads at a glance.
- Stretch (8.3f): an ImGui popup anchored at the plane origin's screen-space position for in-viewport mode switching — ImGuizmo has no custom-widget hook so this is a separate overlay.

#### Tasks

| # | Task | Test |
|---|------|------|
| 8.3a | `scene::AxisSectionController` + sync into `ClipPlaneManager` | Unit: slot add/remove/replace per axis, CutBack sign flip, `AnySectionOnly`, idempotent sync |
| 8.3b | `ui::AxisSectionPanel` — X/Y/Z toggle + mode radio + offset slider | Unit: toggle adds/removes slot, radio writes mode, slider writes offset |
| 8.3c | Per-axis translate gizmo + direction marker overlay | Unit: gizmo → offset writeback math; marker is manual-verify |
| 8.3d | `main.cpp` draw-loop skip when `AnySectionOnly()` | Manual-verify (UI path) |
| 8.3e | Retire `ui::ClipPlanesPanel` + `ColorEdit4` surface | Compile gate; no functional test |
| 8.3f | Stretch — in-viewport mode-selector popup | Manual-verify |
| 8.3g | Retire ImGuizmo; custom single-header `ui::AxisSectionGizmo` (drag bar + grab + F/S/B segmented switcher + (×) close). View cube swapped to `imoguizmo`. | Unit: `ProjectWorldToScreen` + `AxisDragDelta` math; gizmo is manual-verify |

#### Expected API

```cpp
// scene/include/scene/AxisSectionController.h
namespace bimeup::scene {
    enum class Axis : std::uint8_t { X, Y, Z };
    enum class SectionMode : std::uint8_t { CutFront, CutBack, SectionOnly };

    struct AxisSectionSlot {
        float offset;
        SectionMode mode;
    };

    class AxisSectionController {
    public:
        void SetSlot(Axis axis, AxisSectionSlot slot);
        void ClearSlot(Axis axis);
        [[nodiscard]] std::optional<AxisSectionSlot> GetSlot(Axis axis) const;
        [[nodiscard]] bool AnySectionOnly() const;
        void SyncTo(renderer::ClipPlaneManager& manager);
    };
}
```

**Goal**: Full VR support — stereoscopic rendering, tracked controllers, teleport, object selection, in-VR UI.

**Sessions**: 3–4

### Modules involved
- `vr/`
- `renderer/` (stereo rendering, VR-specific pipelines)
- `ui/` (VR UI panels)
- `core/` (VR session management)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 9.1 | Add OpenXR-SDK submodule, create `vr/VRSystem` — session lifecycle | Unit test: system initializes (or reports no HMD gracefully), creates session, destroys | `src/vr/include/vr/VRSystem.h` |
| 9.2 | Implement `vr/VRSwapchain` — stereoscopic swapchain management | Unit test: swapchain creates with correct eye resolution | `src/vr/include/vr/VRSwapchain.h` |
| 9.3 | Implement stereo rendering — render scene to left + right eye views | Visual test: VR headset shows stereo scene | Stereo pipeline |
| 9.4 | Implement `vr/VRInput` — controller tracking, button state | Unit test: input system reports controller poses, button events | `src/vr/include/vr/VRInput.h` |
| 9.5 | Implement teleport movement — arc ray + trigger to move | Visual test: teleport arc visible, movement works | Teleport in vr/ |
| 9.6 | Implement VR ray interaction — controller ray → element selection | Integration test: ray from controller → hits element → selection event fires | Ray interaction in vr/ |
| 9.7 | Implement VR UI panels — ImGui rendered to texture, placed in 3D space | Visual test: UI panel visible in VR, interactable with controller | VR UI in ui/ |
| 9.8 | Implement VR comfort features — vignette on movement, snap turning | Configuration options in config | Comfort settings |
| 9.9 | Implement scale model — grab + scale gesture to resize entire scene | Visual test: pinch gesture scales model | Scale interaction |

### Expected APIs after Stage 9

```cpp
// vr/include/vr/VRSystem.h
namespace bimeup::vr {
    enum class Hand { Left, Right };

    struct ControllerState {
        glm::mat4 pose;
        bool triggerPressed;
        float triggerValue;
        bool gripPressed;
        glm::vec2 thumbstick;
        bool thumbstickClicked;
    };

    class VRSystem {
    public:
        bool Init();
        void Shutdown();
        bool IsAvailable() const;
        bool BeginFrame();
        void EndFrame();
        glm::mat4 GetHeadPose() const;
        ControllerState GetController(Hand hand) const;
        glm::mat4 GetEyeView(uint32_t eye) const;
        glm::mat4 GetEyeProjection(uint32_t eye, float near, float far) const;
        VkImage GetEyeImage(uint32_t eye) const;
        glm::uvec2 GetEyeResolution() const;
    };
}
```

---

## Stage 10 — Ray Tracing (Optional Advanced Rendering)

**Goal**: Vulkan ray tracing pipeline for high-quality rendering mode. Ambient occlusion, reflections, soft shadows.

**Sessions**: 2–3

### Modules involved
- `renderer/` (RT pipeline, acceleration structures)
- `scene/` (BLAS/TLAS management)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 10.1 | Check RT support at runtime, create acceleration structures (BLAS per mesh) | Unit test: AS builds for test mesh, handle valid | `src/renderer/AccelerationStructure.h` |
| 10.2 | Build TLAS from scene instances | Unit test: TLAS contains correct number of instances | TLAS management |
| 10.3 | Create ray tracing pipeline (ray generation, closest hit, miss shaders) | Unit test: pipeline creates, shader binding table valid | RT pipeline in renderer/ |
| 10.4 | Implement RT ambient occlusion | Visual test: AO darkens corners and crevices | AO shader |
| 10.5 | Implement RT soft shadows (single directional light) | Visual test: soft shadow edges | Shadow shader |
| 10.6 | Implement RT reflections on glossy surfaces | Visual test: reflective materials show reflections | Reflection shader |
| 10.7 | Implement hybrid rendering — rasterize primary, RT for AO/shadows | Benchmark: acceptable frame time with RT effects | Hybrid pipeline |
| 10.8 | Toggle between rasterized and RT modes via UI | UI button switches mode, scene re-renders | Mode switching |

---

## Stage 11 — Polish, Packaging & Release

**Goal**: Production-ready application with installers, documentation, and final polish.

**Sessions**: 2–3

### Modules involved
- All modules (final integration)
- CI/CD (packaging)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 11.1 | Implement drag-and-drop IFC file loading | Integration test: drop file → model loads | Platform integration |
| 11.2 | Implement recent files list in UI | Unit test: files added, persisted to config, loaded on restart | UI feature |
| 11.3 | Implement user preferences (theme, render settings, VR settings) | Unit test: settings save/load round-trip | Settings in tools/ |
| 11.4 | Create Linux AppImage packaging in CI | CI produces downloadable AppImage | `.github/workflows/`, CMake install rules |
| 11.5 | Create Windows NSIS installer in CI | CI produces downloadable .exe installer | NSIS script |
| 11.6 | Write user-facing README with screenshots, build instructions | README exists and is accurate | `README.md` |
| 11.7 | Run full integration test suite on both platforms | All tests pass on Linux + Windows CI | Test results |
| 11.8 | Performance profiling pass — identify and fix top 3 bottlenecks | Benchmark before/after shows improvement | Profiling results |
| 11.9 | Accessibility review — keyboard navigation, screen reader hints | All panels navigable by keyboard | UI improvements |

---

## Stage Dependency Graph

```
Stage 1: Bootstrap & Build
    │
    ▼
Stage 2: Platform & Window
    │
    ▼
Stage 3: Basic Rendering ──────────────────┐
    │                                       │
    ▼                                       │
Stage 4: IFC Loading & Scene ──┐            │
    │                          │            │
    ▼                          ▼            │
Stage 5: Core App & Selection  │            │
    │                          │            │
    ├───────────┐              │            │
    ▼           ▼              │            │
Stage 6: UI    Stage 9: VR ◄──┘            │
    │           │                           │
    ▼           │                           │
Stage 7: BIM   │            Stage 10: RT ◄──┘
Features       │            (optional)
    │           │               │
    ▼           │               │
Stage 8:       │               │
Performance    │               │
    │           │               │
    ▼           ▼               ▼
    └───────────┴───────────────┘
                │
                ▼
         Stage 11: Polish & Release
```

**Critical path**: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 11
**Parallel tracks**: VR (Stage 9) can start after Stage 4+5. RT (Stage 10) can start after Stage 3.

---

## Session Planning Rules

Each coding session should:

1. **Start by reading only the module(s) being modified** — never require understanding the full codebase.
2. **Begin with tests** — write failing tests for the task, then implement.
3. **End with all tests passing** — both new and existing.
4. **Touch at most 2 modules** — if a task spans more, split it.
5. **Produce a compilable, runnable state** — no half-finished features left in main.

### Session template

```
1. Read PLAN.md to identify next task
2. Read only the relevant module headers/APIs
3. Write tests for the task
4. Implement the minimal code to pass tests
5. Refactor if needed
6. Run full test suite
7. Commit with descriptive message
```

---

## Test Strategy

| Level | Location | Framework | What it tests |
|-------|----------|-----------|---------------|
| Unit | `src/<module>/tests/` | GoogleTest | Single class/function in isolation |
| Integration | `tests/` | GoogleTest | Cross-module interactions |
| Visual | `tests/visual/` | Manual / screenshot diff | Rendering correctness |
| Performance | `tests/benchmark/` | Google Benchmark | Frame time, load time, memory |

### Test naming convention
```
TEST(ModuleName_ClassName, MethodName_Scenario_Expected)
// Example:
TEST(Scene_BVH, QueryFrustum_AllNodesInside_ReturnsAll)
TEST(Ifc_IfcModel, LoadFromFile_ValidIfc_ReturnsTrue)
TEST(Renderer_Camera, Orbit_PositiveDelta_RotatesRight)
```

### What is NOT tested
- Private implementation details
- GLFW/Vulkan driver behavior
- Third-party library internals

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| IfcOpenShell C++ API is hard to integrate | Stage 4 delayed | Alternative: use web-ifc C++ core directly (already studied). Fallback: STEP parser + manual geometry |
| Vulkan ray tracing not available on target GPU | Stage 10 blocked | RT is optional. Detect at runtime, fall back to rasterization |
| OpenXR runtime not available | Stage 9 blocked | VR is compile-time optional (`BIMEUP_ENABLE_VR`). Desktop works without it |
| Large IFC files cause OOM | Performance issues | Stage 8 addresses: lazy loading, streaming, LOD |
| ImGui Vulkan backend conflicts with main render pass | UI rendering broken | Use separate render pass for ImGui. Well-documented approach |

---

## Estimated Session Count

| Stage | Sessions | Cumulative |
|-------|----------|------------|
| 1. Bootstrap | 1–2 | 1–2 |
| 2. Platform & Window | 2–3 | 3–5 |
| 3. Basic Rendering | 2–3 | 5–8 |
| 4. IFC & Scene | 3–4 | 8–12 |
| 5. Core & Selection | 2–3 | 10–15 |
| 6. UI | 2–3 | 12–18 |
| 7. BIM Features | 3–4 | 15–22 |
| 8. Performance | 2–3 | 17–25 |
| 9. VR | 3–4 | 20–29 |
| 10. Ray Tracing | 2–3 | 22–32 |
| 11. Polish & Release | 2–3 | 24–35 |

**Total: ~24–35 sessions**
