# Bimeup — Progress Tracker

## Current Stage: 7 — BIM Viewer Features
## Current Task: 7.6 — Element visibility by IFC type

## Completed Tasks
<!-- Mark tasks as they are done: - [x] 1.1 Description -->

## Stage 1 — Project Bootstrap & Build System
- [x] 1.1 Create root CMakeLists.txt with C++20, module structure, option flags
- [x] 1.2 Add git submodules: googletest, spdlog, glm
- [x] 1.3 Create tools/ module with Log wrapper around spdlog
- [x] 1.4 Create tools/Config — key-value config loader
- [x] 1.5 Create app/main.cpp — initializes logging, prints version
- [x] 1.6 Create build scripts (debug/release for Linux/Windows)
- [x] 1.7 Set up GitHub Actions CI
- [x] 1.8 Enable clang-tidy and sanitizers

## Stage 2 — Platform Layer & Window
- [x] 2.1 Add GLFW submodule, create platform/Window class
- [x] 2.2 Create platform/Input — keyboard + mouse abstraction
- [x] 2.3 Create Vulkan instance + debug messenger
- [x] 2.4 Create Vulkan physical/logical device selection
- [x] 2.5 Create swapchain tied to GLFW surface
- [x] 2.6 Implement render loop: acquire → clear → present
- [x] 2.7 Implement frame timing and FPS tracking

## Stage 3 — Basic Rendering Pipeline
- [x] 3.1 Implement renderer/Buffer — vertex + index with VMA
- [x] 3.2 Implement renderer/Shader — load SPIR-V
- [x] 3.3 Implement renderer/Pipeline — graphics pipeline
- [x] 3.4 Implement renderer/DescriptorSet — UBO binding
- [x] 3.5 Implement renderer/Camera — perspective + orbit
- [x] 3.6 Write basic vertex + fragment shaders
- [x] 3.7 Render a colored cube with orbit camera
- [x] 3.8 Implement renderer/MeshBuffer
- [x] 3.9 Add render mode switching: shaded / wireframe

## Stage 4 — IFC Loading & Internal Scene
- [x] 4.1 Integrate IFC parsing library as submodule
- [x] 4.2 Create ifc/IfcModel
- [x] 4.3 Create ifc/IfcElement
- [x] 4.4 Create ifc/IfcGeometryExtractor
- [x] 4.5 Create ifc/IfcHierarchy
- [x] 4.6 Create scene/SceneNode and scene/Scene
- [x] 4.7 Create scene/SceneMesh
- [x] 4.8 Create scene/AABB
- [x] 4.9 Create scene/SceneBuilder
- [x] 4.10 Implement batching in SceneBuilder
- [x] 4.11 Upload scene meshes to renderer
- [x] 4.12 Implement per-element color from IFC

## Stage 5 — Core Application & Selection
- [x] 5.1 Create core/Application
- [x] 5.2 Create core/EventBus
- [x] 5.3 Define core events
- [x] 5.4 Implement CPU raycasting
- [x] 5.5 Mouse click → ray → selection pipeline
- [x] 5.6 Element highlighting in renderer
- [x] 5.7 Hover highlighting
- [x] 5.8 Multi-selection and clearing

## Stage 6 — ImGui Integration & Basic UI
- [x] 6.1 Integrate Dear ImGui with Vulkan + GLFW backend
- [x] 6.2 Create ui/UIManager
- [x] 6.3 Create ui/Panel interface
- [x] 6.4 Implement ui/HierarchyPanel
- [x] 6.5 Implement ui/PropertyPanel
- [x] 6.6 Implement ui/Toolbar
- [x] 6.7 Implement ui/ViewportOverlay
- [x] 6.8 Wire hierarchy → selection → property panel
- [x] 6.9 Implement UI theme
- [x] 6.10 Wire UI panels (UIManager, Toolbar, Hierarchy, Property, ViewportOverlay) into main.cpp
- [x] 6.11 Fix toolbar fit-to-view zoom + wire hover picking into main.cpp
- [x] 6.12 Enable fillModeNonSolid, fix surface/swapchain destroy order, per-image swapchain semaphores
- [x] 6.13 Replace demo cube with bundled sample.ifc as default startup scene

## Stage 7 — BIM Viewer Features
- [x] 7.0 Fix element selection (nodeId→expressId + batching-aware raycast)
- [x] 7.0a Fix projection double Y-flip (model was rendered upside down, picking desynced) + wire selection → visible vertex-color highlight via MeshBuffer::SetVertexColorOverride
- [x] 7.1 Distance measurement tool (scene::Measure + MeasureTool state machine)
- [x] 7.2 Point snapping (scene::Snap — vertex/edge/face)
- [x] 7.3 Clipping planes
  - [x] 7.3a Math — `renderer::ClipPlane` struct + `SignedDistance`/`ClassifyPoint` helpers + unit tests
  - [x] 7.3b `ClipPlaneManager` (up to 6 planes, add/remove/enable/update) + unit tests
  - [x] 7.3c UBO + shader integration (`ClipPlanesUbo` + `PackClipPlanes` + `basic.frag` discard + descriptor binding 3)
  - [x] 7.3d UI panel to edit planes interactively (`ui::ClipPlanesPanel` — add via ±X/Y/Z presets, enable/disable, edit normal+offset, remove; wired in `main.cpp`)
  - [x] 7.3e ImGuizmo integration — submodule, UIManager view/proj, clip plane Manipulate, ViewManipulate cube
    - [x] 7.3e.1 Add ImGuizmo submodule under `external/ImGuizmo`, compile into `imgui` target, smoke test links `ImGuizmo::IsUsing/IsOver`
    - [x] 7.3e.2 Thread view/proj matrices into `UIManager` (`SetCameraMatrices` / `GetViewMatrix` / `GetProjectionMatrix`), pushed each frame from `main.cpp`
    - [x] 7.3e.3 Translate/rotate gizmo for active clip plane (`renderer::PlaneToTransform` / `TransformToPlane` + `ImGuizmo::Manipulate` in `ClipPlanesPanel`)
    - [x] 7.3e.4 `ViewManipulate` cube wired to `Camera` orientation (`Camera::SetYawPitch` + `renderer::YawPitchFromForward`, called after `uiManager.BeginFrame()` in `main.cpp`)
- [x] 7.4 Plan view (`ui::PlanViewPanel` — Ground Floor / Roof switcher; creates horizontal clip plane at `elevation + cutAbove` and switches camera to top-down ortho framed to scene XZ; default sample swapped to `sample_house.ifc`)
  - [x] 7.4a Honor viewport aspect so ortho frame isn't stretched
  - [x] 7.4b Force yaw=0 on activate so plan isn't rotated; middle-mouse orbit auto-exits plan view
  - [x] 7.4c `Camera::Zoom` scales `m_orthoHeight` so scroll-zoom works in ortho (plan view + any future ortho mode)
  - [x] 7.4d Restore perspective on Deactivate; ImGuizmo ViewManipulate cube also auto-exits; ortho zoom rate doubled
  - [x] 7.4e `core::SelectionCleared` event — non-additive click on empty space clears selection + PropertyPanel (additive miss preserves)
- [x] 7.5 Section view (BRep approach — see `docs/section_cap_next_session.md`)
  - [x] 7.5a `ClipPlane` section fields (`sectionFill`, `fillColor`) + manager setters
  - [x] 7.5b `ClipPlanesPanel` "Section fill" checkbox + ColorEdit4 wired to setters
  - [x] 7.5c `scene::SliceTriangle` — plane/triangle intersection math (`scene/Slicing.{h,cpp}`; 8 unit tests covering all-front/back, coplanar, 2-1 splits, vertex-on-plane straddle/no-straddle, edge-on-plane)
  - [x] 7.5d `scene::SliceSceneMesh` — walk mesh triangles, transform, collect segments
  - [x] 7.5e `scene::StitchSegments` — endpoint hash-grid stitch into closed polygons (`scene/Slicing.{h,cpp}`; 7 unit tests covering empty input, square stitch, shuffled+flipped, two disjoint loops, open polyline drop, epsilon snap, cube slice → 8-vert closed loop)
  - [x] 7.5f `scene::TriangulatePolygon` — ear-clipping (project to dominant 2D plane, CCW-normalize, O(n²) ear-clip; `scene/Slicing.{h,cpp}`; 6 unit tests covering degenerate input, CCW/CW squares, concave L-shape, oblique plane, vertex-preservation)
  - [x] 7.5g `scene::SectionCapGeometry` — per-scene cap vertex buffer + dirty tracking (`BuildSectionCapVertices` walks planes × visible mesh-bearing nodes, slice → stitch → triangulate → tinted vertex list; class wraps `renderer::Buffer` with hash-based no-op `Rebuild` + `MarkDirty`; in scene/ namespace because scene → renderer dep direction forbids a Scene-aware class inside renderer/. 8 unit tests.)
  - [x] 7.5h `section_fill.{vert,frag}` shaders + `renderer::SectionFillPipeline` (flat-color pipeline: position+color vertex input, depth-test LEQUAL/write-off, CULL_NONE, no stencil, single camera UBO set; 4 Vulkan integration tests for shader-SPIR-V presence, construction, MSAA 4×, RAII)
  - [x] 7.5i Wire `SectionCapGeometry` + `SectionFillPipeline` into `main.cpp` (section-fill pipeline built next to shaded/wire, rebuilt on MSAA change; per-frame rebuild is hash-gated and skipped unless ≥1 plane has `enabled && sectionFill`; draws caps after scene, before UI, using main 4-binding descriptor set — shader reads only binding 0)
  - [x] 7.5k Per-element capping for batched IFC meshes (`BuildSectionCapVertices` now partitions batched-mesh triangles by `triangleOwners[t]`, slice→stitch→triangulate per owner, tint = `ownerVertexColor * plane.fillColor`). Open polylines fallback via new `scene::StitchSegmentsDetailed` (returns `StitchResult{closed, open}`) when no closed loop forms for an element. Attached-mesh path unchanged. 2 new unit tests for `StitchSegmentsDetailed` + 2 new tests for per-owner batched caps (per-element colour, invisible-owner skip); visual verification on `../sample.ifc` deferred to the user since this is a GUI change.
  - [x] 7.5j Stage gate — full `ctest --output-on-failure` (215/215 pass; disabled LSan for `test_core` binary since it contains Vulkan-touching Application + SceneUploaderVulkan tests whose graphics-driver leaks at process exit are environmental. Runtime ASan checks remain active across all targets.)
- [ ] 7.6 Element visibility by IFC type
  - [x] 7.6a `Scene::SetVisibilityByType` + `Scene::GetUniqueTypes` + `scene::DefaultHiddenTypes()` (IfcSpace, IfcOpeningElement, IfcGrid, IfcAnnotation) — 5 new unit tests, 124/124 scene tests pass
- [ ] 7.7 Element isolation
- [ ] 7.8 Element transparency override
- [ ] 7.9 Fit-to-view
- [ ] 7.10 First-person navigation

## Stage R — Render Quality
- [x] R.1 Three-point lighting (key/fill/rim) + ImGui Render Quality panel scaffold
- [x] R.2 MSAA (toggle 1×/2×/4×/8×) with resolve pass
- [x] R.3a Shadow mapping — light-space matrix math (`renderer::ComputeLightSpaceMatrix`) + unit tests
- [x] R.3b Shadow mapping — depth-only render pass + shadow map image/sampler
- [x] R.3c Shadow mapping — LightingUbo shadow fields + `ComputePcfShadow` helper + panel wiring (enable/resolution/bias/PCF radius)
- [x] R.3d Shadow mapping — `basic.{vert,frag}` PCF code + `sampler2DShadow` descriptor binding + depth pass executed each frame from `main.cpp`
- [ ] R.4 SSAO (deferred — shadow mapping already covers architectural depth cues; revisit after Stage N)

## Stage N — Navigation (Blender-style viewport)
- [x] N.1a Input mapping — `renderer::ViewportNavigator::ClassifyDrag` (MMB=orbit, Shift+MMB=pan, Ctrl+MMB=dolly) + wire into `main.cpp`
- [x] N.1b Ortho toggle — `Camera::SetOrthographic` + numpad 5 binding
- [x] N.1c Framing — `Camera::Frame(min,max)` + Home (frame-all) / Numpad . (frame-selected, falls back to frame-all) / Shift+C (reset pivot to origin)
- [x] N.1d Axis snaps — `Camera::SetAxisView(AxisView)` + numpad 1/3/7 (Ctrl = opposite side) for Front/Back/Right/Left/Top/Bottom

## Stage 8 — Performance & Large Models
- [ ] 8.1 Async IFC loading with progress
- [ ] 8.2 BVH for scene
- [ ] 8.3 Frustum culling
- [ ] 8.4 LOD generation
- [ ] 8.5 Indirect drawing
- [ ] 8.6 Lazy geometry generation
- [ ] 8.7 Multithreaded geometry processing
- [ ] 8.8 Benchmark with large IFC

## Stage 9 — VR Integration
- [ ] 9.1 OpenXR session lifecycle
- [ ] 9.2 Stereoscopic swapchain
- [ ] 9.3 Stereo rendering
- [ ] 9.4 Controller tracking + input
- [ ] 9.5 Teleport movement
- [ ] 9.6 VR ray interaction
- [ ] 9.7 VR UI panels
- [ ] 9.8 VR comfort features
- [ ] 9.9 Scale model gesture

## Stage 10 — Ray Tracing
- [ ] 10.1 Acceleration structures (BLAS)
- [ ] 10.2 Build TLAS
- [ ] 10.3 RT pipeline + SBT
- [ ] 10.4 RT ambient occlusion
- [ ] 10.5 RT soft shadows
- [ ] 10.6 RT reflections
- [ ] 10.7 Hybrid rendering
- [ ] 10.8 Toggle RT/rasterized via UI

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
