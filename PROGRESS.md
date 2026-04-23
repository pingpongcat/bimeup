# Bimeup — Progress Tracker

## Current Stage: Stage 9 — Ray Tracing (pivot 2026-04-23: three modes, hybrid-composite dropped)
## Current Task: Next session — 9.Q (ray_query shadows). Start with `RenderQualityPanel` gaining a three-radio selector (`Rasterised` / `Ray query` / `Ray tracing`), then implement the ray_query mode end-to-end for sun shadows only. Ray-tracing mode is a separate-pipeline future task; hybrid-composite work is fully retired.

> **Pivot 2026-04-23 — three render modes, not hybrid.** Render-mode selector becomes three distinct modes, each a complete rendering path:
>
> 1. **Rasterised** (default, unchanged) — classical renderer: shadow maps + PCF + RP.18 transmission shadow map + XeGTAO + SMAA + edge overlay. Stays bit-compatible with today's output.
> 2. **Ray query** — still the classical forward-shaded pipeline, but `basic.frag` / `ssao_xegtao.comp` replace specific visibility lookups with inline `rayQueryInitializeEXT`/`ProceedEXT` calls against the TLAS. No dedicated RT pipeline, no SBT, no composite pass, no per-frame descriptor dance. Scope for the immediate rewrite: **sun shadow via ray_query only** (AO / transmission / indoor-fill come later if at all).
> 3. **Ray tracing** — future: a completely separate rendering pipeline, not glued into the classical raster path. Full RT (raygen + closest-hit + miss + any-hit) owning its own frame lifecycle. Simpler than the retired hybrid-composite because it doesn't try to merge raster and RT outputs per-contribution — it's just "a different renderer". Parked as a placeholder task below; implementation deferred.
>
> Why the flip: hybrid-composite (separate RT passes + composite compute passes writing into the shared HDR target) hit persistent `VUID-vkUpdateDescriptorSets-None-03047` on per-frame descriptor rewires across five descriptor sets (RtShadow/Ao/IndoorPass + sun/indoor composites). The per-frame plumbing was too fragile for the payoff, and the "merge per contribution" model was over-engineered for a viewer whose RT needs are single-ray visibility queries. Ray queries inside the existing raster shaders cover 95% of those needs with a fraction of the plumbing; a fully-separate RT pipeline (option 3) covers anything ray queries can't. The hybrid-RT test fix series from this session was reverted. BLAS/TLAS machinery (9.1 / 9.2) stays. 9.9 path tracer, if ever pursued, folds into option 3's separate RT pipeline.

## Stage CLI — Command-line flags + build ergonomics
- [x] CLI.1 `--device-id <N>` + `-h` / `--help`. New `tools::CliArgs` parser (positional `<ifc-path>`, `--help` / `-h`, `--device-id N|--device-id=N`, friendly error + help on unknown/multiple positionals). `Device` gains a 3-arg ctor `(instance, surface, deviceIndexOverride)` that bypasses the rate-based auto-pick; OOR / no-graphics / no-present overrides throw with a user-readable message. `PickPhysicalDevice` now logs `Vulkan device #N: ...` so the index is visible in the startup log. Tests: 13× `CliArgsTest` (parse, error paths, help text). Wired in `main.cpp` before log init; `argv[1]` positional path now routes through `cli.ifcPath.value_or(BIMEUP_SAMPLE_IFC)`.
- [x] CLI.2 Debug build now compiles with `-Og -g` (non-MSVC) in place of CMake's default `-O0 -g` — same debuggability, noticeably faster Vulkan/ImGui frame iteration. MSVC Debug left alone (`/RTC1` conflicts with optimization). Set via `CMAKE_{C,CXX}_FLAGS_DEBUG` FORCE-cached in the top-level `CMakeLists.txt`. Sanitizer default (`BIMEUP_ENABLE_SANITIZERS=OFF`) was already correct; no change needed there beyond clearing the stale `build/CMakeCache.txt` where it had been manually enabled.

> Completion notes live in `git log` (all commits use `[stage.task] description` per CLAUDE.md). This file stays terse — one line per task, sub-tasks one line each. Plan details per stage: `docs/plan/stage_<X>.md`.

## Stage 1 — Project Bootstrap & Build System
- [x] 1.1 Root CMakeLists.txt (C++20, modules, options)
- [x] 1.2 Submodules: googletest, spdlog, glm
- [x] 1.3 `tools/Log` (spdlog wrapper)
- [x] 1.4 `tools/Config` (key-value loader)
- [x] 1.5 `app/main.cpp` bootstrap
- [x] 1.6 Build scripts (Linux/Windows × debug/release)
- [x] 1.7 GitHub Actions CI
- [x] 1.8 clang-tidy + sanitizers

## Stage 2 — Platform Layer & Window
- [x] 2.1 GLFW submodule + `platform/Window`
- [x] 2.2 `platform/Input`
- [x] 2.3 Vulkan instance + debug messenger
- [x] 2.4 Vulkan physical/logical device
- [x] 2.5 Swapchain tied to GLFW surface
- [x] 2.6 Render loop: acquire → clear → present
- [x] 2.7 Frame timing + FPS

## Stage 3 — Basic Rendering Pipeline
- [x] 3.1 `renderer/Buffer` (VMA)
- [x] 3.2 `renderer/Shader` (SPIR-V)
- [x] 3.3 `renderer/Pipeline`
- [x] 3.4 `renderer/DescriptorSet` (UBO)
- [x] 3.5 `renderer/Camera` (perspective + orbit)
- [x] 3.6 Basic vertex + fragment shaders
- [x] 3.7 Colored cube with orbit camera
- [x] 3.8 `renderer/MeshBuffer`
- [x] 3.9 Render-mode switching (shaded/wireframe)

## Stage 4 — IFC Loading & Internal Scene
- [x] 4.1 IFC parser submodule
- [x] 4.2 `ifc/IfcModel`
- [x] 4.3 `ifc/IfcElement`
- [x] 4.4 `ifc/IfcGeometryExtractor`
- [x] 4.5 `ifc/IfcHierarchy`
- [x] 4.6 `scene/SceneNode` + `scene/Scene`
- [x] 4.7 `scene/SceneMesh`
- [x] 4.8 `scene/AABB`
- [x] 4.9 `scene/SceneBuilder`
- [x] 4.10 Batching in SceneBuilder
- [x] 4.11 Scene meshes upload to renderer
- [x] 4.12 Per-element color from IFC

## Stage 5 — Core Application & Selection
- [x] 5.1 `core/Application`
- [x] 5.2 `core/EventBus`
- [x] 5.3 Core events
- [x] 5.4 CPU raycasting
- [x] 5.5 Click → ray → selection
- [x] 5.6 Renderer highlight on selected
- [x] 5.7 Hover highlight
- [x] 5.8 Multi-selection + clearing

## Stage 6 — ImGui Integration & Basic UI
- [x] 6.1 ImGui Vulkan+GLFW backend
- [x] 6.2 `ui/UIManager`
- [x] 6.3 `ui/Panel` interface
- [x] 6.4 `ui/HierarchyPanel`
- [x] 6.5 `ui/PropertyPanel`
- [x] 6.6 `ui/Toolbar`
- [x] 6.7 `ui/ViewportOverlay`
- [x] 6.8 Hierarchy → selection → property wiring
- [x] 6.9 UI theme
- [x] 6.10 Panels wired into `main.cpp`
- [x] 6.11 Toolbar fit-to-view + hover picking in `main.cpp`
- [x] 6.12 `fillModeNonSolid` + surface/swapchain destroy order + per-image semaphores
- [x] 6.13 Default startup scene = bundled `sample.ifc`

## Stage 7 — BIM Viewer Features
- [x] 7.0 Element-selection fix (nodeId→expressId + batching-aware raycast)
- [x] 7.0a Projection Y-flip fix + selection highlight via `SetVertexColorOverride`
- [x] 7.1 Distance measurement (`scene::Measure` + `MeasureTool`)
- [x] 7.2 Point snapping (`scene::Snap` — vertex/edge/face)
- [x] 7.3 Clipping planes
  - [x] 7.3a `renderer::ClipPlane` math + unit tests
  - [x] 7.3b `ClipPlaneManager` (≤6 planes)
  - [x] 7.3c UBO + shader (`basic.frag` discard + binding 3)
  - [x] 7.3d `ui::ClipPlanesPanel`
  - [x] 7.3e ImGuizmo integration (submodule + Manipulate + ViewManipulate cube)
- [x] 7.4 Plan view (Ground Floor / Roof, horizontal clip + top-down ortho)
  - [x] 7.4a/b/c/d/e Viewport-aspect / yaw-reset / ortho zoom / perspective restore / `SelectionCleared` event
- [x] 7.5 Section view (BRep slice-stitch-triangulate)
  - [x] 7.5a ClipPlane section fields + manager setters
  - [x] 7.5b Panel "Section fill" toggle + color
  - [x] 7.5c `scene::SliceTriangle` math
  - [x] 7.5d `scene::SliceSceneMesh`
  - [x] 7.5e `scene::StitchSegments`
  - [x] 7.5f `scene::TriangulatePolygon` (ear-clip)
  - [x] 7.5g `scene::SectionCapGeometry` (dirty-tracked)
  - [x] 7.5h `section_fill.{vert,frag}` + `SectionFillPipeline`
  - [x] 7.5i Wire into `main.cpp`
  - [x] 7.5j Stage gate
- [x] 7.6 Element visibility by IFC type
  - [x] 7.6a `SetVisibilityByType` + `GetUniqueTypes` + `DefaultHiddenTypes`
- [x] 7.7 Element isolation
  - [x] 7.7a `IsolateByExpressId` / `ShowAll` / `FindByExpressId`
  - [x] 7.7b HierarchyPanel selection-sync + auto-expand
  - [x] 7.7c Per-row eye/isolate buttons
  - [x] 7.7d Toolbar "Frame Selected"
  - [x] 7.7e Row dim for hidden types
  - [x] 7.7f Active-bg on eye/isolate + ShowAll on second-click
- [x] 7.8 Element transparency override
  - [x] 7.8a `ExtractSubMeshes` preserves per-piece surface-style alpha
  - [x] 7.8c Transparent pass (alpha-over blend, draw order opaque → caps → transparent)
  - [x] 7.8d Per-element / per-type alpha overrides (PropertyPanel + TypeVisibilityPanel)
- [x] 7.9 Fit-to-view (`Camera::Frame` + toolbar/Home/Numpad .)
- [x] 7.10 First-person navigation
  - [x] 7.10a Type-alpha defaults (`DefaultTypeAlphaOverrides` = `{IfcWindow: 0.4}`)
  - [x] 7.10b Toolbar "Point of View" + `ApplyPointOfViewAlpha`
  - [x] 7.10c Teleport on IfcSlab click + `renderer::FirstPersonController` + disk-marker hover
  - [x] 7.10d `ui::FirstPersonExitPanel`

## Stage R — Render Quality
- [x] R.1 Three-point lighting + Render Quality panel scaffold
- [x] R.2 MSAA 1×/2×/4×/8× with resolve
- [x] R.3 Shadow mapping (light-space matrix + depth pass + PCF + shader wiring)
- [ ] R.4 SSAO — **deferred** (shadow maps cover depth cues; revisit after Stage N)

## Stage N — Navigation (Blender-style viewport)
- [x] N.1a `renderer::ViewportNavigator::ClassifyDrag` (MMB orbit / Shift pan / Ctrl dolly)
- [x] N.1b Ortho toggle + numpad 5
- [x] N.1c `Camera::Frame` + Home / Numpad . / Shift+C
- [x] N.1d Axis snaps (numpad 1/3/7, Ctrl = opposite)

## Stage 8 — Loading Responsiveness & Memory
Re-scoped 2026-04-17: original 8.2 BVH, 8.3 frustum-cull, 8.4 LOD, 8.5 indirect dropped (city-scale, not building-scale).
- [x] 8.1 Async IFC loading (`AsyncLoader` + progress/cancel)
- [x] 8.2 Loading modal in `app/`
- [ ] 8.3 Axis Section Mode — re-do of 7.3 + 7.5 as a single BIM-oriented UX. Up to three axis-locked clip planes per X/Y/Z; modes {CutFront, CutBack, SectionOnly}; section fill always on.
  - [x] 8.3a `scene::AxisSectionController`
  - [x] 8.3b `ui::AxisSectionPanel`
  - [x] 8.3c Single-axis translate gizmo + direction marker
  - [x] 8.3d Draw-loop wiring
  - [x] 8.3e Retire free-form 6-plane UI (`ClipPlanesPanel` deleted)
  - [x] 8.3f In-viewport mode-selector popup
  - [x] 8.3g Retire ImGuizmo + custom `AxisSectionGizmo` + imoguizmo view cube
  - [x] 8.3h UX polish bundle (Y/Z swap, axis-colour tweaks)
- [x] 8.4 Stability + UX bundle (2026-04-18)
  - [x] 8.4a poly2tri stack-overflow guard
  - [x] 8.4b Selection disabled during section view
  - [x] 8.4c Per-axis default section mode
  - [x] 8.4d Per-axis offset clamp from scene AABB
  - [x] 8.4e Per-axis gizmo colours (CG convention)
  - [x] 8.4f PoV marker + ghosting polish
  - [x] 8.4g Device-pick robustness + telemetry

## Stage RP — Render Polish (non-RT beauty pass)
Closed 2026-04-19 (RP.13b), reopened for RP.14; closed 2026-04-19 (RP.14.2), reopened for RP.15; closed 2026-04-20 (RP.15.b), reopened 2026-04-20 for RP.16.
- [x] RP.1 Hemisphere sky ambient (replaces flat ambient)
- [x] RP.2 HDR offscreen target + ACES tonemap resolve
  - [x] RP.2a/b/c CPU ACES mirror → shaders + `TonemapPipeline` → two-pass composition
- [ ] RP.3 MRT normal G-buffer (R16G16 oct-packed view-space normal) — marked open but satisfied by RP.3a–d landing during RP.4/5 setup
  - [x] RP.3a/b/c/d Oct-pack mirrors → MRT pipeline → RenderLoop attachment → `basic.frag` emit
- [x] RP.4 Linear depth + 4-mip pyramid (compute)
  - [x] RP.4a/b/c/d Mirrors → `depth_linearize.comp` → `depth_mip.comp` → RenderLoop wire
- [x] RP.5 SSAO (Chapman hemisphere kernel; later replaced by RP.12e XeGTAO)
  - [x] RP.5a/b/c/d Kernel mirrors → `ssao_main.comp` → `ssao_blur.comp` → RenderLoop + tonemap composite
- [x] RP.6 Selection + hover outline (stencil + Sobel + depth-discontinuity) — later retired in RP.15
  - [x] RP.6a/b/c/d Edge mirrors → outline pipeline → stencil G-buffer → draw-push + panel
- [x] RP.7 SSIL (Godot/Intel port) — later retired in RP.13a
  - [x] RP.7a/b/c/d Reprojection mirrors → `ssil_main.comp` → `ssil_blur.comp` → RenderLoop + tonemap additive
- [x] RP.8 FXAA post — later replaced by SMAA (RP.11)
  - [x] RP.8a/b/c Luma mirrors → `fxaa.{vert,frag}` + pipeline → RenderLoop wire
- [x] RP.9 Depth fog — later retired in RP.13b
  - [x] RP.9a/b `ComputeFog` mirror → tonemap.frag + panel
- [x] RP.10 Small-kernel bloom — later retired in RP.12a
  - [x] RP.10a/b/c/d Mirrors → dual-filter pipelines → RenderLoop wire → exposure knob + validation cleanup
- [x] RP.11 SMAA 1x replaces FXAA
  - [x] RP.11a `SmaaMath` CPU (luma + edge predicate)
  - [x] RP.11b SMAA shaders + pipelines (sub-split 4 ways)
    - [x] RP.11b.1 AreaTex/SearchTex LUTs (`iryoku/smaa` submodule) + `renderer::SmaaLut`
    - [x] RP.11b.2 `smaa.vert` + `smaa_edge.frag` + `SmaaEdgePipeline`
    - [x] RP.11b.3 `smaa_weights.frag` + `SmaaWeightsPipeline`
    - [x] RP.11b.4 `smaa_blend.frag` + `SmaaBlendPipeline`
  - [x] RP.11c Wire SMAA 3-pass chain (replaces FXAA; not MSAA-gated)
- [x] RP.12 Architectural rendering polish
  - [x] RP.12a Retire bloom
  - [x] RP.12b Transparency bit in stencil G-buffer (bit 4)
  - [x] RP.12c SSIL transparency-gate + luminance clamp + toned defaults
    - [x] RP.12c.1 CPU clamp + shader clamp + push grow
    - [x] RP.12c.2 Stencil binding + per-tap transparency gate
    - [x] RP.12c.3 "Max luminance" slider + default-intensity tweak
  - [x] RP.12d SSAO transparency-gate + architectural defaults
  - [x] RP.12e SSAO → XeGTAO port
    - [x] RP.12e.1 `XeGtaoMath` CPU (slice direction + visibility)
    - [x] RP.12e.2 `ssao_xegtao.comp` + `SsaoXeGtaoPipeline`
    - [x] RP.12e.3 RenderLoop swap + retire classic SSAO
- [x] RP.13 Retire SSIL + Fog
  - [x] RP.13a Retire SSIL (shaders, pipelines, math, tests)
  - [x] RP.13b Retire Fog (math, `FogSettings`, `SetFogParams`)
- [x] RP.14 Retire MSAA
  - [x] RP.14.1.a RenderLoop-side MSAA removal
  - [x] RP.14.1.b Pipeline signature cleanup (drop `rasterizationSamples` + ctor param)
  - [x] RP.14.2 UI + app wiring cleanup
- [x] RP.15 Retire selection outline
  - [x] RP.15.a Retire `OutlinePipeline` + shaders + RenderLoop wiring + panel widgets
  - [x] RP.15.b Slim stencil G-buffer to transparency-only + retire hover plumbing

- [ ] RP.16 Site-driven sun lighting — retire three-point for a single physically-plausible sun driven by `(date, time, site lat/long, TrueNorth)`; sky ambient derived from sun elevation; artificial-indoor-lights preset toggle. RT-friendly (sun = future RT shadow ray, `SkyColor` LUT = future miss-shader sample). LightingUbo shape preserved (sun→key, indoor fill→fill, rim zeroed). Plan: `docs/plan/stage_RP_render_polish.md` → "RP.16".
  - [x] RP.16.1 Solar-position math (`renderer/SunPosition.{h,cpp}`, NOAA-simplified)
  - [x] RP.16.2 Sky colour from elevation (`renderer/SkyColor.{h,cpp}`, 5-key LUT)
  - [x] RP.16.3 IfcSite location extraction (`ifc/IfcSiteLocation.{h,cpp}` — RefLat/Lon DMS, RefElevation, TrueNorth)
  - [x] RP.16.4 `SunLightingScene` + `PackSunLighting` (retired `LightingScene`/`MakeDefaultLighting`). Sub-split:
    - [x] RP.16.4.a `SunLightingScene` + `PackSunLighting` additive (sun→key, fill/rim zeroed, ambient from sky LUT)
    - [x] RP.16.4.b Retired `LightingScene`/`MakeDefaultLighting`/`PackLighting` + wired `sun` through `main.cpp`/`RenderQualityPanel`
  - [x] RP.16.5 Artificial-indoor preset (overhead fill + ambient boost)
  - [x] RP.16.6 `RenderQualityPanel` rewrite (Sun header; retire three-point + sky-colour)
  - [x] RP.16.7 Wire site → panel → renderer in `main.cpp`
  - [x] RP.16.8 `ApplicationTest` VMA-leak — fixed via CMakeLists.txt shader-path ordering (set() before add_subdirectory(src/core) so `BIMEUP_SHADER_DIR` isn't empty) + `RenderLoop::Cleanup()` now calls `CleanupDepthPyramidResources / CleanupSmaaResources / CleanupSsaoResources`.
  - Ordering: 16.1 / 16.2 / 16.3 parallelizable → 16.4 → 16.5 → 16.6 → 16.7 → 16.8 (stage gate)
  - Stage gate at RP.16.8: full `ctest -j$(nproc) --output-on-failure` 547/547 ✓ (2026-04-20)

- [ ] RP.17 Feature-edge overlay — retire the noisy `VK_POLYGON_MODE_LINE` wireframe (every triangulation seam visible) in favour of an overlay that draws only **feature edges** (boundary + dihedral-angle > threshold). Extracted CPU-side per source mesh at scene-build time, drawn after opaque + before transparent with depth-test ≤, polygon-offset bias, configurable alpha/thickness. Sharper read on the model and, later, a first-class snap source for measurement. Plan: `docs/plan/stage_RP_render_polish.md` → "RP.17".
  - [x] RP.17.1 `scene::EdgeExtractor` (dihedral-angle filter, weld by position, CPU) + unit tests
  - [x] RP.17.2 `SceneMesh` line buffer + `SceneBuilder` wires extractor per source mesh
  - [x] RP.17.3 `renderer::EdgeOverlayPipeline` (line topology, depth ≤, polygon-offset, alpha)
  - [x] RP.17.4 `edge_overlay.{vert,frag}` + RenderLoop draw-pass wiring
  - [x] RP.17.5 Toolbar "Edges" toggle replaces current "Wireframe" radio + main.cpp cleanup
  - [ ] RP.17.6 (Optional) feed extracted edges into `scene::Snap` as edge-snap source
  - [x] RP.17.7 Smooth-line rasterization via `VK_EXT_line_rasterization` (added 2026-04-20)
  - [ ] RP.17.8 Edge overlay + axis section (added 2026-04-20)
    - [x] RP.17.8.a Edge overlay respects axis clip planes (ClipPlanesUbo binding + `edge_overlay.frag` discard)
    - [x] RP.17.8.b Section-plane element outlines (reuse `SectionCapGeometry` stitched polylines; draw via `EdgeOverlayPipeline` after caps)
  - Ordering: 17.1 → 17.2 → 17.3 → 17.4 → 17.5 → 17.7 → 17.8.a → 17.8.b → (optional 17.6) → stage gate
  - Stage gate at end of RP.17: full `ctest -j$(nproc) --output-on-failure` 565/565 ✓ (2026-04-20, re-run after 17.8)

- [ ] RP.18 Window-transmitted sun shadows — classical raster approximation of Stage 9.6's RT transmission, so sun lights the floor behind `IfcWindow` glass in the default (non-RT) renderer. Coloured transmissive shadow map: second attachment on the shadow render pass, min-blended RGBA glass tint, sampled in `basic.frag` and multiplied into the sun term after the PCF visibility test. Default on. Plan: `docs/plan/stage_RP_render_polish.md` → "RP.18".
  - [x] RP.18.1 Transmission attachment in `ShadowPass` (R16G16B16A16_SFLOAT, cleared white)
  - [x] RP.18.2 `shadow_transmission.{vert,frag}` + `ShadowTransmissionPipeline` (min-blend, depth-test only)
  - [x] RP.18.3 Draw-loop wiring: classify opaque vs transmissive via existing `effectiveAlpha` plumbing
  - [x] RP.18.4 `basic.frag` samples transmission map + multiplies sun tint (+ `ComputeTransmittedSun` CPU mirror)
  - [x] RP.18.5 Panel toggle `windowTransmission` (default on) + stage gate
  - Ordering: 18.1 → 18.2 → 18.3 → 18.4 → 18.5 → stage gate
  - Stage gate at RP.18.5: full `ctest -j$(nproc) --output-on-failure` 570/570 ✓ (2026-04-20)
  - [x] RP.18.6 Neutralise glass transmission tint (push `vec3(1 - alpha)` instead of `surfaceColor.rgb * (1 - alpha)`; architectural glass is near-neutral in transmission, blue was bleeding onto sunlit floors).
  - [x] RP.18.7 Gate glass tint on light-space depth (store nearest-glass Z in `transmit.a` + `sunColor * visibility * (glassAhead ? tint : 1)`; walls between a window and an unrelated room now correctly block the tint).

- [x] RP.19 SMAA tuning knobs in `RenderQualityPanel` — threshold slider (0.05–0.2) + LOW/MEDIUM/HIGH quality radio wired through `SmaaSettings` → `SmaaEdgePipeline`/`SmaaWeightsPipeline` push constants. `smaa_weights.frag`'s `SMAA_MAX_SEARCH_STEPS[_DIAG]` promoted from const to push-constant ints. Defaults pinned in `RenderQualityPanelTest`.

- [x] RP.20 XeGTAO panel knobs — `SsaoSettings { radius, falloff, intensity, shadowPower }` with architectural defaults (0.35 m / 0.6 / 0.5 / 1.5) wired through `RenderLoop::SetSsaoParams` into `ssao_xegtao.comp`'s push constants. Panel gains an "Ambient occlusion" header with four sliders. Defaults pinned in `RenderQualityPanelTest`.

- [x] RP.21 Edge overlay settings + always-on default — `EdgeOverlaySettings { enabled=true, color, opacity, width }` replaces the hardcoded `kEdgeColor` + `edgesAutoFromMeasure` hack in `main.cpp`. `VK_DYNAMIC_STATE_LINE_WIDTH` added to `EdgeOverlayPipeline`; `vkCmdSetLineWidth` emitted per draw (clamped to 1.0 on devices without `wideLines`). Measurement-mode auto-enable retired. Panel gains an "Edges" header with checkbox + color picker + opacity + width sliders; toolbar + `W` key + panel all share the same source of truth. Defaults pinned in `RenderQualityPanelTest`.

Stage gate at end of Stage RP: full `ctest -j$(nproc) --output-on-failure` all passing ✓ (2026-04-20).

## Stage 9 — Ray Tracing (additive, opt-in render mode)
Goal: add an RT light-transport render mode *alongside* the classical renderer. **Nothing is removed or replaced** — XeGTAO, shadow maps, SMAA, edge overlay all stay live. Classical rasterised is the default on every launch; Hybrid RT and Path Traced are opt-in modes selected in `RenderQualityPanel`, both gated on `VK_KHR_ray_tracing_pipeline` (not guaranteed on all GPUs). Sun direction / site / date-hour / indoor preset continue to come from `SunLightingScene` — single authoritative lighting model shared across all modes.

> **RETIRED 2026-04-23 — three-mode pivot.** Tasks 9.3 through 9.8.d below are preserved as historical record but represent a superseded hybrid-composite approach. Replaced by 9.Q (ray query shadows) below, and eventually a separate-pipeline 9.RT (full ray tracing) mode. 9.1 + 9.2 (BLAS/TLAS machinery) stay — both modes need them. 9.9 path tracer, if ever pursued, folds into 9.RT.

### 9.Q — Ray query mode (focus of the next session)
Scope: classical forward-shaded pipeline unchanged; `basic.frag` + the PCF shadow-map sample path swap for inline `rayQueryInitializeEXT` against the TLAS when the panel's render mode is `Ray query`. Sun shadows only for the first slice — AO, transmission, indoor-fill stay on their classical paths for now.
- [ ] 9.Q.1 `Device::HasRayQuery()` probe + `VK_KHR_ray_query` enabled on the logical device. Gated the same way as `HasRayTracing`. Panel's third radio is greyed out when false.
- [ ] 9.Q.2 TLAS descriptor binding plumbed into the main descriptor set layout (so `basic.frag` can access it). Binding is optional — raster mode leaves it null.
- [ ] 9.Q.3 `basic.frag` gains `#extension GL_EXT_ray_query : require` + a `uint useRayQueryShadow` push constant + a ray-query branch replacing the PCF shadow-map sample when the flag is on. `shadow_transmission` sample path stays live for glass tint.
- [ ] 9.Q.4 `RenderQualityPanel` — third radio (`Ray query`) joins `Rasterised` / (disabled) `Ray tracing`. `main.cpp` maps it onto a new `RenderLoop::RenderMode::RayQuery` + pushes the shader flag each frame.
- [ ] 9.Q.5 Stage gate — visual check (Ray query shadows match classical shadow-map output for the sample scene) + full `ctest`.

### 9.RT — Full ray tracing mode (parked)
Scope: a completely separate rendering pipeline selected by the panel's third radio. Not hybrid, not composited — a distinct renderer owning its own frame lifecycle. Simpler than the retired hybrid-composite because there's no contract to merge raster and RT outputs per-contribution. Implementation deferred until after 9.Q lands and we've lived with ray-query-shadows enough to know what the full-RT mode actually needs to deliver.
- [ ] 9.RT.1 Render-mode enum + panel radio stub (disabled until the pipeline lands).
- [ ] 9.RT.2..N TBD — raygen / hit / miss / SBT / frame loop. Out of scope for now.
- [ ] 9.1 RT-capability probe + BLAS per mesh (`AccelerationStructure`)
  - [x] 9.1.a `Device::HasRayTracing()` probe — enumerates `VK_KHR_acceleration_structure` + `ray_tracing_pipeline` + `deferred_host_operations`, queries `accelerationStructure` / `rayTracingPipeline` + 1.2-core `bufferDeviceAddress` / `descriptorIndexing`. RT extensions + feature chain (`VkPhysicalDeviceVulkan12Features` + AS/RT KHR structs) enabled on the logical device. Retry-without-RT fallback when `vkCreateDevice` rejects the chain (observed on NVIDIA 595 headless) so classical path always stays live. `HasRayTracing()` reflects the post-create truth.
  - [x] 9.1.b `renderer::AccelerationStructure` — BLAS-per-mesh build from `MeshData`, dispatch-pointer load via `vkGetDeviceProcAddr`, no-op when `device.HasRayTracing() == false`. Raw-Vulkan buffers with `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT` (VMA's BDA flag not configured on this codebase); synchronous fence-wait on the graphics queue for the one-shot BLAS build; RT-device tests `GTEST_SKIP` gracefully when the probe says no.
- [x] 9.2 TLAS build from scene instances (built only when an RT mode is active) — `renderer::TopLevelAS` + `TlasInstance{transform, blasAddress, customIndex, mask}`. `Build(instances)` rebuilds from scratch: tears down any previous AS first (even when the new list is empty), transposes glm::mat4 into VK's 3×4 row-major `VkTransformMatrixKHR`, packs `VkAccelerationStructureInstanceKHR`s, queries sizes, allocates AS storage + scratch, submits a synchronous build on the graphics queue. Caches TLAS handle + device address for SBT descriptor binding in 9.3. Strict no-op on non-RT devices; same raw-Vulkan-BDA buffer pattern as 9.1.b. Tests cover count-matches, rebuild-replaces-handle, rebuild-to-zero-resets, empty-list-no-op, and non-RT-is-no-op; 263/263 renderer suite ✓.
- [x] 9.3 RT pipeline + SBT (raygen / closest-hit / miss / any-hit), built lazily — `renderer::RayTracingPipeline` owns one raygen / miss / hit group (closest-hit + optional any-hit), a pipeline layout (caller-supplied descriptor-set layout + optional push-constant range) and a host-visible SBT buffer. `Build(settings)` reads SPIR-V from `settings.{raygenPath,missPath,closestHitPath,anyHitPath}.spv`, creates shader modules, configures 3 shader groups (2× general + 1× triangles-hit), calls `vkCreateRayTracingPipelinesKHR`, fetches group handles via `vkGetRayTracingShaderGroupHandlesKHR`, and lays out raygen/miss/hit records at `shaderGroupBaseAlignment` boundaries (raygen size == stride, miss/hit size == alignedHandleSize). Rebuild tears down + re-creates. Strict no-op on non-RT devices. New `rt_probe.{rgen,rchit,rmiss,rahit}` shader stubs + `CompileShaders.cmake` RT-bucket (glslc `--target-env=vulkan1.2 --target-spv=spv1.4`). Tests: 6 (2 always-run + 4 GTEST_SKIP on non-RT) covering construct, no-op-on-non-RT, empty-paths-false, build-pipeline-and-SBT, build-without-anyhit, rebuild-replaces-handle. 264/264 renderer suite ✓.
- [x] 9.4 RT sun shadows — *additive* alongside shadow-map path; Hybrid picks RT, Rasterised still uses PCF
  - [x] 9.4.a `shadow.{rgen,rmiss}` + `renderer::RtShadowPass` — visibility R8 image (STORAGE + SAMPLED), descriptor set (TLAS at 0, depth combined-image-sampler at 1, visibility storage image at 2), RT pipeline via 9.3 with `rt_probe.rchit` reused as the skipped closest-hit, and `Dispatch(cmd, tlas, depthView, depthSampler, view, proj, sunDirWorld)` emitting barriers + `vkCmdTraceRaysKHR` + tail transition to SHADER_READ_ONLY_OPTIMAL. 96-byte push block `{ invViewProj, sunDirWorld, extent, pad×2 }` stays under the 128-byte minimum guarantee. Tests: 3 always-run (construct / no-op-on-non-RT / PushConstants==96) + 3 GTEST_SKIP on non-RT (build-allocates / rebuild-replaces-image / dispatch-records-and-submits-clean with triangle-BLAS + 1-instance-TLAS + 16×16 D32_SFLOAT depth); renderer suite 265/265 ✓.
  - [x] 9.4.b `RenderLoop` wire — `RenderMode::{Rasterised, HybridRt}` enum + `SetRenderMode` / `GetRenderMode` (Rasterised default); `SetRtShadowInputs(tlas, sunDirWorld, view)` per-frame API; `GetRtShadowVisibilityView()` accessor (null-handle until 9.8 composites). Flipping to Hybrid RT lazy-builds `RtShadowPass` + a dedicated NEAREST/clamp depth sampler; flipping back `WaitIdle` + destroy. Per-frame dispatch slots between `vkCmdEndRenderPass(main)` and `BuildDepthPyramid` (depth already in SHADER_READ_ONLY_OPTIMAL from main-pass finalLayout); safe no-op when RT mode off, device lacks RT, pass unbuilt, or TLAS null. `RecreateForSwapchain` rebuilds the pass at the new extent; `Cleanup` tears down before VMA shutdown. Tests: 5 (default-mode / setter round-trip / no-TLAS-still-cycles / with-TLAS-on-RT-device `GTEST_SKIP` on non-RT / switch-back-releases-visibility-view); renderer suite 265/265 ✓ (20/20 in RenderLoopTest fixture with RT test skipped on non-RT device).
- [x] 9.5 RTAO — *additive* alongside XeGTAO; Hybrid picks RTAO, Rasterised still uses XeGTAO
  - [x] 9.5.a `rtao.{rgen,rmiss}` + `renderer::RtAoPass` — R8_UNORM AO image (STORAGE + SAMPLED), descriptor set (TLAS 0, depth CIS 1, AO storage 2), RT pipeline via 9.3 with `rt_probe.rchit` as the skipped closest-hit, and `Dispatch(cmd, tlas, depthView, depthSampler, view, proj, radius, frameIndex)` emitting barriers + `vkCmdTraceRaysKHR` + tail transition to SHADER_READ_ONLY_OPTIMAL. 144-byte push block `{ invViewProj, invView, extent, radius, frameIndex }` — invView is carried for tangent-space basis even though the raygen reconstructs the world normal from depth neighbours (cheaper than a normal-GBuffer binding). Cosine-weighted hemisphere sample via Frisvad basis; short ray (caller-supplied radius, default 1 m). Tests: 3 always-run (construct / no-op-on-non-RT / PushConstants==144) + 3 GTEST_SKIP on non-RT (build-allocates / rebuild-replaces-image / dispatch-records-and-submits-clean with triangle-BLAS + 1-instance-TLAS + 16×16 D32_SFLOAT depth); renderer suite 266/266 ✓.
  - [x] 9.5.b `RenderLoop` wire — `SetRtAoInputs(radius)` seeds the AO-only knob; TLAS + view are re-used from the 9.4.b shadow setter (same scene / camera). `GetRtAoImageView()` accessor (null until 9.8 composites). Flipping to `HybridRt` lazy-builds the AO pass alongside the shadow pass; flipping back waits-idle + destroys both. Per-frame dispatch slot is right after `DispatchRtShadow` (depth already in SHADER_READ_ONLY_OPTIMAL; shadow + AO write disjoint images so no inter-dispatch barrier needed). `m_rtAoFrameIndex` increments each dispatch as the PCG seed — the foundation for a future 9.5.c temporal accumulator. `RecreateForSwapchain` rebuilds on resize; `Cleanup` tears down before VMA shutdown. Tests: extended 9.4.b fixtures — `HybridRtModeWithTlasCyclesFrameOnRtDevice` now also asserts `GetRtAoImageView() != null` + calls `SetRtAoInputs(1.0F)`; `SwitchingBackToRasterisedReleasesRtResources` also checks AO view is null after flip. Renderer suite 266/266 ✓.
- [x] 9.6 Window transmission — sun through `IfcWindow` into interior rooms (RT modes only). **Note (2026-04-20):** classical renderer already has this via RP.18 (transmission shadow map + `basic.frag` glass tint), so 9.6 is no longer "the only way" — it's the RT-native, geometrically-truthful version (any-hit attenuator on the shadow ray instead of a second rasterised shadow pass). Should be materially simpler than RP.18 since the light-space depth gate + min-blend RGBA map all collapse into "any-hit multiplies payload by glass tint".
  - [x] 9.6.a `TlasInstance::flags` (`VkGeometryInstanceFlagsKHR`, default 0) plumbed through `TopLevelAS::Build` — OR'd with the always-on `TRIANGLE_FACING_CULL_DISABLE_BIT`. Glass instances opt into `VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR` here so the shadow-ray any-hit shader (9.6.b) runs for them. BLAS stays built with `VK_GEOMETRY_OPAQUE_BIT_KHR`; the per-instance `FORCE_NO_OPAQUE_BIT` overrides that for transmissive windows without needing a second BLAS path. Unit tests: `DefaultInstanceFlagsAreZero` (pre-9.6 semantics preserved) + `BuildWithTransmissiveFlagsSucceedsOnRtDevice` (GTEST_SKIP on non-RT). Renderer suite 266/266 ✓.
  - [x] 9.6.b `shadow.rahit` + `shadow.rchit` + `RtShadowPass` pipeline wire. Payload semantics flipped to a transmission accumulator: raygen seeds `payload = 1` (fully lit), `shadow.rahit` multiplies by a 0.95 pass-through factor and `ignoreIntersectionEXT`s so sun light continues through glass, `shadow.rchit` writes `payload = 0` on the first accepted opaque hit, `shadow.rmiss` is a no-op so accumulated transmission survives to the sky. `shadow.rgen` drops `gl_RayFlagsOpaqueEXT` + `gl_RayFlagsSkipClosestHitShaderEXT`, keeps `TerminateOnFirstHitEXT`. `RtShadowPass::Build` swaps the 9.4.a `rt_probe.rchit` stub for the dedicated `shadow.rchit` and wires `shadow.rahit` via the existing `RayTracingPipelineSettings::anyHitPath`. Pass-through factor is grayscale (R8_UNORM stays) per RP.18.6's architectural-glass-is-neutral-in-transmission finding. Test: extended `DispatchRecordsAndSubmitsCleanlyOnRtDevice` to build a 2-instance TLAS (one opaque + one `FORCE_NO_OPAQUE` glass) so validation exercises both shader groups on RT devices. Renderer suite 266/266 ✓.
- [x] 9.7 Indoor-light sampling — overhead fill respects walls when `indoorLightsEnabled` (RT modes only)
  - [x] 9.7.a `rt_indoor.{rgen,rmiss}` + `renderer::RtIndoorPass` — R8_UNORM visibility image (STORAGE + SAMPLED), descriptor set (TLAS 0, depth CIS 1, visibility storage 2), RT pipeline via 9.3 with `rt_probe.rchit` as the skipped closest-hit, and `Dispatch(cmd, tlas, depthView, depthSampler, view, proj, fillDirWorld)` emitting barriers + `vkCmdTraceRaysKHR` + tail transition to SHADER_READ_ONLY_OPTIMAL. 96-byte push block `{ invViewProj, fillDirWorld, extent, pad×2 }` — same shape as 9.4.a's `RtShadowPass` (indoor fill is geometrically a directional light). Raygen uses the classical sun-style shadow-ray pattern: `OpaqueEXT | TerminateOnFirstHit | SkipClosestHitShader`, seeds payload 0 (occluded), `rt_indoor.rmiss` writes 1 (lit). Walls are hard occluders; glass transmission intentionally not modelled for the fill (it's a room-level ambient proxy, not the sun). Tests: 3 always-run (construct / no-op-on-non-RT / PushConstants==96) + 3 GTEST_SKIP on non-RT (build-allocates / rebuild-replaces-image / dispatch-records-and-submits-clean with triangle-BLAS + 1-instance-TLAS + 16×16 D32_SFLOAT depth); renderer suite 267/267 ✓.
  - [x] 9.7.b `RenderLoop` wire — `SetRtIndoorInputs(fillDirWorld, enabled)` seeds the fill direction + runtime gate; `GetRtIndoorVisibilityView()` accessor (null until 9.8 composites). Flipping to `HybridRt` lazy-builds the indoor pass alongside the shadow + AO passes (the per-frame `enabled=false` gate decides whether a trace runs, so toggling indoor lights at runtime doesn't need a pipeline rebuild); flipping back waits-idle + destroys all three. Per-frame dispatch slot is right after `DispatchRtAo` (depth still in SHADER_READ_ONLY_OPTIMAL, disjoint storage-image writes so no inter-dispatch barrier needed). `RecreateForSwapchain` rebuilds on resize; `Cleanup` tears down before VMA shutdown. Tests: extended 9.4.b/9.5.b fixtures — `HybridRtModeWithTlasCyclesFrameOnRtDevice` now also asserts `GetRtIndoorVisibilityView() != null` + calls `SetRtIndoorInputs({0.2,-1,0.3}, true)` so the dispatch records under validation; `SwitchingBackToRasterisedReleasesRtResources` also checks indoor view is null after flip. Renderer suite 267/267 ✓.
- [ ] 9.8 Hybrid composite — per-contribution raster/RT routing; Rasterised mode stays bit-compatible with pre-Stage-9 output. Sub-split 2026-04-20: `basic.frag` is forward-shaded and bakes sun + PCF + transmission + fill; RT dispatches land after the main pass → AO can composite cleanly in `tonemap.frag`, shadow + indoor need deferred-contribution composite passes.
  - [x] 9.8.a RT AO routing — `tonemap.frag` gained binding 2 (RT AO sampler) + `float useRtAo` push constant (struct now 8 B, 2 floats); `mix(xegtao, rtAo, useRtAo)` selects the AO source. `RenderLoop` enlarged the tonemap descriptor layout/pool to 3 bindings, falls binding 2 back to the XeGTAO view when no RT AO exists (raster path always has a valid sampler). `RefreshTonemapRtRouting` is invoked from `SetRenderMode` + `RecreateForSwapchain` — sets `m_tonemapUseRtAo` + `m_exposurePush.useRtAo` + re-binds binding 2. `IsRtAoSourcedInTonemap()` exposes the routing flag for tests. Tonemap tests updated: push-constant size 4 B → 8 B (+ `useRtAo` offset = 4); `FragmentShaderDoesNotDeclareBindingTwo` → `FragmentShaderDeclaresRtAoBindingTwo`; descriptor-set layout fixture gains binding 2. Renderer suite 267/267 ✓.
  - [ ] 9.8.b RT sun-shadow routing — defer the sun term out of `basic.frag` into a screen-space composite that picks RT visibility (Hybrid) or PCF shadow map (Rasterised). Needs an albedo-or-sun-contribution G-buffer channel. Sub-sub-split 2026-04-20.
    - [x] 9.8.b.1 `basic.frag` push constant grew to `{ uint transparentBit @ 64, uint useRtSunPath @ 68 }` (8 B); `useRtSunPath = 1` skips the whole sun block (lambert + PCF + glass-transmit) so the Stage-9.8.b.2 composite can re-apply sun with RT visibility without double-counting. `main.cpp` pushes the flag per draw: opaque gets `renderLoop.GetRenderMode() == HybridRt ? 1 : 0`; transparent always stays on path 0 (the composite only covers opaque). `BasicShaderTest.FragmentShaderDeclaresTransparentBitPushConstant` widened to assert exactly two `uint` members at offsets 64 + 68. Hybrid RT visibly loses the sun term until 9.8.b.2 lands — expected during the sub-sub-split.
    - [x] 9.8.b.2 `renderer::RtSunCompositePipeline` + `rt_sun_composite.comp` — compute pass wrapping `keyColor * NdotL * transmittedTint * rtVisibility` with the same RP.18 glass-ahead gate as `basic.frag`. Descriptor set 0 is caller-owned with 6 bindings: depth CIS (0), view-space oct-packed normal CIS (1), RT shadow visibility CIS (2), shadow transmission CIS (3), `LightingUBO` (4), HDR storage image (5). 144-byte push block `{ invViewProj, invView, extent, pad×2 }` — `invViewProj` reconstructs worldPos from depth (background `depth >= 1.0` early-outs so the sky ambient from the main pass survives), `invView` lifts the view-space normal G-buffer back to world space so the Lambert dot + the light-space matrix for the transmission sample both run in world coordinates (matches the pre-9.8 `basic.frag` math). Additive `imageLoad/imageStore` on `rgba16f`. Pipeline class mirrors `SsaoXeGtaoPipeline` — ctor takes caller's descriptor-set layout + shader, wraps pipeline layout + compute pipeline. Tests: 3 CPU (`SizeIs144Bytes`, `FieldOffsetsMatchShaderLayout`) + 4 GPU (`ShaderCompiledToSpirv`, `ConstructsWithValidHandles`, `DestructorCleansUp`, `ComputeShaderDeclaresAllSixBindings` walks SPIR-V `OpDecorate` Binding decorations). Renderer suite 270/270 ✓. No RenderLoop wire yet — that's 9.8.b.3.
    - [x] 9.8.b.3 RenderLoop wire — `RtSunCompositePipeline` lazy-built alongside `m_rtShadowPass` in `BuildRtSunComposite` (guarded by `Device::HasRayTracing`); per-swap descriptor sets allocated from a dedicated pool (4× CIS + 1× UBO + 1× storage image), updated by `UpdateRtSunCompositeDescriptors` once the caller has wired the RP.18 shadow-transmission view + sampler + `LightingUbo` buffer via the new `SetRtSunCompositeInputs` API. HDR images gained `VK_IMAGE_USAGE_STORAGE_BIT` (raster path is still bit-compatible — flag is dormant unless the composite dispatch fires). `DispatchRtSunComposite` slots in between `DispatchRtIndoor` and `BuildDepthPyramid`, transitions the per-swap HDR image SHADER_READ_ONLY_OPTIMAL → GENERAL (barrier: COLOR_ATTACHMENT_OUTPUT → COMPUTE_SHADER), records an 8×8-tile dispatch with push constants `{invViewProj, invView, extent}`, then transitions back to SHADER_READ_ONLY_OPTIMAL for the tonemap sampler. `SetRenderMode(HybridRt)` builds after `BuildRtShadowPass` (binding 2 comes from `m_rtShadowPass->GetVisibilityImageView()`); `SetRenderMode(Rasterised)` / `Cleanup` tear down in reverse. `RecreateForSwapchain` rebuilds so descriptor sets point at the new HDR / normal views. `IsRtSunCompositeBuilt()` accessor exposes the state for tests. RenderLoop suite 270/270 ✓.
    - [x] 9.8.b.4 Stage gate — full `ctest -j$(nproc) --output-on-failure` all green (user-run 2026-04-20). Visual check deferred to end of 9.8.d: main.cpp has never been wired to call `SetRenderMode(HybridRt)` / `SetRt{Shadow,Ao,Indoor,SunComposite}Inputs`, so all per-frame RT dispatch gates short-circuit today. 9.8.d lands the main-loop plumbing + TLAS build; visual verification (raster bit-compat + Hybrid RT sun through composite) happens naturally there.
  - [ ] 9.8.c RT indoor-fill routing — same structure as 9.8.b for the fill term. Sub-sub-split 2026-04-20.
    - [x] 9.8.c.1 `basic.frag` push constant grew to `{ uint transparentBit @ 64, uint useRtSunPath @ 68, uint useRtIndoorPath @ 72 }` (12 B); `useRtIndoorPath = 1` skips the indoor-fill lambert so the Stage-9.8.c.2 composite can re-apply it with RT wall-occlusion without double-counting. `main.cpp` pushes both flags per draw: opaque gets `renderLoop.GetRenderMode() == HybridRt ? 1 : 0` for each; transparent always stays on path 0 for both (the composite only covers opaque). Pipeline layouts widened the fragment push range from 8 B → 12 B; `CubeRenderTest` kept in sync as the renderer-side layout-shape regression guard. `BasicShaderTest.FragmentShaderDeclaresTransparentBitPushConstant` widened to assert exactly three `uint` members at offsets 64 + 68 + 72. Hybrid RT visibly loses the indoor fill until 9.8.c.2 lands — expected during the sub-sub-split.
    - [x] 9.8.c.2 `renderer::RtIndoorCompositePipeline` + `rt_indoor_composite.comp` — compute pass wrapping `fillColor * NdotL * rtIndoorVisibility` (directional-only, no transmission — windows are the sun's job, fill is an in-room light). Descriptor set 0 is caller-owned with 5 bindings: depth CIS (0), view-space oct-packed normal CIS (1), RT indoor visibility CIS (2), `LightingUBO` (3), HDR storage image (4). 144-byte push block `{ invViewProj, invView, extent, pad×2 }` kept shape-compatible with `RtSunCompositePipeline::PushConstants` so the 9.8.c.3 RenderLoop wire reuses the sun-composite push setup — `invViewProj` is unused in the shader (no world-position reconstruction needed for directional lambert). Fill contribution multiplied by `fillColorEnabled.w` CPU-side enable flag so a stale visibility image can't leak the indoor fill when the preset is disabled. Pipeline class mirrors `RtSunCompositePipeline`. Tests: 2 CPU (`SizeIs144Bytes`, `FieldOffsetsMatchShaderLayout`) + 4 GPU (`ShaderCompiledToSpirv`, `ConstructsWithValidHandles`, `DestructorCleansUp`, `ComputeShaderDeclaresAllFiveBindings` walks SPIR-V `OpDecorate` Binding decorations). Renderer suite 273/273 ✓. No RenderLoop wire yet — that's 9.8.c.3.
    - [x] 9.8.c.3 RenderLoop wire — `RtIndoorCompositePipeline` lazy-built alongside `m_rtIndoorPass` in `BuildRtIndoorComposite` (guarded by `Device::HasRayTracing`); per-swap descriptor sets allocated from a dedicated pool (3× CIS + 1× UBO + 1× storage image), updated by `UpdateRtIndoorCompositeDescriptors` once the caller has wired the `LightingUbo` via the new `SetRtIndoorCompositeInputs(VkBuffer, VkDeviceSize)` API (separate setter from the sun composite — orthogonal API, no transmission map since the indoor fill ignores glass tint). `DispatchRtIndoorComposite` slots right after `DispatchRtSunComposite`: entry barrier uses `COLOR_ATTACHMENT_OUTPUT_BIT | COMPUTE_SHADER_BIT` as srcStage so the transition is correct regardless of whether the sun composite ran before it. 8×8-tile dispatch with push constants `{invViewProj, invView, extent}`, then back to SHADER_READ_ONLY_OPTIMAL. Additional per-frame gate on `m_rtIndoorEnabled` — the indoor preset toggle skips the composite when the trace above also skipped (avoids compositing stale visibility). `SetRenderMode(HybridRt)` builds after `BuildRtIndoorPass` (binding 2 comes from `m_rtIndoorPass->GetVisibilityImageView()`); `SetRenderMode(Rasterised)` / `Cleanup` tear down in reverse order. `RecreateForSwapchain` rebuilds so descriptor sets point at the new HDR / normal / indoor-visibility views. `IsRtIndoorCompositeBuilt()` accessor exposes the state for tests. RenderLoop suite 273/273 ✓.
    - [x] 9.8.c.4 Stage gate — full `ctest -j$(nproc) --output-on-failure` all green (user-run 2026-04-20). Visual check deferred to 9.8.d: main.cpp has never been wired to call `SetRenderMode(HybridRt)` / `SetRt{Shadow,Ao,Indoor,SunComposite,IndoorComposite}Inputs`, so all per-frame RT dispatch gates short-circuit today. 9.8.d lands the main-loop plumbing + TLAS build; visual verification (raster bit-compat + Hybrid RT sun + indoor-fill through composites) happens naturally there.
  - [x] 9.8.d main.cpp + `RenderQualityPanel` glue (folds into 9.10 UI). `RenderQualitySettings` gained `RenderMode mode` + `bool rayTracingAvailable`; panel grew a "Mode" collapsing header with `Rasterised` / `Hybrid RT` / `Path Traced` radios (`Hybrid RT` greyed out + tooltipped when `Device::HasRayTracing` is false; `Path Traced` permanently disabled as a 9.9 hook). Defaults pinned in `RenderQualityPanelTest` (17/17 ✓). `main.cpp` builds per-scene-mesh BLAS + per-node TLAS right after `SceneUploader::Upload` (strict-no-op on non-RT devices via the AS classes' own probes); glass sub-meshes opt into `FORCE_NO_OPAQUE_BIT_KHR` so the 9.6 any-hit shader attenuates sun through them. Per-frame sync block maps panel mode → `RenderLoop::SetRenderMode` (only calls the setter when it changes; the setter internally `WaitIdle`s + builds/tears-down the RT passes) and, when `HybridRt` is active, feeds `SetRtShadowInputs` (TLAS + `sunPos.dirWorld` + view), `SetRtAoInputs(1.0)`, `SetRtIndoorInputs` (mirror of `PackSunLighting`'s `normalize(0.2,-1,0.3)` + `indoorLightsEnabled`), `SetRtSunCompositeInputs` (RP.18 transmission view + sampler + lighting UBO), `SetRtIndoorCompositeInputs` (lighting UBO). Stage gate: full `ctest -j$(nproc)` 582/582 ✓ (2026-04-20). Subsumes 9.10 (UI switch); 9.9 optional path tracer remains open.
- [ ] 9.9 Optional path tracer — multi-bounce GI accumulator; resets on camera move
- [x] 9.10 UI render-mode switch — folded into 9.8.d's `RenderQualityPanel` "Mode" header (radio + probe-gating tooltip + default `Rasterised`).

## Stage 10 — VR Integration
- [ ] 10.1 OpenXR session lifecycle
- [ ] 10.2 Stereoscopic swapchain
- [ ] 10.3 Stereo rendering
- [ ] 10.4 Controller tracking + input
- [ ] 10.5 Teleport movement
- [ ] 10.6 VR ray interaction
- [ ] 10.7 VR UI panels
- [ ] 10.8 VR comfort features
- [ ] 10.9 Scale model gesture

## Stage 11 — Polish & Release
- [ ] 11.1 Drag-and-drop file loading
- [ ] 11.2 Recent files list
- [ ] 11.3 User preferences
- [ ] 11.4 Linux AppImage packaging
- [ ] 11.5 Windows NSIS installer
- [ ] 11.6 README with screenshots
- [ ] 11.7 Full integration test suite
- [ ] 11.8 Performance profiling pass
- [ ] 11.9 Accessibility review
