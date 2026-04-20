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

