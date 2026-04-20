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

