## Stage 10 — VR Integration

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
| 10.1 | Add OpenXR-SDK submodule, create `vr/VRSystem` — session lifecycle | Unit test: system initializes (or reports no HMD gracefully), creates session, destroys | `src/vr/include/vr/VRSystem.h` |
| 10.2 | Implement `vr/VRSwapchain` — stereoscopic swapchain management | Unit test: swapchain creates with correct eye resolution | `src/vr/include/vr/VRSwapchain.h` |
| 10.3 | Implement stereo rendering — render scene to left + right eye views | Visual test: VR headset shows stereo scene | Stereo pipeline |
| 10.4 | Implement `vr/VRInput` — controller tracking, button state | Unit test: input system reports controller poses, button events | `src/vr/include/vr/VRInput.h` |
| 10.5 | Implement teleport movement — arc ray + trigger to move | Visual test: teleport arc visible, movement works | Teleport in vr/ |
| 10.6 | Implement VR ray interaction — controller ray → element selection | Integration test: ray from controller → hits element → selection event fires | Ray interaction in vr/ |
| 10.7 | Implement VR UI panels — ImGui rendered to texture, placed in 3D space | Visual test: UI panel visible in VR, interactable with controller | VR UI in ui/ |
| 10.8 | Implement VR comfort features — vignette on movement, snap turning | Configuration options in config | Comfort settings |
| 10.9 | Implement scale model — grab + scale gesture to resize entire scene | Visual test: pinch gesture scales model | Scale interaction |

### Expected APIs after Stage 10

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

