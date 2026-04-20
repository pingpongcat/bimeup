## Stage 8 — Loading Responsiveness & Memory

**Goal**: Open complex single-building IFC files (e.g. `Ifc2x3_SampleCastle.ifc` — 3.8k elements, 9k scene nodes) without OOM-killing the process, and keep the UI responsive during the multi-second parse + extract phase. The target use-case is compact buildings, not city-scale scenes — so this stage focuses on **load time, peak memory, and UI responsiveness**, not on draw-call throughput or view-dependent culling.

**Sessions**: 2

### Scope decisions (2026-04-17)

The original Stage 8 included BVH, frustum culling, LOD, and indirect drawing. These were dropped after re-scoping against the actual target (compact houses, not 100k-element campuses):

- **Frustum culling** — a single building is in-frustum almost always; the cull would save nothing.
- **BVH** — the scene-node count (≲10k) makes a linear raycast sub-millisecond; a tree adds complexity for no measurable picking gain. Frustum queries vanish with frustum culling.
- **LOD generation** — IFC house elements (walls/slabs/openings/fixtures) are already low-poly; nobody zooms out far enough for simplified versions to matter.
- **Indirect drawing** — Castle renders in 120 draw calls thanks to type+color batching (Stage 4.10). That's already cheap; indirect drawing matters past a few thousand draws.

If a future use-case introduces large-site or campus models, reopen this list — the dropped tasks aren't *wrong*, they're just not justified for the current target.

### Modules involved
- `ifc/` (async loading)
- `app/` (loading modal, progress wiring)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 8.1 | `ifc::AsyncLoader` — `LoadAsync(path, ProgressCallback)` + `Cancel()` returning `std::future<std::unique_ptr<IfcModel>>`; progress callback invoked from the worker thread, cancellation checked between phases | Unit test: load completes on background thread, progress callback fires monotonically, cancel returns early without crash | `src/ifc/include/ifc/AsyncLoader.h` |
| 8.2 | Loading modal in `app/` — ImGui overlay shown while the future is pending; displays % + current phase string + Cancel button; main render loop ticks (clear → present) so the window stays responsive | Manual test: open Castle, modal appears, progress advances, Cancel aborts the load and returns to empty scene | Loading UI in `src/app/main.cpp` (or new `ui::LoadingModal`) |

**Castle-scale perf work (skip-extraction-by-type, memory audit, thread pool,
benchmarks) was removed from Stage 8 on 2026-04-17.** The prior task list
(8.3–8.6) didn't close the loop on `Ifc2x3_SampleCastle.ifc`. Deferred for
now; when we revisit it we plan to try a different approach rather than
resume the old list.

### Expected APIs after Stage 8

```cpp
// ifc/include/ifc/AsyncLoader.h
namespace bimeup::ifc {
    using ProgressCallback = std::function<void(float percent, std::string_view phase)>;

    class AsyncLoader {
    public:
        std::future<std::unique_ptr<IfcModel>> LoadAsync(
            const std::string& path,
            ProgressCallback onProgress = nullptr);
        void Cancel();
        bool IsCancelled() const;
    };
}
```

### Out of scope (intentionally dropped from original Stage 8)

- BVH, frustum culling, LOD generation, indirect drawing — see "Scope decisions" above.
- Splash-screen-before-window — purely cosmetic; revisit in Stage 11 (Polish & Release) if still wanted after the loading modal lands.

### 8.3 Addendum — Axis Section Mode (added 2026-04-17)

A second-pass UX that combines **7.3 (clipping planes)** and **7.5 (section fill)** into a single BIM-oriented tool. The free-form 6-plane UI from 7.3d is retired in favour of at most **three axis-locked planes** (one per X / Y / Z), each with an explicit mode.

#### Data model

- `scene::AxisSectionController` owns ≤3 slots keyed by axis. Each slot is `{ offset: float, mode: CutFront | CutBack | SectionOnly }`.
- Each frame the controller syncs into the existing `renderer::ClipPlaneManager`:
  - CutFront → plane equation with normal = `+axis`, `d = -offset`
  - CutBack → normal = `−axis`, `d = +offset` (shader's front/back stays the same; just flipped)
  - SectionOnly → same equation as CutFront, **and** the controller sets a "hide scene" flag (`AnySectionOnly()`) that `main.cpp` reads to skip the shaded + transparent draws
- Section fill is implicitly on for every axis slot. The `ClipPlane::fillColor` picker is removed from the user-facing UI (field stays on the struct, defaulted so per-element tint from 7.5k is unchanged).

#### Rendering

- `main.cpp` draw loop:
  - if `controller.AnySectionOnly()` → skip opaque + transparent scene draws, keep section-fill pipeline draw
  - else → draw as today (CutFront/CutBack use the existing shader discard path)
- No new pipelines, shaders, or descriptor sets. `SectionCapGeometry` and `section_fill.{vert,frag}` (7.5g–h) are reused as-is.

#### Gizmo

- `ImGuizmo::OPERATION::TRANSLATE_X/Y/Z` scoped to the active slot's axis — bidirectional by default (drag ±).
- Small directional arrow marker (separate scene-space overlay, not an ImGuizmo widget) indicating which side is "kept" so CutFront vs CutBack reads at a glance.
- Stretch (8.3f): an ImGui popup anchored at the plane origin's screen-space position for in-viewport mode switching — ImGuizmo has no custom-widget hook so this is a separate overlay.

#### Tasks

| # | Task | Test |
|---|------|------|
| 8.3a | `scene::AxisSectionController` + sync into `ClipPlaneManager` | Unit: slot add/remove/replace per axis, CutBack sign flip, `AnySectionOnly`, idempotent sync |
| 8.3b | `ui::AxisSectionPanel` — X/Y/Z toggle + mode radio + offset slider | Unit: toggle adds/removes slot, radio writes mode, slider writes offset |
| 8.3c | Per-axis translate gizmo + direction marker overlay | Unit: gizmo → offset writeback math; marker is manual-verify |
| 8.3d | `main.cpp` draw-loop skip when `AnySectionOnly()` | Manual-verify (UI path) |
| 8.3e | Retire `ui::ClipPlanesPanel` + `ColorEdit4` surface | Compile gate; no functional test |
| 8.3f | Stretch — in-viewport mode-selector popup | Manual-verify |
| 8.3g | Retire ImGuizmo; custom single-header `ui::AxisSectionGizmo` (drag bar + grab + F/S/B segmented switcher + (×) close). View cube swapped to `imoguizmo`. | Unit: `ProjectWorldToScreen` + `AxisDragDelta` math; gizmo is manual-verify |
| 8.3h | UX polish: swap Y/Z button labels in `ui::AxisSectionPanel` so "Z" maps to world-Y (BIM users think Z = up); flip drag-handle bar to extend toward the cut-away side; cap present rate to display refresh via `VK_PRESENT_MODE_FIFO_RELAXED_KHR` (fallback FIFO); retire the FPS / camera-pos / small-axes debug overlay from `ui::ViewportOverlay` — keep only the measurement layer. | Manual-verify (UX + vsync); stale `ViewportOverlay` tests dropped |

#### Expected API

```cpp
// scene/include/scene/AxisSectionController.h
namespace bimeup::scene {
    enum class Axis : std::uint8_t { X, Y, Z };
    enum class SectionMode : std::uint8_t { CutFront, CutBack, SectionOnly };

    struct AxisSectionSlot {
        float offset;
        SectionMode mode;
    };

    class AxisSectionController {
    public:
        void SetSlot(Axis axis, AxisSectionSlot slot);
        void ClearSlot(Axis axis);
        [[nodiscard]] std::optional<AxisSectionSlot> GetSlot(Axis axis) const;
        [[nodiscard]] bool AnySectionOnly() const;
        void SyncTo(renderer::ClipPlaneManager& manager);
    };
}
```

### 8.4 Addendum — Stability + UX bundle (added 2026-04-18)

A bundle of small hardening + polish fixes observed on top of the Stage 8 deliverables. Not a coherent feature — grouped here because they share a single session and all land as sibling sub-tasks under the previously-dropped 8.4 slot.

#### Motivation

- **poly2tri crash**: dragging an axis-section gizmo eventually fed numerically degenerate polylines into `poly2tri::Sweep::FlipEdgeEvent`, which recursed into itself + `FlipScanEdgeEvent` until the stack blew. Stack overflow is not a C++ exception, so `scene::TriangulatePolygon`'s `try/catch → EarClipTriangulate` fallback never fired.
- **Axis-section UX gaps surfaced while using 8.3**: selection was still live under the gizmo, default mode didn't match BIM mental model on vertical/depth axes, gizmo drag could push the plane past the model, all three axes used the same colour, close glyph looked off-centre.
- **PoV mode marker**: ghosted non-slab geometry at 0.2 alpha was still too visible; hover disk disappeared behind walls/stairs.
- **Device pick**: reported picking integrated Radeon despite a discrete NVIDIA being present.

#### Tasks

| # | Task | Test |
|---|------|------|
| 8.4a | Patch `external/poly2tri/poly2tri/sweep/sweep.cc`: thread-local recursion-depth guard in `FlipEdgeEvent`/`FlipScanEdgeEvent`, throw `std::runtime_error` past 2048 frames so `scene::TriangulatePolygon`'s existing `EarClipTriangulate` fallback fires. | Manual-verify (reproduce pre-patch crash by hammering the section gizmo under ASan) |
| 8.4b | `main.cpp`: gate `core::PickElement` (left-click) + `core::HoverElement` on `axisSectionController.SlotCount() == 0` so selection is suppressed while any axis-section slot is active. | Manual-verify (UI gate) |
| 8.4c | `AxisSectionPanel::ToggleAxis` reads a per-axis default mode from `kAxisButtons`: X → `CutFront`, Y/Z → `CutBack`. | Existing panel tests stay green |
| 8.4d | `AxisSectionPanel` stores `glm::vec3 m_offsetMin/Max` + new `SetOffsetRange(vec3, vec3)` overload + `OffsetMin(Axis)/OffsetMax(Axis)`; old `(float,float)` signature preserved for backward-compat. Slider + gizmo-drag writeback clamp against per-axis range. `main.cpp` seeds from `AABB.GetMin()/GetMax()` with 10% per-axis padding. | Existing tests unchanged; manual-verify gizmo clamp |
| 8.4e | `AxisSectionGizmo.hpp` `DrawAxisHandle`: per-axis base colour (X=red, Y=blue, Z=green — CG convention; UI label mapping = world axis per 8.3h). `SectionOnly` still overrides to amber. Close "x" glyph nudged up 1.5 px. | Manual-verify (visual) |
| 8.4f | PoV polish: `ApplyPointOfViewAlpha(scene, 0.08F)` (was 0.2F); `DiskMarkerPipeline::depthCompareOp = VK_COMPARE_OP_ALWAYS` so the hover disk draws through ghosted geometry. | Manual-verify |
| 8.4g | `Device::PickPhysicalDevice` logs every enumerated `VkPhysicalDevice` (name, type, score, disqualification reason) + final selection. `Device::RateDevice`: discrete=100000, integrated=1000, virtual=100, cpu=10; `+1 per GiB` of `DEVICE_LOCAL` VRAM as discrete-vs-discrete tiebreaker. | Manual-verify (log + selection on hybrid hardware) |

#### Expected API changes

```cpp
// src/ui/include/ui/AxisSectionPanel.h
void SetOffsetRange(float minVal, float maxVal);          // backward-compat
void SetOffsetRange(const glm::vec3& min, const glm::vec3& max);
float OffsetMin(scene::Axis axis) const;
float OffsetMax(scene::Axis axis) const;
```

```cpp
// external/poly2tri/poly2tri/sweep/sweep.cc — internal only
thread_local int g_bimeup_flip_depth;                      // private, anon ns
// throws std::runtime_error once depth > 2048
```

