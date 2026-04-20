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

