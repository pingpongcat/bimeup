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

