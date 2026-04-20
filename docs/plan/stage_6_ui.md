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

