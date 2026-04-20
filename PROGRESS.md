# Bimeup — Progress Tracker

## Current Stage: Stage RP (re-opened for RP.16) — Render Polish
## Current Task: RP.16.5 Artificial-indoor preset (overhead fill + ambient boost)

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
  - [ ] RP.16.5 Artificial-indoor preset (overhead fill + ambient boost)
  - [ ] RP.16.6 `RenderQualityPanel` rewrite (Sun header; retire three-point + sky-colour)
  - [ ] RP.16.7 Wire site → panel → renderer in `main.cpp`
  - Ordering: 16.1 / 16.2 / 16.3 parallelizable → 16.4 → 16.5 → 16.6 → 16.7
  - Stage gate at RP.16.7: full `ctest -j$(nproc) --output-on-failure` + visual spot-check on `sample_house.ifc` (noon / golden-hour / night / indoor-preset)

## Stage 9 — Ray Tracing
- [ ] 9.1 Acceleration structures (BLAS)
- [ ] 9.2 Build TLAS
- [ ] 9.3 RT pipeline + SBT
- [ ] 9.4 RT ambient occlusion
- [ ] 9.5 RT soft shadows
- [ ] 9.6 RT reflections
- [ ] 9.7 Hybrid rendering
- [ ] 9.8 Toggle RT/rasterized via UI

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
