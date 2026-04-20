## Stage RP — Render Polish (non-RT beauty pass)

**Goal**: Lift the forward renderer from "lit + shadowed" to a Godot-editor-viewport look, using only rasterization + screen-space effects. This is the permanent fallback path when ray tracing isn't available (no RTX, Vulkan 1.1-only, headless CI, low-VRAM GPUs) — so everything here must run on any GPU that supports the existing renderer.

**Reference**: Intel XeGTAO-family SSAO + its SSIL variant from `../godot-feature-updated_screen_space_shaders/` (Godot 4.4 `ssao.glsl` / `ssil.glsl`, clayjohn's Vulkan+compute port).

**Sessions**: 5–8 (several sub-tasks are independent; can be interleaved with other work)

### Motivation

- Flat `vec3 ambient` leaves slabs and ceilings identical — no "ground vs sky" read in interior views.
- No SSAO means corners/crevices/window-wall junctions look pasted on.
- B8G8R8A8_UNORM swapchain write is doing implicit gamma, so lighting math accumulates in sRGB-ish space — looks muddy in shadowed areas.
- Selection is fill-only (vertex-color override) — hard to read through transparency and ghosted PoV mode.
- A proper HDR+tonemap+post-process framework is also a prerequisite for RT later: RT's output has to land somewhere with enough dynamic range.

### Scope decisions (2026-04-18)

**In scope:**
- HDR render target + ACES/AgX tonemap pass (prerequisite for everything else)
- MRT normal attachment (prerequisite for SSAO/SSIL/outlines)
- Depth pyramid (prerequisite for SSAO/SSIL)
- SSAO (Intel XeGTAO port) — the single biggest visible lift
- SSIL (Intel XeGTAO SSIL port) — one-bounce indirect colour, the "Godot look" closer
- Hemisphere sky ambient (3-tone: zenith / horizon / ground) replacing flat ambient
- Screen-space outline for selected + hovered elements
- FXAA post (on top of MSAA)
- Depth-based fog
- Small-kernel bloom

**Out of scope:**
- Full PBR / textured materials — IFC meshes are flat-colored; adds complexity with no visible gain on our asset class.
- SSR — interior BIM scenes rarely have large mirror-like surfaces worth the cost.
- Deferred shading — our scale doesn't need it; stay forward.
- TAA — overkill relative to FXAA on flat-shaded scenes; revisit if we add PBR/specular later.
- Volumetric fog, SDFGI, VoxelGI — out of budget; SSIL is the indirect-light story here.

### Modules involved
- `renderer/` (post-process framework, new compute pipelines, MRT attachment, depth pyramid, tonemap)
- `ui/` (RenderQualityPanel — sliders for each new effect)
- `assets/shaders/` (new GLSL: tonemap, ssao, ssil, outline, fxaa, fog, bloom; updates to `basic.frag`)

### Tasks

| # | Task | Test | Output |
|---|------|------|--------|
| RP.1 | Hemisphere sky ambient — replace `LightingUBO.ambient` (vec3) with `skyZenith` + `skyHorizon` + `skyGround` (3× vec3) blended by `dot(n, up)`. Panel colour pickers for each. | Unit: C++ mirror `ComputeHemisphereAmbient(n, zenith, horizon, ground)` matches shader maths on cardinal normals (±X/±Y/±Z). | `renderer::Lighting.h`, `basic.frag`, panel |
| RP.2 | Post-process framework — off-screen HDR colour attachment (R16G16B16A16_SFLOAT), main-pass render targets unhooked from swapchain, final tonemap pass (ACES-fitted) blits to sRGB swapchain. `BIMEUP_ENABLE_HDR` default on, off = direct swapchain write (fallback for debug). | Unit: ACES curve at known HDR values (0, 0.18, 1.0, 10.0) matches reference; pipeline-build test for tonemap. | `renderer/PostProcessChain.{h,cpp}`, `tonemap.{vert,frag}` |
| RP.3 | MRT normal G-buffer — add R16G16_SNORM (octahedron-packed) normal attachment to the main render pass, `basic.frag` emits `outNormal`. Prepares inputs for SSAO/SSIL/outline. | Unit: C++ mirror `OctPackNormal` / `OctUnpackNormal` round-trip on sphere samples; pipeline-build test for updated `basic` pipeline with 2 colour attachments. | `basic.frag`, updated `RenderLoop::CreateRenderPass` |
| RP.4 | Linear depth + depth pyramid — one-off per-frame compute pass that reads the main depth attachment, linearises to view space, and builds a mip chain (R16_SFLOAT or R32_SFLOAT, 4 mips). Helper `ReconstructViewPosFromDepth(uv, linearDepth, invViewProj)`. | Unit: `LinearizeDepth(nonLinear, near, far)` + `ReconstructViewPos` mirrors on known projections. | `renderer/DepthPyramid.{h,cpp}`, `depth_linearize.comp`, `depth_mip.comp` |
| RP.5 | SSAO compute pass — port Godot/Intel `ssao.glsl` (adaptive-base + main). Inputs: depth pyramid, normal attachment. Output: R8 AO term at half-res. Separable blur. Composite via multiply in tonemap pass (or a cheap `ssao_composite.frag`). Panel: enable / quality preset (0–3) / radius / intensity / shadow power. | Unit: hemisphere-kernel + edge-packing helpers have C++ mirrors tested on known inputs; pipeline-build test for SSAO compute + blur. Visual: contact darkening at slab/wall junctions, corners, window reveals on `sample_house.ifc`. | `renderer/SsaoPass.{h,cpp}`, `ssao_main.comp`, `ssao_blur.comp`, `ssao_composite.frag` |
| RP.6 | Selection outline — new graphics pass after tonemap, samples normal + depth + a single-bit "selected/hovered" stencil written during the main pass (write `1` for selected, `2` for hovered). Sobel-on-id + depth-discontinuity fallback; 2-px outline, configurable colour. | Unit: `SobelMagnitude` + `EdgeFromStencil` helpers on a known 3×3 patch. Visual: crisp outline around selected/hovered element that reads through transparency and PoV ghosting. | `renderer/OutlinePass.{h,cpp}`, `outline.frag` |
| RP.7 | SSIL compute pass — port Godot/Intel `ssil.glsl`. Inputs: depth pyramid, normal attachment, previous-frame HDR colour, reprojection matrix. Output: R16G16B16A16 indirect colour at half-res. Separable blur. Composite: add to `lit` before tonemap. Panel: enable / radius / intensity / normal-rejection amount. | Unit: reprojection matrix helper + normal-rejection weight on known inputs. Visual: coloured bleed (red wall → grey floor tint, wooden floor → white ceiling tint) on `sample_house.ifc`. | `renderer/SsilPass.{h,cpp}`, `ssil_main.comp`, `ssil_blur.comp` |
| RP.8 | FXAA post — single compute/fullscreen-fragment pass between outline and swapchain blit. Panel toggle + "quality" (LOW/HIGH). | Unit: luminance calc matches reference; pipeline-build test. Visual: subpixel shimmer on slab edges is gone at MSAA=1. | `renderer/FxaaPass.{h,cpp}`, `fxaa.frag` |
| RP.9 | Depth fog — additive fragment work inside tonemap pass, `fogColour` + `fogStart` + `fogEnd` + `fogEnabled` in the LightingUBO tail. Panel: enable, colour, distance range. | Unit: `ComputeFog(viewZ, start, end)` mirror. Visual: gentle distance fade improves depth perception on `Ifc2x3_SampleCastle.ifc`. | `tonemap.frag`, `renderer/Lighting.h` |
| RP.10 | Small-kernel bloom — 3-mip downsample + Kawase/dual-filter blur + composite in tonemap. Only triggers on HDR pixels > `bloomThreshold` (defaults: threshold 1.0, intensity 0.04 — subtle). Panel toggle + threshold + intensity. | Unit: downsample/upsample weight helpers. Visual: soft glow around sunlit windows / artificial lights when we add them. | `renderer/BloomPass.{h,cpp}`, `bloom_down.frag`, `bloom_up.frag` |

### Expected APIs after Stage RP

```cpp
// renderer/include/renderer/PostProcessChain.h
namespace bimeup::renderer {
    struct PostProcessSettings {
        bool ssaoEnabled = true;
        int  ssaoQuality = 2;     // 0=lowest … 3=adaptive
        float ssaoRadius = 0.5f;
        float ssaoIntensity = 1.5f;
        float ssaoShadowPower = 1.5f;

        bool ssilEnabled = false;   // off by default — HDR+SSAO is already a big win
        float ssilRadius = 1.0f;
        float ssilIntensity = 0.5f;
        float ssilNormalRejection = 1.0f;

        bool outlineEnabled = true;
        glm::vec3 outlineColor{1.0f, 0.6f, 0.1f};
        float outlineThickness = 2.0f;

        bool fxaaEnabled = true;
        int   fxaaQuality = 1;      // 0=LOW, 1=HIGH

        bool fogEnabled = false;
        glm::vec3 fogColor{0.5f, 0.55f, 0.6f};
        float fogStart = 20.0f;
        float fogEnd = 120.0f;

        bool bloomEnabled = false;
        float bloomThreshold = 1.0f;
        float bloomIntensity = 0.04f;
    };

    class PostProcessChain {
    public:
        PostProcessChain(const Device&, VkExtent2D, VkFormat hdrFormat);
        void Resize(VkExtent2D);
        // Records compute + fragment passes between the main pass and the swapchain blit.
        void Record(VkCommandBuffer, const CameraUbo&, const PostProcessSettings&);
        VkImage  GetHdrTarget()    const;
        VkImageView GetHdrTargetView() const;
    };
}

// renderer/include/renderer/Lighting.h  (additions)
struct HemisphereAmbient {
    glm::vec3 zenith{0.55f, 0.60f, 0.70f};
    glm::vec3 horizon{0.60f, 0.60f, 0.60f};
    glm::vec3 ground{0.25f, 0.22f, 0.20f};
};

struct FogSettings {
    bool enabled = false;
    glm::vec3 color{0.55f, 0.60f, 0.70f};
    float start = 20.0f;
    float end   = 120.0f;
};

glm::vec3 ComputeHemisphereAmbient(const glm::vec3& normal,
                                   const HemisphereAmbient& sky);
```

### Stage RP task ordering

RP.1 (sky ambient) has no renderer-architecture dependency — ship it first for an immediate visible win. RP.2 (HDR framework) unlocks everything after. RP.3/4 prepare SSAO inputs. RP.5 is the big-ticket task. RP.6 (outlines) can land in parallel with RP.5 once RP.3 is done. RP.7 (SSIL) is the heavyweight; treat as its own gate. RP.8/9/10 are independent polish tasks that can land in any order after RP.2.

```
RP.1 ── stand-alone
RP.2 ── unlocks → RP.8, RP.9, RP.10
        │
        └── RP.3 ── unlocks → RP.5, RP.6
                         │
                         └── RP.4 ── required for → RP.5, RP.7
                                         │
                                         └── RP.5 ── required for → RP.7
```

### RP.12 Addendum — Architectural rendering polish (added 2026-04-19)

bimeup is a BIM viewer, not a game renderer. Three concrete issues surfaced once the full RP chain shipped:

1. **Bloom is noise on flat-shaded IFC geometry.** No self-emissive surfaces, no sun disc, no HDR point lights — bloom only produces halos that obscure edges.
2. **SSIL reads as a "massive bloom effect."** Its additive output can drive HDR well above 1.0 on large wall panels, which then feeds the bloom threshold and doubles the glow. Even standalone (bloom off) the default intensity 0.5 produces a soft glow rather than the intended colour-bleed.
3. **SSAO is invasive, noisy, and leaks through windows.** Translucent glass still writes depth + oct-packed normal, so SSAO treats glazing as a solid occluder and darkens the walls/floors behind window frames. The 64-sample hemisphere kernel is also visibly noisy after the 7-tap separable blur.

**Scope**: retire bloom; fix SSIL (clamp + transparency gate + sane defaults); fix SSAO (transparency gate + tuned defaults now, algorithm upgrade later).

**Tasks**

| # | Task | Test | Output |
|---|------|------|--------|
| RP.12a | Retire bloom. Delete `BloomDownPipeline`, `BloomUpPipeline`, `bloom*` shaders + math module, `BloomSettings`, `SetBloomParams`, the 3-mip chain in `RenderLoop`, tonemap binding 4 + `bloomIntensity`/`bloomEnabled` push-constant fields, panel "Bloom" section. Shrink `TonemapPipeline::PushConstants` 36 → 28 bytes (`exposure` slides from offset 32 → 24). | Drop `BloomMath*`, `Bloom*PipelineTest`, `RenderLoopTest.Bloom*`. Update `TonemapPipelinePushConstantsTest` to pin the new 28-byte contract. `ctest --output-on-failure` green. | `renderer/BloomPass.*`, `assets/shaders/bloom*`, `RenderLoop.{h,cpp}`, `TonemapPipeline.{h,cpp}`, `tonemap.frag`, `RenderQualityPanel.{h,cpp}`, `main.cpp` |
| RP.12b | Transparency bit in stencil G-buffer. Existing `R8_UINT` stencil carries 0/1/2 (background/selected/hovered). Add bit 4 = "transparent surface" so values become `{0,1,2,4,5,6}`. `basic.frag` fragment-stage push range grows 4 → 8 bytes with a new `uint transparentBit` field (0 or 4); transparent-pipeline draws push `4`, opaque draws push `0`. `outline.frag`'s `edgeFromStencil` masks out bit 4 before the max-reduction so outlines still draw on glass. CPU `EdgeFromStencil` mirror updated in the same commit. | Unit: extend `OutlineEdgeTest` with patches mixing the new transparent bit (outline survives on glass; transparent-only background → no edge). Vulkan: `RenderLoopTest.StencilCarriesTransparentBit` cycles a frame that pushes the bit and re-reads mip 0 of the stencil image. | `basic.frag`, `outline.frag`, `OutlineEdge.{h,cpp}`, `main.cpp` (transparent-draw push) |
| RP.12c | SSIL transparency-gate + clamp + toned-down defaults. `ssil_main.comp` samples the stencil G-buffer at each tap; taps flagged transparent contribute 0 (translucent walls don't bounce colour into the room). Post-accumulation clamp `clamp(indirect, 0, ssilMaxLuminance)` per channel — prevents wide-area glow even when 64 taps all agree. New panel slider "Max luminance" (0.1–2.0, default 0.5); default intensity 0.5 → 0.15. CPU mirror `renderer::SsilClampLuminance(rgb, cap)` with per-channel clamp test. | Unit: `SsilClampLuminance` test (below/at/above cap across three channels, cap = 0 zeroes output). Vulkan: existing `SsilDispatchedDuringFrame` retargeted at the new descriptor layout (add stencil CIS binding). | `ssil_main.comp`, `SsilPipeline.{h,cpp}`, `SsilMath.{h,cpp}`, `RenderLoop.{h,cpp}`, `RenderQualityPanel.{h,cpp}`, `main.cpp` |
| RP.12d | SSAO transparency-gate + tuned architectural defaults. `ssao_main.comp` samples the stencil G-buffer: centre pixel flagged transparent → early-out AO = 1.0 (glass asks for no AO); sample taps flagged transparent contribute 0. Radius default 0.5 → 0.35 m (contact-AO only, not room-scale dimming), intensity 1.0 → 0.5. Panel ranges unchanged. | Vulkan: existing `SsaoDispatchedDuringFrame` retargeted at the new descriptor layout (add stencil CIS binding). No new CPU math — transparency gate is a pure shader change. | `ssao_main.comp`, `SsaoPipeline.{h,cpp}`, `RenderLoop.{h,cpp}`, `RenderQualityPanel.{h,cpp}` |
| RP.12e | SSAO → XeGTAO port. Replace the Chapman hemisphere-kernel pass with horizon-based integration (Intel XeGTAO, 2022) — quieter at fewer taps, better temporal stability on top of the blur, matches the RP.5 PLAN's original target. Reuses the existing depth pyramid + normal G-buffer + RP.12d transparency gate. Likely multi-session; sub-split at kickoff as **RP.12e.1** CPU math mirrors (slice-sample trigonometry + cosine-lobe integral) → **RP.12e.2** `ssao_xegtao.comp` + `SsaoXeGtaoPipeline` → **RP.12e.3** swap into `RenderLoop`, retire the old `SsaoKernel`/`ssao_main.comp`. Adaptive-base temporal refinement from the paper skipped on first pass (no motion-vector infrastructure). | Unit: horizon-integration math mirror vs. analytical cosine lobe on known configs. Vulkan: pipeline-build + full-frame cycle tests mirroring today's `SsaoPipelineTest`. | `renderer/XeGtaoMath.{h,cpp}`, `ssao_xegtao.comp`, `SsaoXeGtaoPipeline.{h,cpp}`, `RenderLoop.{h,cpp}` |

**Ordering**

```
RP.12a ── stand-alone (pure deletion)
RP.12b ── prerequisite for → RP.12c, RP.12d
                                     │
RP.12c ── independent of 12d ────────┤
                                     │
RP.12d ── independent of 12c ────────┘

RP.12e ── after RP.12d (reuses transparency-gate contract)
```

If RP.12c tuning can't deliver useful colour-bleed, a follow-up **RP.12f — Retire SSIL** is the escape hatch (symmetric to RP.12a's bloom removal).

---

### RP.16 — Site-driven sun lighting (added 2026-04-20)

bimeup is an architectural BIM viewer, not a product-visualisation tool. Three-point lighting (key/fill/rim) is the wrong grammar for it, and the RP panel forces the user to tune six direction sliders + six colour pickers + intensities to get a passable architectural look. Swap the model for a single physically-plausible sun driven by `(date, time, site lat/long, TrueNorth)`, with sky ambient derived from sun elevation and an optional "artificial interior lights" preset for indoor views. This is also the correct groundwork for Stage 9 RT — a single directional sun + HDR sky dome is the canonical RT primary-light setup, and `SkyColor` can later feed an RT miss shader unchanged.

**Scope**
- **In:** solar-position math (NOAA-simplified), elevation-keyed sky-colour LUT, IfcSite location extraction (RefLatitude/RefLongitude/RefElevation + TrueNorth), `SunLightingScene` + packer into the existing `LightingUbo`, artificial-indoor preset (overhead fill + ambient boost), panel rewrite, main.cpp wiring.
- **Out:** Preetham/Hosek analytical sky (deferred — 5-key LUT is enough for Stage RP), user-placed interior lights (out of UX budget — toggle preset only), HDR sky texture (Stage 9 RT concern).

**Reused contract**: `LightingUbo` shape (224 B std140) does **not** change — sun lands in the key slot, indoor fill lands in the fill slot, rim slot is zeroed. `basic.frag` and shadow map wiring (`ComputeLightSpaceMatrix` fed by sun direction) are untouched.

**Modules involved**
- `renderer/` (new `SunPosition`, `SkyColor`; rewritten `Lighting`)
- `ifc/` (new `IfcSiteLocation`)
- `ui/` (rewritten `RenderQualityPanel` — three-point + sky-colour sections retired)
- `app/main.cpp` (wire site → panel → renderer on model load)

**Tasks**

| # | Task | Test | Output |
|---|------|------|--------|
| RP.16.1 | **Solar-position math** — `renderer/SunPosition.{h,cpp}` exposes `ComputeSunDirection(julianDay, latitudeRad, longitudeRad, trueNorthRad) → {dirWorld, elevation, azimuth}` using the NOAA-simplified algorithm (good to ~0.1° for viewer purposes). Direction follows the existing convention (points FROM the sun toward the scene). | Unit: known reference values — Greenwich equinox noon → elevation ≈ 90°−lat; Warsaw midsummer noon ≈ 61°; winter noon low sun; TrueNorth offset rotates azimuth 1:1. Bit-exact determinism. | `renderer/SunPosition.{h,cpp}`, `SunPositionTest.cpp` |
| RP.16.2 | **Sky colour from sun elevation** — `renderer/SkyColor.{h,cpp}` exposes `ComputeSkyColor(sunElevationRad) → {zenith, horizon, ground, sunColor}` via a 5-key LUT (night / civil-twilight / golden / day / zenith-sun). Lerps across keys; sunColor warm at low elevation, white at high. | Unit: five key elevations produce expected triples; C⁰ continuity between keys (no slope discontinuities at key boundaries). | `renderer/SkyColor.{h,cpp}`, `SkyColorTest.cpp` |
| RP.16.3 | **IfcSite location extraction** — `ifc/IfcSiteLocation.{h,cpp}` exposes `ExtractSiteLocation(IfcModel&) → std::optional<SiteLocation>` with `{latitudeRad, longitudeRad, elevationM, trueNorthRad}`. Parses `IfcSite.RefLatitude`/`RefLongitude` (IfcCompoundPlaneAngleMeasure DMS tuple) and `RefElevation`; parses `TrueNorth` from the model's `IfcGeometricRepresentationContext` (Direction2D in the XY plane → rad). Returns `std::nullopt` when any required field is missing; caller uses a default (Warsaw, TrueNorth 0). | Unit: sample IFCs with full site metadata, partial metadata (returns nullopt), and none. | `ifc/IfcSiteLocation.{h,cpp}`, `IfcSiteLocationTest.cpp` |
| RP.16.4 | **SunLightingScene + pack** — new `renderer::SunLightingScene{dateTimeUtc, siteLocation, trueNorthRad, indoorLightsEnabled, exposure, shadow}`. `PackSunLighting(scene) → LightingUbo` derives sun direction (16.1), sky triple + sun colour (16.2), fills the existing UBO: key = sun, fill = zero (filled by 16.5 when indoor is on), rim = zero, ambient = sky. Retire `LightingScene` / `MakeDefaultLighting` / three-point API. | Unit: two reference (time, location) pairs produce the expected full UBO; bit-exact idempotency on repeated packs. | `renderer/Lighting.{h,cpp}`, rewritten `LightingTest.cpp` |
| RP.16.5 | **Artificial-indoor preset** — when `indoorLightsEnabled`, `PackSunLighting` writes an overhead soft-white fill into the fill slot (direction ≈ `(0.2, -1, 0.3)` normalised, colour ≈ warm white, intensity 0.5) and scales the ambient (zenith × 0.7, horizon × 0.9, ground × 1.2) to approximate bounce light from interior surfaces. Sun + shadow untouched. | Unit: flipping `indoorLightsEnabled` only mutates the fill slot + ambient triple; key slot + shadow params are bit-identical. | `renderer/Lighting.cpp` (additions), `LightingTest.cpp` (cases) |
| RP.16.6 | **Panel rewrite** — retire the "Three-point lighting" and sky-colour sections of `RenderQualityPanel`. New layout under a "Sun" `CollapsingHeader`: date picker (MM/DD), time slider (0–24 h, UTC offset derived from longitude), "Use site geolocation" checkbox (on → lat/long read-only, sourced from site; off → manual sliders), "Artificial interior lights" checkbox. "Tonemap", "SMAA", and "Shadows" sections retained with the same knobs. | Update `RenderQualityPanelTest` to pin the new `RenderQualitySettings` shape — drop three-point + sky-colour assertions, add sun/date/time/indoor/site-geo assertions. | `ui/RenderQualityPanel.{h,cpp}`, `RenderQualityPanelTest.cpp` |
| RP.16.7 | **Wire site → panel → renderer in `main.cpp`** — on `ModelLoaded`, call `ifc::ExtractSiteLocation` and push the result (or the default) into the panel. Per frame: panel emits `SunLightingScene` → `PackSunLighting` → existing `LightingUbo` upload path → existing `ComputeLightSpaceMatrix(sunDir, …)` for shadows. Retire the old `MakeDefaultLighting` call. | Stage-gate `ctest -j$(nproc) --output-on-failure` green. Visual spot-check on `sample_house.ifc`: noon vs. golden-hour vs. night renders plausibly; indoor preset visibly brightens interior without washing out the sun contribution. | `app/main.cpp`, ripples in existing integration tests |

**Ordering**

```
RP.16.1 ─┐
RP.16.2 ─┼── RP.16.4 ── RP.16.5 ── RP.16.6 ── RP.16.7
RP.16.3 ─┘
```

16.1 / 16.2 / 16.3 are parallelizable one-shot sessions. 16.4 is the integrator and is on the borderline for size — flag at kickoff for potential sub-split into 16.4.a (new API surface, tests) + 16.4.b (old API removal). 16.5 / 16.6 / 16.7 are small.

**Risks**
- `IfcCompoundPlaneAngleMeasure` parsing via web-ifc: DMS tuple + sign convention (S/W negative) — easy to get wrong. Covered by `IfcSiteLocationTest` reference cases.
- `TrueNorth` rotation sign (CW vs. CCW from +Y on the XY ground plane) — covered by `SunPositionTest.TrueNorthOffset`.
- Users lose manual key/fill direction control. Acceptable per stated goal ("significantly simplify the entire rendering interface").

**Stage framing**: Stage RP has been "closed" twice already (after RP.13b, after RP.15b). RP.16 re-opens it a third time rather than standing up a new "Stage L — Lighting". Rationale: this is the same pattern as RP.13/14/15 — architectural-viewer cleanup that couldn't be known at original RP kickoff. Stage 9 (RT) still follows after RP.16 closes.

---

### RP.17 — Feature-edge overlay (added 2026-04-20)

The current "Wireframe" render mode is a global `VK_POLYGON_MODE_LINE` toggle: every triangulation seam is drawn, which turns any non-trivial IFC into a tangle of diagonal lines and is useless both visually and for measurement. Replace it with a proper **feature-edge overlay** — the CAD-viewer approach: extract boundary edges + dihedral-angle-thresholded edges per mesh at scene-build time, draw them as a dedicated line pass on top of the shaded surface. Gives the model crisp silhouettes, exposes the architecturally meaningful edges (wall corners, slab tops, opening reveals), and sets up `scene::Snap` for edge-snap candidates later.

**Scope**
- **In:** CPU edge extraction (dihedral-angle filter, position welding), line buffers on `SceneMesh`, `SceneBuilder` wiring, `EdgeOverlayPipeline` (line topology, depth ≤, polygon-offset, alpha), GLSL `edge_overlay.{vert,frag}`, RenderLoop draw-pass wiring, Toolbar "Edges" toggle (off / on) replacing the current "Wireframe" radio, retire `renderer::RenderMode::Wireframe` + the wireframe graphics pipeline in `main.cpp`.
- **Out:** Geometry-shader / tessellation-shader edge detection (CPU extraction is deterministic, cacheable, and snap-ready), wide-line emulation via quad expansion (use `VK_LINE_WIDTH` + `wideLines` feature first; escalate only if needed), screen-space edge enhancement (Option B in the design discussion — may revisit as a separate effect), edge colour per-IFC-type (single flat colour for now).
- **Deferred:** 17.6 (feed edges into `scene::Snap`) is optional — keep it as a follow-up if 17.1–17.5 ship cleanly.

**Third-party libraries evaluated**
- `meshoptimizer`: already widely used, but no feature-edge API (LOD / simplification / overdraw only). Not a fit.
- `libigl`: has `sharp_edges`, but Eigen-heavy and would pull a large dep for ~80 lines of algorithm.
- `CGAL`: overkill + licensing friction.
- **Decision**: custom implementation. Matches codebase culture (small focused modules with unit tests).

**Modules involved**
- `scene/` (new `EdgeExtractor`, `SceneMesh` gains a line-buffer field, `SceneBuilder` wires extraction)
- `renderer/` (new `EdgeOverlayPipeline`, `edge_overlay.{vert,frag}`, `RenderLoop` draw-pass addition, retire `RenderMode::Wireframe`)
- `ui/` (Toolbar: "Wireframe" radio → "Edges" toggle; `main.cpp` wires the toggle through to the RenderLoop)

**Tasks**

| # | Task | Test | Output |
|---|------|------|--------|
| RP.17.1 | **`scene::EdgeExtractor`** — `ExtractFeatureEdges(positions, indices, config) → {positions, indices}` producing a line-list. Welds vertices by `config.weldEpsilon` (default 1e-5) before building edge→triangle adjacency. Keeps (a) boundary edges (1 triangle) and (b) edges where `acos(dot(n_a, n_b)) ≥ config.dihedralAngleDegrees` (default 30°). Drops co-planar seams. Triangle normals computed from welded positions; degenerate triangles (zero-area) skipped. | Unit: cube → 12 unique line segments (not 36); regular tetrahedron → 6; coplanar quad (two triangles sharing a diagonal) → 4 (diagonal dropped); boundary-open strip → 2 long edges + 2 short (the seam between strips is co-planar → dropped); degenerate input → empty output. Determinism: output is stable across runs. | `scene/EdgeExtractor.{h,cpp}`, `tests/scene/test_edge_extractor.cpp` |
| RP.17.2 | **`SceneMesh` line buffer + builder wiring** — add `SetEdgeIndices(std::vector<uint32_t>)` / `GetEdgeIndices()` + `GetEdgeIndexCount()` to `SceneMesh`. `SceneBuilder` runs `ExtractFeatureEdges` on each source mesh, offsets the returned indices by the batch-vertex-base, and appends into the batch's edge-index buffer. Empty if the batch has no extractable edges. | Unit: builder fed a single cube mesh produces a `SceneMesh` with `GetEdgeIndexCount() == 24` (12 edges × 2). Builder fed two meshes in a batch produces concatenated edge indices with correct vertex-base offsets. | `scene/SceneMesh.{h,cpp}`, `scene/SceneBuilder.{h,cpp}`, tests in `test_scene_mesh.cpp` + `test_scene_builder.cpp` |
| RP.17.3 | **`renderer::EdgeOverlayPipeline`** — new graphics pipeline: `VK_PRIMITIVE_TOPOLOGY_LINE_LIST`, `VK_POLYGON_MODE_FILL` (not LINE — we *are* lines), depth-test `VK_COMPARE_OP_LESS_OR_EQUAL`, depth-write off, polygon-offset `{constant: -1.0, slope: -1.0}` so lines win the z-fight with the surface, alpha-blend on (for the opacity knob). Reuses the existing `basic` vertex layout (position only sampled in shader; normal/colour ignored). | Unit: pipeline-build test in `tests/renderer/` — creates the pipeline under `HeadlessRenderer`, asserts handle non-null and VkResult success; no visual assertion yet. | `renderer/EdgeOverlayPipeline.{h,cpp}`, update `tests/renderer/CMakeLists.txt` + a pipeline-build test |
| RP.17.4 | **`edge_overlay.{vert,frag}` + RenderLoop draw** — minimal shaders: vert applies `uniform.viewProj * position`, frag writes `vec4(edgeColor.rgb, edgeColor.a)` from a push-constant (16 B: `vec4 color`). RenderLoop gets a new draw block inside the existing main-pass command buffer, ordered **after opaque, after section caps, before transparent**. Uses the per-batch edge-index buffer from 17.2. Skipped when the "Edges" toggle is off. | Visual: run the app on `sample.ifc` — wall corners, slab edges, window reveals are crisp single lines (no more X-diagonals through rectangular faces). Unit: smoke test `RenderLoopTest` renders a frame with edges on and compares a checksum or pixel count of the edge-overlay region; if that's too flaky, just assert no validation layer errors. | `assets/shaders/edge_overlay.{vert,frag}`, `renderer/RenderLoop.cpp` (draw wire-up), `renderer/RenderLoop.h` (toggle setter) |
| RP.17.5 | **Toolbar "Edges" toggle + `main.cpp` cleanup** — retire `renderer::RenderMode::Wireframe` and the wireframe graphics pipeline; `Toolbar` replaces the Shaded/Wireframe radio pair with a "Shaded" label + an "Edges" checkbox (defaulting on). `main.cpp` drops `wireframePipeline`, `buildPipelines` returns a single shaded pipeline, keyboard shortcut `W` toggles the Edges overlay (not the mode). `RenderMode.h` can keep the enum if anything else uses it; otherwise delete. | Update `ToolbarTest` (or equivalent UI test) to pin the new shape: radio pair → bool toggle; wireframe radio no longer present. `ctest -j$(nproc)` passes. Visual spot-check: toggle edges on/off with `W` mid-session; no visible regressions in section view / PoV / first-person. | `ui/Toolbar.{h,cpp}`, `ui/tests/ToolbarTest.cpp`, `app/main.cpp`, `renderer/include/renderer/RenderMode.h` (possibly deleted) |
| RP.17.6 (optional) | **Edge-snap integration** — `scene::Snap` gains an edge-snap mode that searches the per-batch edge-index buffers for the line closest to the mouse ray (foot-of-perpendicular). Returns a snap point + the owning edge. Useful for "snap to wall corner edge" when measuring. | Unit: ray aimed at a cube-edge midpoint returns a snap point on the expected edge within epsilon; ray parallel-offset from an edge returns that edge's nearest point. | `scene/Snap.{h,cpp}`, `tests/scene/test_snap.cpp` additions |

**Ordering**

```
RP.17.1 ── RP.17.2 ── RP.17.3 ── RP.17.4 ── RP.17.5 ── (RP.17.6 optional) ── stage gate
```

Strictly linear — each step's output is the next step's input. 17.1 and 17.2 both live in `scene/` (≤2-modules-per-session rule satisfied per task). 17.3 / 17.4 are renderer-only. 17.5 spans `ui/` + `app/main.cpp` + optional `renderer/RenderMode.h` retire — borderline; split if `main.cpp` cleanup balloons.

**Risks**
- **IFC meshes may be flat-shaded** (one vertex per face-corner, no index sharing between adjacent faces). Without position-welding, the extractor would see every edge as a boundary. The `weldEpsilon` default is the mitigation; `test_edge_extractor.cpp` includes a flat-shaded-cube case (3×12=36 vertices, 12 unique positions) to lock this in.
- **Curved surfaces over-fire edges** (e.g. a cylinder triangulated as 32 facets at 30° dihedral would emit 32 edges, which is correct but busy). Acceptable for v1 — curved IFC geometry is rare in buildings; raise the default threshold to 30–35° if the sample house looks cluttered. Configurable via the extractor config so the Toolbar can expose a slider later without re-extracting (we'd need an edge-angle buffer to do that; out of scope for 17).
- **`wideLines` not guaranteed** on all Vulkan devices. Default to `1.0f` line width; if we want thicker later, guard behind a feature check or escalate to quad-expansion (out of scope for 17).
- **Edge z-fighting with surface**. Mitigated by `polygonOffset` + `depthTestEnable=VK_TRUE, depthWriteEnable=VK_FALSE, compareOp=LESS_OR_EQUAL`. If artefacts persist under MSAA-off renderer, bump the bias; lines should always render slightly in front of their owning surface.

**Stage framing**: Stage RP has been "closed" three times (RP.13b, RP.15b, RP.16.8). RP.17 re-opens it a fourth time rather than standing up a new "Stage E — Edges", by the same logic as RP.16: it's architectural-viewer cleanup (retiring a feature that doesn't work for BIM) that couldn't be known at original RP kickoff. Stage 9 (RT) follows after RP.17 closes.

---

### RP.17.7 — Smooth-line rasterization via `VK_EXT_line_rasterization` (added 2026-04-20)

After RP.17.5 shipped, the 1-px aliased line overlay reads poorly on sloped wall corners and window reveals — SMAA is post-process and too coarse to catch the thin high-contrast line on its own. Instead of a quad-expansion + SDF rewrite (the CAD-standard, but a full session's work), start with the GPU-provided path: request `VK_EXT_line_rasterization` with `rectangularLines` + `smoothLines`, and chain `VkPipelineRasterizationLineStateCreateInfoEXT` onto the `EdgeOverlayPipeline` with `VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT`. Driver does coverage-based AA, the overlay's existing alpha-blend state composites the partial coverage correctly. If the extension is absent (headless CI, some mobile drivers), fall back silently to aliased lines — log once at device init.

**Scope**
- **In:** `Device` queries + optionally enables `VK_EXT_line_rasterization` and the `smoothLines` feature; `Device::HasSmoothLines()` accessor; `PipelineConfig::smoothLines` bool gate; `Pipeline.cpp` chains the line-state struct when set; `EdgeOverlayPipeline` ctor takes a `smoothLines` flag and forwards it; `main.cpp` passes `device.HasSmoothLines()` at pipeline build.
- **Out:** Stippled / dashed lines (different `VkLineRasterizationModeEXT`, separate UX question), wide lines (still 1-px), fallback quad-expansion for driver-less paths (deferred to a potential RP.17.8 if silent fallback proves inadequate).

**Tasks**

| # | Task | Test | Output |
|---|------|------|--------|
| RP.17.7 | Enable smooth-line rasterization on the edge overlay. `Device` queries `VK_EXT_line_rasterization` + `VkPhysicalDeviceLineRasterizationFeaturesEXT.smoothLines` at physical-device pick; when both are available, adds the extension to the device create list and chains the features struct on `VkDeviceCreateInfo.pNext`. `HasSmoothLines()` returns the negotiated flag. `PipelineConfig` grows `bool smoothLines = false`; `Pipeline.cpp` chains `VkPipelineRasterizationLineStateCreateInfoEXT{lineRasterizationMode = RECTANGULAR_SMOOTH_EXT}` onto the rasterizer when true. `EdgeOverlayPipeline` takes a `smoothLines` ctor param; `main.cpp` passes `device.HasSmoothLines()`. | Extend `EdgeOverlayPipelineTest` with a `ConstructsWithSmoothLinesWhenSupported` case: `GTEST_SKIP` when `HasSmoothLines()` is false, else assert the pipeline builds with the flag on. Keeps the existing build tests unchanged. | `renderer/Device.{h,cpp}`, `renderer/Pipeline.{h,cpp}`, `renderer/EdgeOverlayPipeline.{h,cpp}`, `app/main.cpp`, `tests/renderer/EdgeOverlayPipelineTest.cpp` |

**Stage framing**: same as RP.17 — stage RP re-opens a fifth time for a one-task polish that couldn't be predicted at RP.17 kickoff. Stage gate: full `ctest -j$(nproc) --output-on-failure`.

---

### RP.17.8 — Edge overlay + axis section (added 2026-04-20)

Two tied issues surfaced after RP.17.7 + the softer grey+alpha tweak: (1) the edge overlay draws *through* axis clip planes — edges keep rendering on the hidden side of the cut — and (2) when the user triggers a section-fill, the cap is a solid-colour polygon with no silhouette, so cross-sections don't read as architectural plans do (where every cut element shows an outline around its filled slice). Fix both so the edge pass becomes a first-class citizen of the section-view pipeline, not just an overlay on the unclipped model.

**Scope**
- **In:** add `ClipPlanesUbo` to the edge-overlay descriptor layout + replicate `basic.frag`'s clip-plane discard loop in `edge_overlay.frag`; derive per-plane section outline geometry from the existing `scene::StitchSegments` output (the same polyline path that feeds `SectionCapGeometry` before triangulation) and draw it via the edge pipeline each frame an axis plane is active.
- **Out:** Hidden-line-removal in section view (i.e. occluded cap edges shown as dashed) — deferred; user-configurable dashed/dotted cap outlines (single solid line for v1); per-IFC-type cap-edge colour (deferred with the same rationale as RP.17's flat edge colour).

**Modules involved**
- `assets/shaders/` (edge_overlay.frag clip-plane discard)
- `renderer/` (EdgeOverlayPipeline descriptor layout bump)
- `scene/` (new `SectionEdgeGeometry` reusing the stitched-segment output from `SectionCapGeometry` — share the dirty-hash so rebuild cost stays single-pass)
- `app/main.cpp` (two new draw blocks: one keeping the overlay aware of clips, one drawing section edges after caps)

**Tasks**

| # | Task | Test | Output |
|---|------|------|--------|
| RP.17.8.a | **Edge overlay respects axis clip planes.** Add binding 3 (ClipPlanesUbo) to the edge-overlay descriptor layout; `edge_overlay.frag` gains the `basic.frag` discard loop (reject fragments where any enabled plane evaluates `dot(plane.xyz, worldPos) + plane.w < 0`). Pass `worldPos` from vert → frag via a new `layout(location=0)` varying. Pipeline rebuild checks the layout change. | Extend `EdgeOverlayPipelineTest` with a `ConstructsWithClipPlaneLayout` case asserting the new binding. Visual: activate an axis clip plane with `CutFront` mode — edges on the hidden half disappear cleanly. | `assets/shaders/edge_overlay.{vert,frag}`, `renderer/EdgeOverlayPipeline.{h,cpp}` |
| RP.17.8.b | **Section-plane element outlines.** New `scene::SectionEdgeGeometry` reusing the per-plane polylines that `SectionCapGeometry::Rebuild` already stitches from triangle-slice segments (factor a shared producer so both dirty-track on the same hash). Upload as a line-list vertex buffer per active plane; `main.cpp` draws after the section-fill pass (so outlines sit on top of caps) using the `EdgeOverlayPipeline`. Drawn whenever a clip plane has `sectionFill = true`, independent of the Toolbar "Edges" toggle (section outlines are a hard architectural-drawing expectation, not a stylistic choice). | Unit: cube mesh + axial plane → 4 segments (one per side of the square cap); two disconnected meshes → two independent outlines. Visual: cross-section through `sample.ifc` reads like a floor plan — filled rooms, outlined walls. | `scene/SectionEdgeGeometry.{h,cpp}`, `scene/SectionCapGeometry.{h,cpp}` (refactor to share polyline producer), `app/main.cpp` |

**Ordering**

```
RP.17.8.a ── independent, ships first
RP.17.8.b ── depends on .a's varying + clip-plane wire-up for the new section-edge draw block
```

Two sessions; each lands as a single commit. Stage gate at end of 17.8: full `ctest -j$(nproc) --output-on-failure`.

**Risks**
- **`SectionCapGeometry` internals leak into `SectionEdgeGeometry`.** Current `Rebuild` triangulates inline; lifting the polyline producer out without regressing cap triangulation is the real cost of 17.8.b. Mitigation: factor a `SliceAndStitch(scene, meshes, manager) → per-plane polyline map` helper that both geometries consume; cap keeps triangulating, edge just appends segments.
- **Clip-plane discard on lines has alpha edge effects.** Smooth-line rasterization writes coverage into alpha; combined with the `discard` the anti-aliased fringe near the cut may look hard. Acceptable for v1; revisit if visually jarring.

**Stage framing**: stage RP re-opens a sixth time. Same pattern as RP.17.5/17.7 — a viewer-specific polish that couldn't be planned at 17 kickoff.

---

### RP.18 — Window-transmitted sun shadows (classical raster, added 2026-04-20)

The raster renderer's sun-shadow test today is binary: depth sample says "lit" or "shadowed". That means sun hitting an `IfcWindow` is treated exactly like sun hitting a concrete wall — the floor behind the window stays fully shadowed, which is wrong on every architectural scene. Stage 9.6 solves this with RT, but only in RT modes. The classical raster path is the default renderer (per the Stage 9 additive rule), so it needs its own approximation — one that works on every GPU, regardless of RT capability. The technique is **colored transmissive shadow maps** (Dachsbacher-Stamminger 2003 family): a second render pass from the light's POV captures the tinted attenuation of transparent geometry; the main fragment multiplies the sun contribution by the sampled tint after the PCF visibility test.

**Quick-read semantics (what the user will see):**
- Sun hits a south-facing window → the floor behind the window is lit at glass-tinted intensity (40–60% of direct sun, tinted by the `IfcWindow` surface colour), not pitch-dark as today.
- Multiple glass panes in the light path → attenuations multiply (min-blended in the transmission pass).
- Opaque wall anywhere in the light path → fragment is fully shadowed regardless of transmission (opaque depth wins).
- Hemisphere ambient + indoor preset (artificial interior lights from RP.16.5) stay unchanged — RP.18 only tints the sun.

**Scope**
- **In:** second colour attachment on the shadow render pass (R16G16B16A16_SFLOAT, cleared to white each frame); new `shadow_transmission.{vert,frag}` pipeline that renders transparent-only geometry with min-blend into the transmission attachment; classification in `main.cpp` of opaque vs transmissive draw calls (reuses existing surface-alpha + per-type-alpha plumbing from 7.8a / 7.8d); `basic.frag` samples the transmission map and multiplies the sun-key contribution by the sampled tint; `RenderQualityPanel` toggle defaulting **on**; docs.
- **Out:** Per-window normal-mapped caustics (out of budget; needs a photon-map-ish approach); coloured *indirect* bounce off the sunlit floor (that's Stage 9's path-tracer scope); transmission from artificial indoor lights (they're point-ish and don't need windows); alpha-stippled or thin-frame transmission (we treat frames as opaque — correct, since `IfcWindow` frames are opaque in most IFCs). RT path already handles everything above in Hybrid / Path Traced modes.

**Why additive + consistent with Stage 9:** the raster transmission map is the classical equivalent of Stage 9.6's any-hit shadow ray. Both read the same `IfcWindow` + surface-alpha info; both feed the same `SunLightingScene.sun.color`. Raster gets a direct-light approximation (floor lit, back wall not), RT gets the full solution. The renderer-mode switch (Stage 9.10) toggles which one runs.

**Modules involved**
- `renderer/` — `ShadowPass` (second attachment), new `ShadowTransmissionPipeline`
- `assets/shaders/` — `shadow_transmission.{vert,frag}`; `basic.frag` (sun-tint multiply)
- `app/main.cpp` — classify opaque vs transmissive draw calls for the shadow render pass; bind the transmission map as a sampler to the main descriptor set
- `ui/RenderQualityPanel.cpp` — new "Window transmission" checkbox under the Shadows header (default on)

**Tasks**

| # | Task | Test | Output |
|---|------|------|--------|
| RP.18.1 | **Transmission attachment in `ShadowPass`.** Extend the shadow render pass with a second attachment: `R16G16B16A16_SFLOAT`, same resolution as the shadow depth map, `LOAD_OP_CLEAR` with clearValue = (1,1,1,1), `STORE_OP_STORE`. Add `GetTransmissionImageView()` + `GetTransmissionSampler()` accessors. Expose `transmissionMapResolution` tied to `mapResolution`. | `ShadowPassTest`: new `ExposesTransmissionAttachment` case — build a shadow pass at 1024², assert non-null image view + sampler and that the attachment format is `R16G16B16A16_SFLOAT`. | `renderer/ShadowPass.{h,cpp}`, test |
| RP.18.2 | **`shadow_transmission.{vert,frag}` + `ShadowTransmissionPipeline`.** Vertex shader = standard light-space transform (push-constant `lightSpace × model`, same as `shadow.vert`). Fragment shader outputs `vec4(materialColor.rgb * (1 - alpha), 1)` — glass tint scaled by transmission factor — with **min blending** (`VK_BLEND_OP_MIN`) so multiple panes compose multiplicatively (darkest/most-tinted wins). Depth-test LESS against the opaque depth map (so a wall behind glass still blocks); depth-write OFF. | `ShadowTransmissionPipelineTest`: construct with the same render pass; assert pipeline builds; assert min-blend state is wired. | `assets/shaders/shadow_transmission.{vert,frag}`, `renderer/ShadowTransmissionPipeline.{h,cpp}`, test |
| RP.18.3 | **Draw-loop wiring: two sub-passes in the shadow render pass.** In `main.cpp`'s shadow-depth block, split the per-draw loop into (a) opaque — write depth via existing `shadowPipeline`; (b) transmissive — bind `ShadowTransmissionPipeline`, push the glass RGBA (scene mesh surface colour × `(1 - effectiveAlpha)`), draw with depth-test but no depth-write. Classification uses the already-plumbed `effectiveAlpha = min(surfaceStyleAlpha, typeAlphaOverride, perElementAlpha)` — alpha < ~0.95 → transmissive bucket. | Unit test in `ShadowPassTest`: helper `ClassifyOpaqueVsTransmissive` on a synthetic draw-call list with alphas {1.0, 0.4, 1.0, 0.8} returns the correct two buckets. No visual expectation at this step — the attachment exists and is populated but `basic.frag` doesn't sample it yet. | `app/main.cpp`, tiny classifier helper + test |
| RP.18.4 | **`basic.frag` samples transmission + multiplies sun tint.** Add sampler binding (binding 4 on the main descriptor set layout, slot borrowed next to the existing shadow-map at binding 2 + shadow-transmission at binding 4). After the PCF visibility test, sample the transmission map at the same light-space UV: `vec4 transmit = texture(shadowTransmissionMap, lightUv)`. Compute the sun contribution as `sunColor * max(visibility, transmit.rgb)` — visibility wins for fully-lit fragments; transmission wins for fragments the opaque depth pass marked shadowed but the glass passes light through. Update descriptor set + binding count. | CPU mirror `ComputeTransmittedSun(visibility, sunColor, transmit)` in `renderer/Lighting.{h,cpp}` with unit tests: full visibility → unchanged sun; zero visibility + white transmit → black (wall-shadowed); zero visibility + tinted transmit → sunColor × tint (window-shadowed). Visual: on `sample.ifc`, the floor behind a south-facing `IfcWindow` is noticeably lit while surrounding wall-shadowed floor stays dark. | `assets/shaders/basic.frag`, `renderer/Lighting.{h,cpp}`, `renderer/DescriptorSet.*` (binding), `app/main.cpp` (wire sampler) |
| RP.18.5 | **Panel toggle + stage gate.** Add `bool windowTransmission = true` to `ShadowSettings` (default on — the whole point is that this ships enabled). `RenderQualityPanel`: new checkbox under the Shadows header, disabled when shadows are off. When off, the shadow-transmission pass is skipped and `basic.frag` uses `visibility` alone (bit-compatible with pre-18 output — regression guard). | `RenderQualityPanelTest` case pinning the default `windowTransmission == true`. `ShadowPassTest` guard: when `windowTransmission == false`, the transmission sub-pass early-outs without any `vkCmdDraw` calls. Stage gate: full `ctest -j$(nproc) --output-on-failure`. | `renderer/Lighting.h` (field), `ui/RenderQualityPanel.{h,cpp}`, `app/main.cpp` (skip when off) |

**Ordering**

```
RP.18.1 ── independent (attachment plumbing)
RP.18.2 ── independent of .1 shader-wise, but the pipeline needs .1's render pass
RP.18.3 ── depends on .1 + .2 (draws into the attachment with the new pipeline)
RP.18.4 ── depends on .3 (needs a populated transmission map to read from)
RP.18.5 ── depends on .4 (the toggle gates the whole chain end-to-end)
```

Single session expected; sub-tasks each land as their own `[RP.18.N]` commit per the per-task-per-commit rule.

**Risks**
- **Min-blend on MoltenVK / older drivers.** `VK_BLEND_OP_MIN` is core Vulkan 1.0, but a few mobile drivers historically had issues. Mitigation: unit-test pipeline construction; if a device pick hits a problem, fall back to `VK_BLEND_OP_DST_COLOR`-style multiplicative blending (mathematically equivalent for 0-or-one-pane cases; multi-pane overlap gets dimmer than min-blend but not wrong).
- **Transmission map resolution tied to shadow-map resolution.** At 4096² the transmission attachment is another 128 MB (R16G16B16A16). Mitigation: half-res is sufficient for a smoothly-tinted result; can downsize in a follow-up if the VRAM cost annoys. For first version, match shadow-map resolution — if user picks 4096 they've opted into the memory.
- **Per-mesh material colour on the transmission pipeline.** The shadow pass today doesn't carry material state — push constants are light-space × model only. We'll push a small `vec4` (glass RGBA = surface-style colour × (1-alpha)) via an additional push-constant range on the transmission pipeline. Keeps the existing opaque shadow pipeline untouched.
- **Double-counting with hemisphere ambient.** Hemisphere ambient already lights interior-facing normals a little. That's fine — transmission only augments the *sun* term, which was zero on shadowed interior floors before. Ambient + transmitted sun stacks to a visually correct amount.

**Stage framing**: stage RP re-opens a seventh time. RP.18 is a classical-raster feature that RT (Stage 9.6) happens to duplicate in a more principled way — both ship because the classical renderer is the default and can't wait on RT (per the "additive, never replace" feedback from 2026-04-20). Stage gate at end of RP.18: full `ctest -j$(nproc) --output-on-failure`, then proceed to Stage 9.

---

