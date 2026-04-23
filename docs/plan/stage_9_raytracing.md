## Stage 9 — Ray Tracing (three render modes, additive)

**Pivot 2026-04-23**: this plan was originally "classical raster + hybrid
RT composite". That approach hit persistent
`VUID-vkUpdateDescriptorSets-None-03047` on per-frame descriptor
rewires and was over-engineered for the viewer's actual needs. Rewritten
here as three distinct render modes — no hybrid-composite plumbing.

**Goal**: Expose three independent render modes in `RenderQualityPanel`,
each a complete rendering path:

1. **Rasterised** (default, unchanged) — the current classical renderer:
   shadow maps + PCF, RP.18 transmission shadow map, XeGTAO, SMAA, edge
   overlay. Bit-compatible with today's output.
2. **Ray query** — still the forward-shaded raster pipeline, but
   specific visibility lookups in `basic.frag` (and later
   `ssao_xegtao.comp`) swap for inline `rayQueryInitializeEXT` /
   `rayQueryProceedEXT` against the TLAS. No dedicated RT pipeline, no
   SBT, no composite pass, no per-frame RT descriptor dance. Scope for
   the initial slice: **sun shadow only**.
3. **Ray tracing** — a fully separate rendering pipeline owning its own
   frame lifecycle. Not hybrid-composited. Parked as a placeholder until
   after ray-query mode lands and we have real signal on what this mode
   needs to deliver.

### Why three modes, not two

- **Rasterised stays the default** for the same reasons as before: the
  classical renderer is fast, portable, works on every GPU, and looks
  good. It's the out-of-the-box experience. Nothing is removed.
- **Ray query is the pragmatic upgrade.** For single-ray visibility
  (sun shadow, AO, transmission gate, wall-occluded indoor fill), a
  `rayQuery` call in the existing raster shader replaces the classical
  approximation directly. No new pipeline, no new descriptor pool — the
  fragment shader just gets a TLAS binding and one extra push-constant
  bit. This is the high-leverage mode that closes the shadow-map-bias /
  contact-shadow gap with minimal plumbing.
- **Ray tracing is the exotic mode** for anything `rayQuery` can't do:
  multi-bounce GI, a real path tracer, reflection rays requiring
  recursion or material evaluation at hit-points. Because it's a fully
  separate renderer (not a composite layered onto raster), it's actually
  simpler than the retired hybrid — there's no contract to merge raster
  and RT outputs per contribution.

Sun direction, sky colour, site lat/lon, TrueNorth, date and hour
continue to come from `SunLightingScene` + `RenderQualityPanel` (Stage
RP.16). One authoritative lighting model, read by every mode.

**Sessions**: 1–2 for 9.Q (ray query, sun shadows); 9.RT is open-ended
and deferred.

### Modules involved

- `renderer/` — `Device` capability probe, main descriptor-set layout
  gets a TLAS binding, `basic.frag` gains a `rayQuery` branch
- `scene/` — BLAS-per-mesh + TLAS (already built, 9.1 / 9.2) plus glass
  instance flags (already built, 9.6.a)
- `ui/` — `RenderQualityPanel` gains a three-radio mode selector

### Ground rules for the stage

- **Nothing from the classical renderer is deleted or replaced.**
  Rasterised stays the default and ships bit-compatible output.
- **Every mode sits behind a runtime capability probe.** Ray query is
  gated on `VK_KHR_ray_query`; the Ray tracing radio stays disabled
  with a tooltip until 9.RT delivers.
- **No per-frame descriptor rewires.** The lessons from the retired
  hybrid approach: descriptor sets are wired once at build / swap
  recreate, never per-frame. Ray query obeys this naturally — the TLAS
  binding is part of the main descriptor set, written at scene upload.
- **`SunLightingScene` stays the single source of truth for lighting.**
  All three modes read the same UBO.

### Preserved (done)

- **9.1** — Runtime RT-capability probe + `AccelerationStructure`
  (BLAS-per-mesh). Tests green.
- **9.2** — `TopLevelAS` + `TlasInstance` (transform, BLAS address,
  customIndex, mask, flags). Tests green.

### Retired (historical record in `PROGRESS.md`)

- **9.3 through 9.8.d** — hybrid RT pipeline + SBT + `RtShadowPass` /
  `RtAoPass` / `RtIndoorPass` + sun/indoor composite compute pipelines.
  Code is still in the tree (reverted, unused). Can be deleted once
  9.Q.4 lands and the panel actively routes to Ray query.

### Tasks — 9.Q (ray query, sun shadows)

| # | Task | Test | Output |
|---|------|------|--------|
| 9.Q.1 | `Device::HasRayQuery()` probe; enable `VK_KHR_ray_query` on the logical device when the extension is advertised and the feature bit is true. Chain the feature struct into `vkCreateDevice` the same way `HasRayTracing` does. | Unit: probe reports true on RT-capable device + GTEST_SKIP on non-RT (same pattern as `HasRayTracing` tests). | `renderer/Device.{h,cpp}` |
| 9.Q.2 | Add a TLAS binding to the main descriptor-set layout (optional; raster mode leaves it bound to `VK_NULL_HANDLE`). `SceneUploader` writes the TLAS into the descriptor after the TLAS build completes. | Unit: descriptor set layout binds the TLAS type at the new slot; raster mode still works when TLAS is null. | `renderer/DescriptorSet` + `core/SceneUploader` |
| 9.Q.3 | `basic.frag` gets `#extension GL_EXT_ray_query : require` + `uint useRayQueryShadow` push-constant bit + a ray-query branch replacing the PCF shadow-map sample when the flag is on. Glass transmission (RP.18 tint path) stays active — ray-query shadows and glass tint compose the same way today's PCF + tint do. | Visual test: Ray-query mode produces sharp, bias-free contact shadows that the PCF map misses; no pink/magenta dev banners; Rasterised mode output unchanged. | `basic.frag` |
| 9.Q.4 | `RenderQualityPanel` gains a three-radio selector (`Rasterised` / `Ray query` / `Ray tracing (disabled)`). `main.cpp` maps it onto a new `RenderLoop::RenderMode::RayQuery` and pushes `useRayQueryShadow = 1` per draw when active. Ray tracing radio is permanently disabled with a "future work" tooltip. | Unit: defaults pinned in `RenderQualityPanelTest`; probe-false hides Ray query; probe-true enables it; Ray tracing always disabled. | `ui/RenderQualityPanel` + `src/app/main.cpp` |
| 9.Q.5 | Stage gate — visual check (Ray query shadows match / exceed classical shadow map on the sample scene) + full `ctest -j$(nproc) --output-on-failure`. | Full test suite green; screenshot comparison to classical mode documented in the commit. | — |

### Ordering
9.Q.1 → 9.Q.2 → 9.Q.3 → 9.Q.4 → 9.Q.5 (stage gate).

### Tasks — 9.RT (full ray tracing, parked)

| # | Task | Status |
|---|------|--------|
| 9.RT.1 | Render-mode enum value + panel radio stub (disabled until the pipeline lands). | Parked |
| 9.RT.2..N | Dedicated ray-gen / closest-hit / miss / any-hit pipeline + SBT + accumulation target + frame loop. Scope to be defined after 9.Q lands. | Parked |

9.RT is explicitly out of scope for the immediate work. If it ever
happens, it subsumes the original 9.9 path tracer idea.

### Out of scope (Stage 9)

- RTAO via ray query — deferred. Classical XeGTAO covers it today.
- Window transmission via ray query — deferred. RP.18 classical
  transmission shadow map covers it today. Could be folded into 9.Q.3
  as an any-hit-style walk later.
- Indoor-fill wall occlusion via ray query — deferred. Raster mode
  keeps the cheap directional fill.
- RT reflections, denoisers, dynamic light editing — outside the BIM
  viewer's use case; revisit post-Stage 11 if ever.
- **Retiring XeGTAO, shadow maps, or any other classical renderer
  feature — explicitly NOT in scope. Classical renderer stays the
  default.**

---
