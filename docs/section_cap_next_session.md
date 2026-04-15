# Section View — Next Session Handoff

## Decision: reset and restart 7.5 with a BRep approach

The committed 7.5a–7.5h work built a stencil-cap section-view technique that **cannot be made to work on IFC geometry** (see "Why stencil fails" below). Rather than layer a BRep implementation on top of dead stencil infrastructure, reset the branch to before 7.5 started and re-do 7.5 from scratch with per-element BRep slicing.

**Reset point**: commit `9613514` (`PROGRESS: expand 7.4 with 7.4a–7.4e sub-tasks completed this session`). This is the last commit before 7.5a.

**First action next session**:

```bash
git reset --hard 9613514
```

This drops commits `9f43fa5` (7.5a) through `655c666` (7.5h) and any uncommitted WIP. Do **not** push with `--force` without asking — origin is still at `424ebc2` or similar, confirm with the user before any remote sync.

## Why the stencil cap fails on IFC

The stencil-parity trick counts back-face crossings between camera and cap plane; odd count → "inside solid". It requires:
1. Every element's mesh is closed/watertight (equal entries and exits along any ray).
2. Winding is consistent outward across the mesh.
3. No coincident faces between adjacent elements.

IFC in practice violates all three: walls are often single-surface, winding varies per element, shared boundaries produce overlapping faces. Scene-wide parity ends up random, per-element parity is only marginally better, and no pipeline tweak fixes the topology input.

Concrete evidence from the discarded 7.5h:
- With correct `0x01` stencil write/compare masks and the default face winding, the fill appeared in the **exterior** of the model rather than the wall cross-sections (image attached in session log).
- Switching `frontFace` to `CW` swapped the affected region but never produced cross-sections at wall thicknesses.
- Per-element parity spike (Alternative A, never implemented) was rejected after analysis — too many IFC elements are single-sided.

Conclusion: the correct input for capping is the **triangulated BRep**, not the rasterised depth/stencil buffer. Slice per triangle on CPU, stitch, triangulate, draw as flat-colour triangles.

## Plan: stage 7.5 (restart) — BRep section view

Re-plan 7.5 under a single umbrella with these subtasks. Each is its own TDD commit. Keep each commit small.

### 7.5a — `ClipPlane` section fields + manager setters
Re-add `ClipPlane.sectionFill: bool` (default false) and `ClipPlane.fillColor: glm::vec4` (default `(0.6, 0.6, 0.6, 1.0)`). Add `ClipPlaneManager::SetSectionFill(id, bool)` and `SetFillColor(id, vec4)`. Unit tests: default values, round-trip through `TransformToPlane` resets section fields.

### 7.5b — `ClipPlanesPanel` section UI
Checkbox "Section fill" and `ColorEdit4` "Fill color" per plane, wired to the new setters. No rendering change yet — just UI state. Existing panel tests cover regressions; add one new test asserting the checkbox mutates the manager.

### 7.5c — `scene::PlaneTriangleSlice`
Pure math in `src/scene/include/scene/Slicing.h` + `src/scene/src/Slicing.cpp`:

```cpp
struct TriangleCut {
    uint8_t pointCount;              // 0 or 2 (1 is degenerate and dropped)
    std::array<glm::vec3, 2> points; // world-space intersection points
};

TriangleCut SliceTriangle(const renderer::ClipPlane& plane,
                          const glm::vec3& a,
                          const glm::vec3& b,
                          const glm::vec3& c);
```

Classify vertices via `ClassifyPoint`; build the segment by linear-interpolating along the two edges that straddle the plane (`t = sdA / (sdA - sdB)`). Unit tests: all-front, all-back, all-on-plane, 2-1 split each direction, vertex-on-plane cases.

### 7.5d — `scene::SliceSceneMesh`
Walk a `SceneMesh`'s triangles (already indexed in `renderer::MeshBuffer` — read indices from the `SceneMesh` directly, not the GPU buffer), transform by the node's world matrix, call `SliceTriangle`, collect segments. Returns `std::vector<Segment>` where `Segment = {glm::vec3 a; glm::vec3 b;}`. Unit tests on a hand-built 12-triangle cube: plane `y=0.5` cutting a unit cube at origin produces exactly 4 segments that close into a unit square.

### 7.5e — `scene::StitchSegmentsToPolygons`
Stitch segments with shared endpoints (within epsilon) into closed polygons.

```cpp
std::vector<std::vector<glm::vec3>> StitchSegments(
    std::span<const Segment> segments, float epsilon = 1e-4F);
```

Hash endpoints into a quantised grid of resolution `epsilon`; greedy walk from any unvisited segment, follow matched endpoint to next segment, close when returning to start. Drop open polylines (gaps in mesh → don't try to force-close; rendering gracefully skips). Unit tests: 4 segments of a square → 1 polygon of 4 verts; two disjoint squares → 2 polygons; 3 segments that don't close → empty result.

### 7.5f — `scene::TriangulatePlanarPolygon`
Ear-clipping triangulation.

```cpp
std::vector<glm::vec3> TriangulatePolygon(const std::vector<glm::vec3>& polygon,
                                          const glm::vec3& planeNormal);
```

Project to the 2D plane dominant to `planeNormal` (drop the axis with the largest component magnitude), run ear-clipping in 2D, rebuild triangles with the original 3D vertices. Output size is multiple of 3. Unit tests: square → 2 triangles; concave L-shape → 4 triangles; signed area sum equals polygon area (within epsilon).

**Consider**: add `external/earcut.hpp` (header-only, MIT) as a submodule and wrap it instead of writing our own ear-clipper. Saves a day of work and handles robustness edge cases. Decide in-session based on submodule policy — `CLAUDE.md` allows submodules freely in `external/`.

### 7.5g — `renderer::SectionCapGeometry`
Per-scene cache of cap triangles + per-vertex color (baked in at build time):

```cpp
struct SectionVertex { glm::vec3 position; glm::vec4 color; };

class SectionCapGeometry {
public:
    explicit SectionCapGeometry(const Device&);
    // Rebuild cap triangles from every scene node whose bounds straddle any
    // plane with sectionFill=true. No-op when nothing is dirty.
    void Rebuild(const scene::Scene&, const ClipPlaneManager&);
    VkBuffer GetVertexBuffer() const;
    uint32_t GetVertexCount() const;
    bool IsEmpty() const;
};
```

Dirty tracking: recompute when any `ClipPlane.equation` or `sectionFill` or `fillColor` changes. Simplest version — hash the manager's state per frame and compare to last hash. Walk planes × nodes; for each straddling pair, slice → stitch → triangulate → append vertices with the plane's fill color.

Vulkan integration test: construct with a synthetic scene (one cube node), call `Rebuild` with one plane through the cube, assert vertex count > 0 and the buffer handle is valid.

### 7.5h — Flat-color section pipeline + shaders
`assets/shaders/section_fill.{vert,frag}` — position-only geometry already has baked per-vertex color, so the vertex shader passes color through to the fragment.

```glsl
// section_fill.vert
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(set = 0, binding = 0) uniform CameraUBO { mat4 view; mat4 projection; } camera;
layout(location = 0) out vec4 fragColor;
void main() {
    gl_Position = camera.projection * camera.view * vec4(inPosition, 1.0);
    fragColor = inColor;
}
```

```glsl
// section_fill.frag
layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;
void main() { outColor = fragColor; }
```

`renderer::SectionFillPipeline` — wraps `Pipeline` with: depth test `LEQUAL`, depth write off, no stencil, no clip-plane UBO, `CULL_NONE`, flat-colour. Integration test: construction + RAII.

### 7.5i — Wire into `main.cpp`
- Construct `SectionCapGeometry` and `SectionFillPipeline` next to the shaded/wireframe pipelines. Rebuild the pipeline on MSAA change.
- Per frame, if any section plane is active, call `capGeometry.Rebuild(...)` (internally no-op on unchanged state). If `!IsEmpty()`: bind the fill pipeline, bind `descriptorSet` (camera binding only), bind the cap vertex buffer, `vkCmdDraw(GetVertexCount(), 1, 0, 0)` — all inside the existing main render pass, after the scene draws, before UI.
- No stencil, no Pass A, no second render of the scene.

### 7.5j — Stage gate
Run full `ctest --output-on-failure`. Update `PROGRESS.md`: mark all 7.5a–7.5i done, move Current Task to 7.6.

## Files that will NOT exist after 7.5

These were all part of the discarded stencil path and the reset removes them. If you find yourself re-creating any of these in 7.5, stop — you've drifted back toward the wrong approach:

- `renderer/StencilCap.{h,cpp}`
- `renderer/StencilMarkPipeline.{h,cpp}`, `renderer/StencilMarkPipelineConfig.{h,cpp}`
- `renderer/CapFillPipeline.{h,cpp}`, `renderer/CapFillPipelineConfig.{h,cpp}`, `renderer/CapFillPushConstants.{h,cpp}`
- `renderer/SectionCap.{h,cpp}` (old `BuildCapQuad`)
- `renderer/DepthStencilFormat.{h,cpp}` (nice-to-have, but unused — only bring back if something else needs stencil)
- `assets/shaders/section_mark.{vert,frag}`, `assets/shaders/section_cap.{vert,frag}`
- All their test files

The new section path uses one shader pair (`section_fill.{vert,frag}`), one pipeline (`SectionFillPipeline`), one geometry manager (`SectionCapGeometry`), and four new `scene::` helpers (slice / stitch / triangulate / wrapper). ~1500 LOC smaller than the stencil attempt.

## Starter prompt for the next session

> Read `docs/section_cap_next_session.md`. Execute the reset-and-restart plan for stage 7.5.
>
> Steps:
> 1. `git reset --hard 9613514` to drop 7.5a–7.5h + any WIP.
> 2. Rewrite the 7.5 section in `PROGRESS.md`: replace the old 7.5a–h line items with the new 7.5a–j list from this doc (one line per subtask, unchecked). Keep the Stage 7 header; don't touch earlier stages.
> 3. Start subtask 7.5a (`ClipPlane` section fields). Follow `CLAUDE.md` rules — TDD, touched-module tests per task, one commit per task named `[7.5a] ...`.
>
> Do **not** read the full `PLAN.md` or re-explore the codebase; this doc + `PROGRESS.md` + the specific module you're touching are all you need. Do **not** push to origin without asking — the remote still has the discarded 7.5a–h commits.
>
> If at any point slicing produces obviously wrong geometry and you're tempted to fall back to stencil, re-read the "Why stencil fails" section of this doc first. The decision to go BRep was deliberate after empirical failure.
