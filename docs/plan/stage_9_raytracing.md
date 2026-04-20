## Stage 9 — Ray Tracing (Optional Advanced Rendering)

**Goal**: Vulkan ray tracing pipeline for high-quality rendering mode. Ambient occlusion, reflections, soft shadows.

**Sessions**: 2–3

### Modules involved
- `renderer/` (RT pipeline, acceleration structures)
- `scene/` (BLAS/TLAS management)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 9.1 | Check RT support at runtime, create acceleration structures (BLAS per mesh) | Unit test: AS builds for test mesh, handle valid | `src/renderer/AccelerationStructure.h` |
| 9.2 | Build TLAS from scene instances | Unit test: TLAS contains correct number of instances | TLAS management |
| 9.3 | Create ray tracing pipeline (ray generation, closest hit, miss shaders) | Unit test: pipeline creates, shader binding table valid | RT pipeline in renderer/ |
| 9.4 | Implement RT ambient occlusion | Visual test: AO darkens corners and crevices | AO shader |
| 9.5 | Implement RT soft shadows (single directional light) | Visual test: soft shadow edges | Shadow shader |
| 9.6 | Implement RT reflections on glossy surfaces | Visual test: reflective materials show reflections | Reflection shader |
| 9.7 | Implement hybrid rendering — rasterize primary, RT for AO/shadows | Benchmark: acceptable frame time with RT effects | Hybrid pipeline |
| 9.8 | Toggle between rasterized and RT modes via UI | UI button switches mode, scene re-renders | Mode switching |

---

