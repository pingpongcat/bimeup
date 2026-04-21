## Stage 9 — Ray Tracing (additive, opt-in render mode)

**Goal**: Add a Vulkan ray-traced light-transport render mode *alongside*
the existing rasterised pipeline. The classical renderer stays the
default — XeGTAO, shadow maps, SMAA, edge overlay, all of it — and
nothing is removed or swapped out. RT is a new mode the user picks from
`RenderQualityPanel`; when RT isn't supported by the GPU (or the user
picks Rasterised), the classical path runs exactly as it does today.

Why additive:
- The classical renderer is fast, portable, and the user is happy with
  the look. It must remain the out-of-the-box experience.
- `VK_KHR_ray_tracing_pipeline` + `VK_KHR_acceleration_structure` are
  not guaranteed on all GPUs (Intel iGPUs, older AMD, some mobile).
  Replacing the raster path would break the viewer on those machines.
- RT gives us capabilities raster can't reach (geometric-truth soft
  shadows, window-transmission into rooms, multi-bounce GI in the path
  tracer) — those are the reasons to add it, not to rip out what works.

The sun direction, sky colour, site lat/lon, TrueNorth, date and hour
continue to come from the existing `SunLightingScene` +
`RenderQualityPanel` flow (Stage RP.16) — one authoritative lighting
model, read by every render mode. Artificial interior lights come from
the indoor-preset toggle (also already on `SunLightingScene`).

Render modes after Stage 9:
- **Rasterised** (default, unchanged) — current classical renderer.
- **Hybrid RT** — classical raster primary pass, plus RT passes
  layered *on top* (RT soft sun shadows, RTAO, window transmission,
  indoor-light occlusion). The raster AO / shadow-map outputs are still
  produced; the composite uses whichever input the mode selects, so the
  raster path stays wired and testable.
- **Path Traced** — pure RT accumulator mode for presentation stills;
  resets on camera move, lower interactive framerate. Optional.

**Sessions**: 3–4

### Modules involved
- `renderer/` (RT pipeline, acceleration structures, hybrid composite)
- `scene/` (per-mesh BLAS build, TLAS instance list, transmissive-material tagging)
- `ui/` (render-mode switch in `RenderQualityPanel`; RT modes greyed out with a tooltip when the capability probe says no)

### Ground rules for the stage
- **Nothing from the classical renderer is deleted or replaced**. XeGTAO,
  `ShadowPass`, `EdgeOverlayPipeline`, SMAA, etc. all stay live.
- **All RT code sits behind a runtime capability probe**. On machines
  without RT support, the RT modes are disabled in the UI and no RT
  resources are allocated.
- **Every RT task ships with a "raster path still works" guard**. When
  RT is off the frame must be bit-compatible with today's classical
  output (no new state leaking into the raster path).
- **`SunLightingScene` is the single source of truth for lighting**.
  Both raster and RT read it — no parallel lighting model.

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| 9.1 | Runtime RT-capability probe (`VK_KHR_acceleration_structure`, `VK_KHR_ray_tracing_pipeline`) and BLAS-per-mesh build from `SceneMesh`. Probe is a no-op on unsupported GPUs. | Unit test: probe reports capability booleans on CPU mock; BLAS builds for a triangle mesh, handle valid. | `renderer/AccelerationStructure.{h,cpp}` |
| 9.2 | TLAS build from scene instances (per-`SceneNode` transform + BLAS handle); rebuild on scene change. Only built when an RT mode is active. | Unit test: TLAS instance count matches scene; rebuild hook fires on scene-edit signal. | `renderer/TopLevelAS.{h,cpp}` |
| 9.3 | RT pipeline + SBT (ray-gen, closest-hit, miss, any-hit for transmission). Created lazily the first time RT is enabled. | Unit test: pipeline + SBT build with one ray-gen, one miss, N hit groups; handle validation. | `renderer/RayTracingPipeline.{h,cpp}` |
| 9.4 | **RT sun shadows** — *additive* to the shadow-map path, not a replacement. Uses `SunLightingScene.sun.dirWorld`. Hybrid composite selects RT visibility when RT mode is active; raster-only mode continues to sample the shadow map. | Visual test: Hybrid mode picks up contact shadows + thin-trim shadows the shadow map misses; Rasterised mode output unchanged from today. | `shadow.rgen` + composite path; `ShadowPass` untouched |
| 9.5 | **RTAO** — *additive* to XeGTAO. Cosine-weighted hemisphere sampling, short ray (≤1 m), temporal accumulation on static camera. Composite picks RTAO in Hybrid mode, XeGTAO otherwise. | Visual test: Hybrid AO is visibly more faithful at silhouettes/corners; Rasterised mode output unchanged. | `rtao.rgen`; XeGTAO pipeline untouched |
| 9.6 | **Window-transmission** — sun light passes through `IfcWindow` geometry. Shadow-ray any-hit treats transmissive materials as attenuators (glass tint × transmission factor). Only active in RT modes. | Visual test: interior floor next to a south-facing window is lit in proportion to sun angle (Hybrid/PT modes); Rasterised mode unaffected. | Transmissive material flag in scene + any-hit shader |
| 9.7 | **Indoor-light sampling** — when `indoorLightsEnabled`, cast a shadow ray toward the overhead-fill light so it respects walls. Only active in RT modes; raster still uses the cheap directional fill from RP.16.5. | Visual test: corridor behind a wall no longer lit by the indoor fill in RT mode; Rasterised mode unchanged. | `rt_indoor.rgen` hit group |
| 9.8 | **Hybrid composite** — raster primary (G-buffer + SMAA) + RT visibility/AO/transmission as separable contributions, reassembled in the tonemap pass. Routing flag decides per-contribution which input (raster vs RT) feeds the composite. | Benchmark: Hybrid frame time within budget on reference GPU. Visual test: Rasterised mode produces bit-compatible output with pre-Stage-9 reference renders. | `tonemap.frag` RT input + RenderLoop wire |
| 9.8.e | **NRD integration** — NVIDIA Real-time Denoiser (open-source MIT, cross-vendor) transforms noisy 1-spp RT outputs into smooth AAA-quality results. Add NRD as git submodule, implement motion-vector generation (velocity G-buffer), wire SIGMA denoiser for RT shadow visibility (1-3ms @ 1080p), wire RELAX denoiser for RTAO + indoor-fill visibility (1-2ms each). NRD requires depth + normals + motion vectors + per-frame history buffers for temporal reprojection. | Visual test: RT shadow/AO/indoor-fill outputs are smooth and stable during camera motion; performance overhead stays within 5ms budget @ 1080p. Benchmark: full Hybrid frame (trace + denoise + composite) under 10ms on reference GPU. | `renderer/NrdDenoiser.{h,cpp}`, motion-vector pass, NRD history buffers |
| 9.9 | **Optional path tracer** — separate rendering mode; ray-gen accumulates N bounces into an HDR accumulator; resets on camera move. Uses the same `SunLightingScene` + indoor preset as Hybrid. | Visual test: colour bleed from coloured walls + soft sky fill; accumulator resets on orbit/pan. | `pathtrace.rgen` + accumulator target |
| 9.10 | **UI — render-mode switch** in `RenderQualityPanel`: `{Rasterised (default), Hybrid RT, Path Traced}`. Modes gated on capability probe; disabled modes show a tooltip explaining why. Default stays **Rasterised** on every launch. | UI test: probe-false disables RT modes and keeps Rasterised selected; probe-true enables all three; mode switch recomposes render graph without leaking state into the raster path. | `RenderQualityPanel` Mode section |

### Ordering
9.1 → 9.2 → 9.3 (foundation) → 9.4 + 9.5 in either order (additive hybrid effects) → 9.6 (transmission needs 9.4) → 9.7 (indoor light needs 9.4 shape) → 9.8 (hybrid composite + raster-regression guard) → 9.8.e (NRD denoising polishes RT outputs) → 9.9 (optional PT, parallelisable with 9.10) → 9.10 (UI, already folded into 9.8.d) → stage gate.

### Out of scope (Stage 9)
- RT reflections on glossy / mirror surfaces — cool but not architectural-render-critical, and most BIM materials are matte. Revisit post-Stage 11 if needed.
- Dynamic light editing: sun/time/site stay the single authoritative inputs; no arbitrary light-placement UI in this stage.
- **Retiring XeGTAO, shadow maps, or any other classical renderer feature — explicitly NOT in scope. Classical renderer stays the default.**

### Originally out of scope, now in scope
- **Denoising** — originally deferred ("rely on temporal accumulation + a cheap bilateral; no ML denoiser"), but during 9.8.d testing the noisy 1-spp RT outputs proved unusable for production. Added **9.8.e NRD integration** to transform noisy traces into smooth, AAA-quality results using NVIDIA Real-time Denoiser (open-source, cross-vendor, industry-proven).

---

