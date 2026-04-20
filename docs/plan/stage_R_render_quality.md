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

