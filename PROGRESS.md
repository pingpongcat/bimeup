# Bimeup — Progress Tracker

## Current Stage: N — Navigation (Blender-style viewport)
## Current Task: N.1 complete — next stage TBD (Stage 7 or Stage 8)

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
- [ ] 7.3 Clipping planes (deferred — after Stage R)
- [ ] 7.4 Plan view
- [ ] 7.5 Section view
- [ ] 7.6 Element visibility by IFC type
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
