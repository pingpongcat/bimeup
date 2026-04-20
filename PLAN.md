# Bimeup — Multi-Stage Development Plan

**BIM me up** — Open-source BIM viewer for IFC files
Vulkan + ImGui + OpenXR | Linux + Windows

Per-stage details live in `docs/plan/` — see the [Stages index](#stages) below. This file is the top-level table of contents; do not add stage-level detail here.

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

## Stages

Each stage lives in its own file under `docs/plan/`. Open only the file for the current stage — don't read the whole directory.

| Stage | File | Focus |
|-------|------|-------|
| 1 — Project Bootstrap & Build System | [`docs/plan/stage_1_bootstrap.md`](docs/plan/stage_1_bootstrap.md) | CMake, submodules, logging, CI |
| 2 — Platform Layer & Window | [`docs/plan/stage_2_platform.md`](docs/plan/stage_2_platform.md) | GLFW window, Vulkan instance/device, swapchain |
| 3 — Basic Rendering Pipeline | [`docs/plan/stage_3_rendering.md`](docs/plan/stage_3_rendering.md) | Buffers, shaders, pipelines, orbit camera |
| 4 — IFC Loading & Internal Scene | [`docs/plan/stage_4_ifc.md`](docs/plan/stage_4_ifc.md) | web-ifc integration, scene graph, batching |
| 5 — Core Application & Selection | [`docs/plan/stage_5_selection.md`](docs/plan/stage_5_selection.md) | Application, EventBus, CPU raycasting |
| 6 — ImGui Integration & Basic UI | [`docs/plan/stage_6_ui.md`](docs/plan/stage_6_ui.md) | ImGui, UIManager, panels, toolbar |
| 7 — BIM Viewer Features | [`docs/plan/stage_7_bim_features.md`](docs/plan/stage_7_bim_features.md) | Measure, snap, clip, plan/section, visibility, PoV |
| R — Render Quality | [`docs/plan/stage_R_render_quality.md`](docs/plan/stage_R_render_quality.md) | Three-point lighting, shadow mapping |
| 8 — Loading Responsiveness & Memory | [`docs/plan/stage_8_performance.md`](docs/plan/stage_8_performance.md) | Async loading, axis-section, PoV stability |
| RP — Render Polish | [`docs/plan/stage_RP_render_polish.md`](docs/plan/stage_RP_render_polish.md) | HDR, SSAO→XeGTAO, SMAA, sun lighting (RP.16) |
| 9 — Ray Tracing | [`docs/plan/stage_9_raytracing.md`](docs/plan/stage_9_raytracing.md) | BLAS/TLAS, RT pipeline, hybrid rendering |
| 10 — VR Integration | [`docs/plan/stage_10_vr.md`](docs/plan/stage_10_vr.md) | OpenXR, stereo, controllers, teleport, VR UI |
| 11 — Polish & Release | [`docs/plan/stage_11_release.md`](docs/plan/stage_11_release.md) | Drag-drop, packaging, README, perf pass |

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
Stage 6: UI    Stage 10: VR ◄──┘            │
    │           │                            │
    ▼           │                            │
Stage 7: BIM   │            Stage 9: RT ◄──┘
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
**Parallel tracks**: RT (Stage 9) can start after Stage 3. VR (Stage 10) can start after Stage 4+5.

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
1. Read PROGRESS.md → find current stage + next unchecked task
2. Read docs/plan/stage_<X>.md for that stage (NOT this top-level PLAN.md)
3. Read only the relevant module headers/APIs
4. Write tests for the task
5. Implement the minimal code to pass tests
6. Refactor if needed
7. Run full test suite
8. Commit with descriptive message
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
| Vulkan ray tracing not available on target GPU | Stage 9 blocked | RT is optional. Detect at runtime, fall back to rasterization |
| OpenXR runtime not available | Stage 10 blocked | VR is compile-time optional (`BIMEUP_ENABLE_VR`). Desktop works without it |
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
| RP. Render Polish | 5–8 | 22–33 |
| 9. Ray Tracing | 2–3 | 24–36 |
| 10. VR | 3–4 | 27–40 |
| 11. Polish & Release | 2–3 | 29–43 |

**Total: ~24–35 sessions**
