# Rendering — the WebGPU renderer

**Sources of this file:** `sim/src/render/` (13 files: `FBRenderer.h/.cpp`, `FBDrawStage.h`,
`FBFrameContext.h`, `FBGpu.h`, `FBChunkMesh.h`, `FBChunkVtx.h`, `FBMips.h`, plus the units chain `FBJson`/`FBGlb`/`FBUnitModel`/`FBUnitDraw.h` — the HUD geometry/font and
`FBCamera.h` moved to `systems/` and `core/`) and
`sim/src/render/stages/` (30 files), plus CLAUDE.md's `render/`, `render/stages/` and "Rendering (das
Herzstück)" sections. Every number below appears verbatim in the source; derivations and settings are
marked as such. Contradictions between CLAUDE.md and the code are in *Gaps* — they are not resolved
silently.

Neighbouring files: `architecture.md` (lib/client split), `world-and-terrain.md` (what feeds the
renderer with geometry and albedo). The HUD **symbology** (what is drawn) does not belong here but to
`systems/FBDisplaySystem` + `modules/f16/displays/FBF16Hud`; this file describes only the **backend**
(what it is drawn with).

---

## Spec

One renderer source, two link targets. The renderer is a **bolt-on**: never a dependency of the
physics or the termination logic.

| Contract | Acceptance / measurement anchor |
|---|---|
| WebGPU only, WGSL lives in stage files | `grep -c 'R"(' FBRenderer.cpp` == 0 |
| A stage is one shader with its pipelines, bind groups and draws, drawing into a BORROWED encoder | no stage opens a render pass |
| The pass topology is a contract — only `FBRenderer` sets pass boundaries | the encode order is fixed and documented; a stage split must not multiply passes |
| **The TACTICAL MAP is drawn by the HUD stage, in the HUD pass, and REPLACES the cockpit symbology there** | `FBRenderer::SetMapOverlay(const FBHudGeometry*)` → `FBHudStage::SetOverlay`. `Encode` builds the display slot's geometry only when there is NO overlay, so the two pictures cannot be on screen together — which is [`../player-layer.md`](../player-layer.md) §9.7 enforced by construction rather than by discipline. **Zero `BeginRenderPass` were added:** `FBRenderer.cpp` still has 7 call sites and `render/stages/*` + `render/FBTacticalMap.cpp` have 0 |
| **The map's OSM base IS the terrain renderer, at nadir** | the camera altitude is chosen so its footprint equals the map span (`H = span / (2·tan(30°)·aspect)`), which is why a symbol sits on the ground it was measured over with no second projection to keep in step. **The map's centre pixel is `ViewH/2`, NOT `Height/2`** — the scene is shifted up by the MFD bank's third (`ViewShiftNdc`), and drawing the centre at the frame centre put every symbol 120 px (4.1 km at a 46 km span) SOUTH of its ground. Measured: the scale-invariant fixed point of two frames rendered at spans 6 km and 12 km from the same tick sits at **cy ≈ 226…240** (masked normalised correlation 0.363 / 0.333) against **0.159 at cy = 360** — the fix is confirmed in direction and magnitude; the method's own resolution is ±15 px and a tighter registration check is open. [MESS] the pitch is **−89.9° and not −90°**: `FBCameraBasisEcef` builds `right` from `fwd × world-up` and at exact nadir those are parallel — the basis collapses, and the frame comes back EMPTY with the terrain reporting 22 leaves DRAWN. 0.1° costs `H·tan(0.1°)` = 21 m at H = 12 km, i.e. 0.6 px of a 1280 px frame |
| Global standard WGS84-ECEF, camera-relative, reversed-Z depth | far terrain does not z-fight; horizon dip from curvature |
| Ground truth from model geometry | eye height at ground from JSBSim's gear geometry, not a fixed number |
| Native and WASM render the same frame | `gpu_native` is the headless PNG oracle for frame proofs (`../build-and-ops.md`) |
| Feature gates are baked constants | a disabled pass costs nothing |
| **The frame is a 3x3 grid: the windscreen is the top two rows, the MFD bank the bottom one** | the WORLD covers the whole frame and the projection puts the boresight at the windscreen's centre (§2.4) — the MFD bays are translucent over it; the projection CROPS instead of squeezing, **measured**: pitch 0 → −20° moves a feature exactly as the cut model predicts (median residual −0.22 px over 107 columns), the letterbox model is 76 px out; the bank costs no pass — `passcount` stays 6 / 7 with a deck / 5 with the HUD off |

## State

Built; the stage split is finished (zero inline shaders in `FBRenderer.cpp`).

| Piece | Status | Anchor |
|---|---|---|
| Stage split in four slices | done | `c9206eb`…`2099cb0` |
| Hillaire atmosphere (transmittance / sky-view / sky), sun, moon, stars | built | `c9206eb`…`2099cb0` |
| Terrain stage with render bundles and two-phase streaming | built | `c9206eb`…`2099cb0` |
| Tonemap, upscale, loading screen | built | `c9206eb`…`2099cb0` |
| Camera basis shared by native and WASM (`FBCameraBasisEcef`) | built | `705c90a` |
| HUD backend | built — see [`hud.md`](hud.md) | `2f3c277`, `8997eec`, `6f160af` |
| Cloud chain | rebuilt: ONE stage over ONE shared density function — see [`clouds.md`](clouds.md) | `9ca2c0e` + the R5 follow-up round |
| Units | **built** — one indexed draw per unit from the published pose, LOD off the asset sidecar, moving parts off the published articulation. Pass count unchanged (6). See [`units-visual.md`](units-visual.md) | this round |
| Sprites (chaff/flares/smoke) | **nothing** — `FBSpritesStage` is still NoOp | — |

## Gaps

### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `render/FBCamera` + `sim/up.sh` | **the camera clamp "never below the surface" has no consumer any more.** `FB_GROUND_CLEAR` is produced by `fb-sim` and read by nobody — an invariant promised in `CLAUDE.md` is effectively off. |
| `render/FBCamera` | reversed-Z numbers disagree: `CLAUDE.md` says near 0.01 m / far 240 km, the code uses an infinite far plane and `zn = 0.05`; the 240 km are the streaming view radius |

### Open work (from the retired `TODO.md` §4)

| # | Thing |
|---|---|
| 4.1 | ~~`FBUnitsStage`/`FBSpritesStage` NoOp~~ — the aircraft half is built and measured ([`units-visual.md`](units-visual.md)); the effect half is not |
| 4.3 | transmittance LUT is recomputed every frame although it only depends on altitude and sun cos θ |
| 4.4 | ~~aerial perspective off by default (`FB_AP=0`)~~ **closed** — the switch and the dead Rayleigh/Mie block are gone; the terrain now runs a WEATHER-driven haze out of the same `FBAtmoHaze.h` the cloud deck uses (§6, [`clouds.md`](clouds.md)). Its scale height was corrected the same day: two summed terms (molecular 8 km, aerosol 1.2 km) instead of one, and the molecular one carries λ⁻⁴, which closes `clouds.md` Gaps 5.7 **and** 5.8 — the per-channel extinction the deleted block had is back, from the physics rather than from a LUT |
| 4.5 | upscale is bilinear only (`TODO bicubic/sharpen`) |
| 4.6 | dead code `w3_frustum_from`, `w3_aabb_visible`; the static terrain path is untested inheritance |

Renderer-adjacent items that belong to the world/tile side (`TODO.md` §4.7/4.8 — DEM cache per worker
instance, time-based eviction, `kNodeCeil`, imagery mode not declarable in `.fbm`, TLS not wired) are
parked in [`../roadmap.md`](../roadmap.md) until `world/` is split.

### Inventory (from the previous `Open points` section)

1. **~~`FBUnitsStage` and `FBSpritesStage` are NoOp.~~ HALF CLOSED.** `FBUnitsStage` draws: one indexed
   draw per visible unit from the BORROWED registry's published pose, with the asset sidecar's LOD table
   and its part table, and the pass count is still 6. Measured: an F-16 at 60 m lands within **1.69 px
   (1.07 %)** of its projected pose, and a mission with nothing to draw produces **bit-identical** PNGs
   to the pre-round binary. **`FBSpritesStage` is still NoOp** — chaff, flares, smoke and missile plumes
   remain invisible, and so do released stores and ground targets, which publish no mesh key. See
   [`units-visual.md`](units-visual.md).
2. **The reversed-Z numbers contradict each other.** CLAUDE.md says "near 0.01 m / far 240 km". The
   code (`MvpCamRel`) uses an **infinite** far plane and `zn = 0.05`. The 240 km are the streaming view
   radius (`FB_VIEW_KM`), not the far plane; the 0.01 m appear nowhere in the code. Not resolved —
   presumably stale documentation, but that is a presumption.
3. **The camera clamp "never below the surface" has no consumer any more.** `fb-sim` supplies
   `window.FB_GROUND_CLEAR` to the browser (`clients/FBSimHost.cpp`, read from `/tmp/fb_clearance`), and
   `web/config.js` sets it to 0 — but **no** C++ code reads it (`grep FB_GROUND_CLEAR src/` finds only
   the producer). The geometry-true height acts today only through the spawn IC
   (`GetGroundClearanceM`), not as a per-frame clamp. CLAUDE.md's phrasing "the camera never goes below
   the surface" therefore describes a state the code no longer enforces.
4. **Dead code in `FBCamera.h`.** `w3_frustum_from` and `w3_aabb_visible` (Gribb-Hartmann culling) have
   **no caller** in the whole tree — the culling happens in the streamer (`FBWorld`) via view radius
   and frustum weighting, not via these planes. Used from this file are only `w3_cam_from`,
   `w3_basis_from`, `w3_horizon_dip_rad` (all three by the HUD symbology) and `FBCameraBasisEcef`.
   Whether the frustum helpers are reserved for a future `FBUnitsStage` or should be dropped without
   replacement is open.
5. **Aerial perspective — resolved, and by REPLACEMENT rather than by arming the old one.** The
   `FB_AP` switch and the whole tLUT-ratio/inscatter/glow block it gated are removed. The block was a
   CLEAR-AIR model (a Rayleigh/Mie transmittance-LUT ratio) that could not see the weather at all; the
   terrain now runs the same weather-driven haze the cloud deck runs, out of one shared header
   (`stages/FBAtmoHaze.h`, §6). The one capability of the removed block that the first cut lost — a
   per-CHANNEL extinction — came back with the scale-height correction: splitting σ₀ into a molecular
   and an aerosol term lets the molecular one carry its own λ⁻⁴, so distant terrain loses its blue
   because of Rayleigh and not because of a lookup ([`clouds.md`](clouds.md) Gaps 5.7/5.8, both
   closed). The transmittance LUT is still computed every frame because sky, sun and the cloud march
   need it; the terrain no longer binds it.
7. **The transmittance LUT is recomputed every frame.** It depends only on altitude and the sun's
   cos θ; a `TODO cache while the sun is static` is in the header and is open.
8. **Upscale is bilinear.** `TODO bicubic/sharpen` in the header; likewise missing is the HUD's 8-tap
   glow (`TODO` in `FBHudStage.cpp`), which would give the luminance feel of a real combiner.
9. **`FBTerrainLoader`'s static path (`SetTerrain`/`SetAlbedoArray`) is bring-up only.** Both clients
   run streaming; the static path in `FBTilesStage` (direct draws, layer == tile index) remains as a
   second code path and is nowhere tested regularly.
10. **The loading screen has a timeout exit** (30 s, `FB_LOAD_TIMEOUT_MS`) after which it releases the
    picture no matter how little is resident — the log distinguishes `converged` from `TIMEOUT`. A frame
    proof originating from a timeout boot is therefore not the same picture as one from a converged
    boot. Today that is visible only from the log line, not from the PNG.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1 Fundamental decisions

| Decision | Implementation | Derivation / reason |
|---|---|---|
| **API: WebGPU** | ONE source, TWO link targets — emdawnwebgpu (browser) and native Dawn (`gpu_native`) | The same Dawn header family on both sides: "write once, link twice". Native Dawn really renders; a headless browser with SwiftShader does not provide that proof. |
| **WGS84-ECEF, camera-relative** | vertices carry the offset to the tile origin; the frame subtracts `origin_ecef − cam_ecef`; the camera sits at the origin | No absolute 6.4e6 m coordinate ever reaches a `float`. Precision is best AT the eye everywhere on Earth. |
| **Reversed-Z (Depth32Float)** | clear `0.0`, `CompareFunction::Greater`, infinite far, `zn = 0.05 m` | `[0,1]` clip (native, not GL's `[-1,1]`) → the full mantissa for depth; distant terrain does not z-fight. |
| **HDR + ACES** | scene → `rgba16float` target, one fullscreen tonemap (Narkowicz ACES fit) → sRGB | Light stays linear until then; display encoding happens at EXACTLY ONE place (the tonemap pass, whose sRGB view encodes on store). |
| **Render bundle submission** | terrain draws are recorded once and replayed per frame; re-recording only on a STRUCTURAL change | ~N CPU draw encodes become one `ExecuteBundles`. |
| **Feature gates = baked constants** | env-driven string replace on the shader source before `CreateShaderModule` | A dead path is optimised away by the shader compiler and costs nothing — no runtime branch. |
| **Fixed 720p frame target** | scene + tonemap + HUD land in `FrameTex` (1280×720); an upscale pass puts it onto the swapchain | The sim resolution is stable, the display follows canvas × DPR. |

#### 1.1 Two link targets, one renderer

| | Browser (WASM/emdawnwebgpu) | Native (Dawn, `gpu_native`) |
|---|---|---|
| Bring-up | `FBRenderer::Init(canvasSelector, w, h)` — asynchronous, callbacks `AllowSpontaneous`; `Ready()` polls the app | `FBRenderer::InitOffscreen(w, h)` — blocking via `Instance::WaitAny(future, UINT64_MAX)` |
| Prerequisite | the browser event loop pumps the callbacks | the instance feature `TimedWaitAny` is requested at `CreateInstance` |
| Final target | `wgpu::Surface` on `#gpu`, format = the first sRGB-capable one from `SurfaceCapabilities` (otherwise `BGRA8Unorm`) | `OffscreenTex`: `RGBA8UnormSrgb`, usage `RenderAttachment\|CopySrc` |
| Return | — | `ReadPixels()` → tightly packed W·H·4 RGBA8, already sRGB-encoded → straight into `stb_image_write` |

At the adapter it is logged **what** WebGPU really runs on (`FBLog::Info("render","adapter")`):
`adapterType == CPU` means a software rasteriser (SwiftShader/lavapipe/WARP) — then high CPU load is
the browser's, not our code's. Also logged: `maxTextureArrayLayers`, `maxBufferSize`,
`maxTextureDimension2D`.

Device loss is not a crash: the `DeviceLostCallback` sets `DeviceLost`, GPU operations are skipped from
then on, the CPU side (tile streaming, counters) keeps running. `DeviceUsable()` =
`DeviceReady && !DeviceLost`.

#### 1.2 Camera-relative ECEF and the projection

`MvpCamRel()` (`FBRenderer.cpp`, static) builds **projection × view** like this:

- View = a pure rotation from the ECEF camera basis (`right`, `camUp`, `−fwd`) — no translation,
  because the vertices already arrive pre-shifted.
- Projection = infinite reversed-Z: `p = {f/asp, 0,0,0, 0,f,0,0, 0,0,0,−1, 0,0,zn,0}` with
  `f = 1/tan(fov/2)`, `fov = 60°`, `zn = 0.05`. From that `z_clip = zn`, `w = −z_eye`, hence
  **`depth = zn / (−z_eye)`** — monotonically falling with distance, 1 at `zn`, → 0 at infinity.
- No explicit far plane. The "view distance" is a property of the **streaming** (radius `FB_VIEW_KM`,
  default 240 km — see `world-and-terrain.md`), not of the projection.

The 20-float block `Mvp20` in the `FBFrameContext` is this matrix (index 0..15, column-major) plus the
sun direction (16..18) plus one pad — exactly as the terrain uniform expects it.

#### 1.3 Depth states per stage

| Stage | `depthWriteEnabled` | `depthCompare` | Effect |
|---|---|---|---|
| `FBSkyStage` | false | `Always` | background; everything draws over it |
| `FBSunStage`, `FBMoonStage` | — (additive, in the same pass directly after sky) | as sky | pure addition, order-independent |
| `FBStarsStage` | false | `Always` | "at infinity"; terrain paints over them |
| `FBTilesStage` | true | `Greater` | reversed-Z: nearer = greater |
| `FBTileLightsStage` | false | `GreaterEqual` | hills occlude distant lights, but the sprites write no depth |

#### 1.4 HDR format: why `rgba16float` and not `rg11b10ufloat`

`rg11b10ufloat` was the bandwidth choice and is **rejected**: it has no alpha channel and no guaranteed
blend support — the cloud pass blends premultiplied alpha over it, and that broke. `rgba16float` is the
standard blend-capable HDR format; the 4 extra bytes per pixel at 720p are negligible
(`FBRenderer::OnAdapter`, comment there).

#### 1.5 The present path

| Resource | Size/format | Role |
|---|---|---|
| `HdrTex` | width×height, `HdrFormat` (rgba16float), `RenderAttachment\|TextureBinding` | scene target, linear radiance |
| `DepthTex` | width×height, `Depth32Float`, `RenderAttachment\|TextureBinding` | reversed-Z; the cloud pass SAMPLES it (hence `TextureBinding`) |
| `FrameTex` | width×height (1280×720), `SurfaceFormat` (sRGB), `+CopySrc` | scene+tonemap+HUD land here |
| Swapchain | canvas `clientSize × devicePixelRatio`, capped at 4096 | follows the display |

`SyncSwapSize()` reconfigures the surface only when width OR height changes by **≥ 8 px** (hysteresis
against sub-pixel jitter). Scene and HUD are untouched by this — only swapchain and upscale viewport
follow.

#### 1.6 Feature gates and env switches

The mechanism has two levels: either a **constant is baked into the WGSL source** (string replace
before `CreateShaderModule`, after which the shader compiler strips the dead block), or a whole
resource/pipeline group is **not built at all**.

| Switch | Default | Effect | Place |
|---|---|---|---|
| `FB_CLOUDS` | **1 (armed)** | arms the cloud layer pass. Off = no pipeline, no VRAM. Armed is not the same as drawn: the pass only exists when the weather sample has a deck | `FBRenderer::OnDevice` |
| `--cloudq` (native) / `SetCloudQuality` | 1.0 | scales the NODE count of the march, 0.25…8 | `FBCloudLayerStage` |
| `FB_MOON_SCALE` | 1.0 | multiplier on the real lunar angular radius (0.0045 rad ≈ 0.5°) | `FBRenderer::UpdateAtmosphere` |
| `FB_PHOTO_ZMAX` | 11 | from which zoom an aerial-imagery tile counts as "bright enough" (gain calculation) | `FBTilesStage.cpp` |
| `FB_PHOTO_EMA` / `FB_PHOTO_MAXGAIN` / `FB_PHOTO_LOG` | 0.08 / 2.5 / off | adaptive brightness adjustment of the imagery layers | `FBTilesStage.cpp` |
| `FB_GPU_NOOP` / `FB_GPU_BISECT` | compile defines | bisect levels: inert frames and acquire-only respectively — separates init-side from frame-side device death | `FBRenderer::RenderFrame` |

---

### 2 The pass topology as a contract

**The rule:** `FBRenderer` owns instance/adapter/device/queue/surface/targets, **every**
`BeginRenderPass`/`BeginComputePass` boundary and the encode order. An `FBDrawStage`
(`render/FBDrawStage.h`) draws **into the borrowed encoder** that `FBRenderer` has already opened, and
**never** opens or closes a pass itself.

Why this is a rule and not a matter of style:

1. **The pass count is a measured quantity.** The stage split was not allowed to change the number of
   `Begin*Pass` calls per frame; `RenderFrame` therefore counts them (`passCount`) and logs them
   (`FBLog::Debug("render","passcount")`) at the first SCENE frame and every 300 frames thereafter. A
   before/after diff is thus readable directly from the telemetry.
2. **A pass is expensive, a draw is not.** If every stage could draw its own pass boundaries, every new
   stage would multiply the topology — creepingly and unobtrusively.
3. **Attachments are a matter of contract.** Whoever opens the pass determines target views, load/store
   ops and clear values. The cloud resolve, for example, writes into TWO attachments whose ping-pong
   index the renderer queries BEFORE `Encode()` — the stage cannot build that descriptor itself.

Supplementary rules from `FBDrawStage.h`:

- **A stage self-gates.** "Nothing visible this frame" means: `Encode()` draws nothing. The renderer
  calls every stage in its slot **unconditionally** (the only exception is the HUD pass, whose
  `if (HudEnabled)` sits outside — an empty pass would still be a pass, and the pass-count invariant
  lives on exactly this outer `if`).
- **Two `Encode` forms.** `Encode(ctx, RenderPassEncoder&)` and
  `EncodeCompute(ctx, ComputePassEncoder&)`; the other one stays the inert default.
- **`FBGpu`** (device/queue/formats/size/instance) is given to a stage ONCE at `Init`/`Configure`,
  never per frame. **`FBFrameContext`** is the shared per-frame state (camera basis, MVP, sun, moon,
  day factor, weather, frame number, size) — a stage never reaches back into `FBRenderer`.

#### 2.4 The 3x3 grid — a projection, not an overlay and not a scissor

The cockpit takes the bottom third of the screen. Three ways to arrive there:

- **Overlay.** Draw the scene full-frame with the boresight at the FRAME's centre and paint the bank
  over it. The boresight then sits 120 px above the panel edge and half the pitch ladder is behind the
  panel — the aiming surface is no longer the aiming surface.
- **Scissor (built first, `95c2e8e`).** Give the scene pass `SetViewport/SetScissor(0, 0, Width,
  ViewH)`. Correct geometry, but the bottom row then holds nothing but the clear value, so the bays
  can only ever be drawn on BLACK.
- **Off-centre projection (built, this round).** `MvpCamRel` takes the PRE-GRID full-frame projection
  — 60° over `hFull`, aspect `w/hFull`, so `f` and `f/asp` are bit for bit the numbers from before the
  grid existed — and adds ONE term: a constant NDC shift `shift = 1 − hVp/hFull` on the y row of the
  z column (`p[9] = −shift`), which lifts the boresight from the frame's centre to the WINDSCREEN's.
  The scene pass then carries **no viewport and no scissor at all**: the world covers the whole frame
  and simply continues behind the bank, which is what the translucent bays show. With `hVp == hFull`
  (HUD off, the cloud lab) `shift` is 0 and every term is the pre-grid number.

The atmosphere uniform mirrors it exactly — `params.x = tan(halfFov)`, `params.y = w/hFull`,
`view.x = shift`, and every ray reconstruction goes through ONE spliced helper (`camRay()` in
`FBAtmoCommon.h`) instead of four copies of the same expression, so sky, sun, moon and cloud cannot
drift from the terrain. `kAtmoUniformBytes` is likewise one constant: the struct grew by a `vec4` and
six bind groups pin their binding size to it — the first build after the growth was six
`minBindingSize` failures and a black frame.

**Pixels per radian is unchanged, and that is the measurement.** Two frames, same camera, pitch 0 vs
−20°, the sky/ground boundary read per column (`gpu_native --pitch`):

| Model | pixels per unit tangent | residual against the measurement |
|---|---:|---:|
| **Cut** (built) | `K = (Height/2)/tan(30°) = 623.5` | **median −0.22 px**, mean −0.50, sd 3.20 (107 columns) |
| Letterbox | `(ViewH/2)/tan(30°) = 415.7` | median −75.8 px |

The tonemap pass gets no viewport either; it is a 1:1 `HdrTex -> FrameTex` map and the bottom row now
carries world like every other row.

**The bays' veil.** `FBHudGeometry::Fill()` draws one translucent quad per bay before its symbology.
Opacity is not taste, it is a contrast budget: the HUD pass blends in linear light, so `(1−a)` of the
background survives, the brightest bay background measured in a flying frame (99.5th percentile) is
the white SVS ground at `L = 0.93`, and HUD green (`L = 0.740`) has to clear WCAG AA's 4.5:1 for small
text:

```
(0.740 + 0.05) / ((1−a)·0.93 + 0.05) ≥ 4.5   ->   a ≥ 0.865
```

`kMfdVeil = 0.87` `[HERL]`. **Measured back** on `payerne-full` frames at that value, per bay, ink
excluded, against the 99.5th percentile of the bay's own background:

| Frame | bay 0 | bay 1 | bay 2 |
|---|---|---|---|
| cruise over the white plateau | green **4.74:1**, amber 3.51:1 | 4.68:1 / 3.46:1 | 4.67:1 / 3.45:1 |
| turn over darker ground | 5.91:1 / 4.37:1 | 5.91:1 / 4.37:1 | 4.72:1 / 3.49:1 |

Green clears 4.5:1 everywhere, amber clears the 3:1 large-text threshold everywhere and carries only
short warning words. The blend itself was checked separately: transmission through a bay is 0.23 of
the linear background at `a = 0.78`, i.e. exactly `1−a`. Over dark terrain the bays read nearly black;
that is the price of a FIXED veil, stated rather than hidden — the alternative, sampling the
background per pixel, needs the frame as a texture inside the HUD pass and therefore a second pass,
which the pass-count contract forbids.

`FBHudStage` draws the bank by appending into the SAME `FBHudGeometry` `BuildHud` just filled, so
three more displays cost **zero** `Begin*Pass` calls — measured: `passcount passes=6 clouds=1
cloudPass=0 hud=1` on `payerne-full`, unchanged across this round.
`ViewH == Height` whenever the HUD is off (the cloud lab), so those frames are untouched.

#### 2.1 The complete encode order

Order as in `FBRenderer::RenderFrame()`:

| # | Pass | Stage(s) | Target | Note |
|---|---|---|---|---|
| 1 | Compute | `FBTransmittanceStage` | `TransLUT` (256×64) | recomputed every frame (TODO: cache while the sun is static) |
| 2 | Compute | `FBSkyViewStage` | `SkyLUT` (192×108) | reads `TransLUT` |
| 3 | **Scene** (colour `HdrTex`, depth `DepthTex`, both clear) | `FBSkyStage` | HDR | fills the background, first drawing in the pass |
| 3 | ″ | `FBSunStage` | HDR | additive (One/One) |
| 3 | ″ | `FBMoonStage` | HDR | additive |
| 3 | ″ | `FBStarsStage` | HDR | additive, self-gated (EVS night only) |
| 3 | ″ | `FBTilesStage` | HDR + depth | terrain; render bundle (streaming) or direct draws (static) |
| 3 | ″ | **`FBUnitsStage`** | HDR | **NoOp**, but hard-wired: AI/weapon units draw directly after the terrain |
| 3 | ″ | `FBTileLightsStage` | HDR | night lights, depth-tested, self-gated |
| 3 | ″ | **`FBSpritesStage`** | HDR | **NoOp**, hard-wired: effect billboards directly before the HUD |
| 4 | Cloud layer (`FB_CLOUDS=1` **and** the weather has a deck) | `FBCloudLayerStage` | `HdrTex`, premultiplied blend | its OWN pass, because it must SAMPLE `DepthTex` (which was still an attachment in the scene pass) |
| 5 | Tonemap (colour `FrameTex`, clear) | `FBTonemapStage` | 720p sRGB | ACES; one pipeline — the cloud is already in `HdrTex` |
| 7 | HUD (colour `FrameTex`, **LoadOp `Load`**) | `FBHudStage` | 720p sRGB | only when `HudEnabled`; preserves the tonemapped picture |
| 8 | Upscale (colour = final, clear) | `FBUpscaleStage` | swapchain or offscreen | bilinear |

**Pass counts (the logged invariant):**

| Configuration | Passes | Verified |
|---|---|---|
| Standard, no weather deck | **6** (2 compute + scene + tonemap + HUD + upscale) | `passcount passes=6 clouds=1 cloudPass=0 hud=1` |
| clouds armed AND the weather has a deck | **7** (+ the cloud layer pass) | `passcount passes=7 clouds=1 cloudPass=1 hud=1` |
| Loading screen | **2** (HUD text into `FrameTex` + upscale) | |

No order-critical follow-ups remain after `Finish()`/`Submit()`: the temporal resolve that owned them
(ping-pong flip, history snapshot, timestamp poll) went with the old chain.

#### 2.2 The loading screen

A separate, short frame path (`if (LoadingScreen)`): a black `FrameTex`, `Hud->EncodeLoadingText` (the
text pipeline only: "LOADING TERRAIN x%" + tile counters), then upscale. No scene, no sky. The app
keeps JSBSim frozen meanwhile — the first flown frame is therefore already at full target resolution,
without a low-res ladder. Threshold and timeout live with the client (`FB_LOAD_THRESH` 0.95;
`FB_LOAD_TIMEOUT_MS` 30000 — `clients/FBAppWasm.cpp`).

---

### 3 Stage catalogue

| Class | File (`render/stages/`) | Kind | Target / result | Gate |
|---|---|---|---|---|
| `FBTransmittanceStage` | `.h/.cpp` | compute | `TransLUT` 256×64 rgba16float | always |
| `FBSkyViewStage` | `.h/.cpp` | compute | `SkyLUT` 192×108 rgba16float | always |
| `FBSkyStage` | `.h/.cpp` | render | sky dome + cloud-deck value-noise sheet | always (SVS pins day=1) |
| `FBSunStage` | `.h/.cpp` | render, additive | sun disc + forward glow | returns `vec4f(0)` outside EVS |
| `FBMoonStage` | `.h/.cpp` | render, additive | moon as an illuminated sphere; owns the NASA LROC albedo texture | as sun |
| `FBStarsStage` | `.h/.cpp` | render, instanced additive | HYG star field at true alt/az | self-gated: no SVS, no day, no catalogue → no draw |
| `FBTilesStage` | `.h/.cpp` | render | terrain (see §6) | always, once configured |
| `FBUnitsStage` | `.h` | — | **NoOp** | — |
| `FBTileLightsStage` | `.h/.cpp` | render, instanced additive | night-light sprites on the ground | self-gated like stars |
| `FBSpritesStage` | `.h` | — | **NoOp** | — |
| `FBCloudLayerStage` | `.h/.cpp` | render | the whole cloud chain: ray ∩ shell per deck, trapezoid march, premultiplied into `HdrTex` (see [`clouds.md`](clouds.md)) | armed AND the weather has a deck |
| `FBTonemapStage` | `.h/.cpp` | render | ACES → `FrameTex`; one pipeline | always |
| `FBHudStage` | `.h/.cpp` | render | HUD overlay (see §7) | `HudEnabled` |
| `FBUpscaleStage` | `.h/.cpp` | render | 720p → display resolution, bilinear | always |

In addition three pure **WGSL splice headers** (no classes, no independently compilable shader — the
consumer concatenates them in front of its own code):

| Header | Content | Consumers |
|---|---|---|
| `FBAtmoCommon.h` | `Atmo` uniform struct + scattering physics | transmittance, sky view, sky, sun, moon, tiles, clouds |
| `FBAtmoSample.h` | sky-view LUT sampling + exposure (`kSkyExposure = 8.0`) | sky, sun, tiles, clouds |
| `FBAtmoHaze.h` | **THE air**: Koschmieder σ₀, its molecular/aerosol split, the two scale heights (8 km / 1.2 km), `kMinSunUp`, the per-channel extinction law, the inscatter colour, and the deck's sun transmittance — C++ AND WGSL, like the density below. Its Rayleigh coefficients ARE `FBAtmoCommon.h`'s, read directly in WGSL and tied to the C++ mirror by a `const_assert` | `FBTilesStage`, `FBCloudLayerStage`, `--cloudcheck` |
| `FBCloudDensityWGSL.h` | the shared cloud density function plus the constants printed from its C++ half | `FBCloudLayerStage`, `--cloudcheck` |

---

### 4 The atmosphere (Hillaire 2020)

Two compute LUTs plus one fullscreen sky pass. The sky-view LUT is also what the terrain's and the
cloud deck's haze dissolve INTO (`FBAtmoHaze.h`), so sky, ground and cloud converge on one colour; the
transmittance LUT feeds sky, sun and the cloud march's sun colour, and since the `FB_AP` removal no
longer the terrain.

**Shared resources owned by `FBRenderer`** (because 3+ consumers read them):

| Resource | Format/size | Note |
|---|---|---|
| `TransLUT` | 256×64 rgba16float, `StorageBinding\|TextureBinding` | parameterised only by altitude × sun cos θ → needs no camera |
| `SkyLUT` | 192×108 rgba16float | single scattering, raymarched |
| `LutSamp` | linear, **U = Repeat** (azimuth wraps), V = ClampToEdge | |
| `AtmoBuf` | 11 × vec4 = 44 floats | see table below |

**Content of `AtmoBuf`** (`FBRenderer::UpdateAtmosphere`, order = layout):

| Index | Field | Meaning |
|---|---|---|
| 0 | `camPosMm` | eye in megametres (ECEF/1e6) |
| 1 | `sunDir` | sun direction ECEF |
| 2 | `up` | radial up at the eye |
| 3 | `sunTan` | sun direction, tangentially projected |
| 4 | `side` | `up × sunTan` |
| 5–7 | `camRight`, `camUp`, `camFwd` | **rolled** camera basis (only the view-ray reconstruction uses it) |
| 8 | `params` | `tan(30°)`, aspect, `cos(0.5°)` (sun angular radius), `30` (disc intensity) |
| 9 | `moonDir` | xyz direction, w = illuminated phase fraction |
| 10 | `skyExtra` | x = day factor, y = EVS gate, z = cloud cover, w = lunar angular radius `0.0045 × FB_MOON_SCALE` |

**Atmosphere constants** (`FBAtmoCommon.h`, Hillaire's standard values): `groundRadiusMM = 6.360`,
`atmosphereRadiusMM = 6.460`, Rayleigh base `(5.802, 13.558, 33.1)`, Mie scattering `3.996`, Mie
absorption `4.4`, ozone `(0.650, 1.881, 0.085)`.

**Day factor** (`DaylightFactor`, verbatim from the predecessor code `atmo.h::w3_daylight`):
`t = clamp((sunElDeg + 9)/12, 0, 1)`, then `t²(3−2t)` (smoothstep). Full day above ≈ +3°, dark from
≈ −9° (nautical twilight). ONE number for sky, ground and star fade.

**SVS vs. EVS** (the TAB toggle):

| Mode | Ground source | Sun | Additional effects |
|---|---|---|---|
| SVS (`GroundPhoto = 0`) | OSM render | fixed 45° elevation, azimuth 180° | day factor pinned to 1, no stars/lights/clouds |
| EVS (`GroundPhoto = 1`) | aerial imagery | real ephemeris (`core/FBEphemeris.h`, via `SetHud`) | stars, night lights, clouds, moonlight |

Rationale in the code: SVS is a **time-independent database view**; only EVS is "the real camera", so
only there does dawn/dusk have to match the picture's brightness.

**Init-order contract** (`FBRenderer::CreateAtmosphere`, documented explicitly as a CONTRACT): WebGPU
bind groups pin a concrete `TextureView` at creation — there is no "rebind later". So the stages have
to be configured in dependency order:

```
Transmittance (owns TransLUT)
   └─ SkyView   (reads TransLUT, writes SkyLUT)
        └─ Sky  (reads SkyLUT + AtmoBuf)
   ├─ Sun       (reads TransLUT for the sun colour + AtmoBuf)
   └─ Moon      (builds the albedo texture from the bytes staged by SetMoonTexture + AtmoBuf)
…only then CreateTerrainPipeline() → FBTilesStage::Configure(… LutSamp, SkyLUT …)
```

The terrain takes the **LUT sampler**, not the albedo one, and that is a correctness detail rather
than tidiness: the sky-view LUT wraps in azimuth (`AddressMode::Repeat`) and its seam sits at u = 0 —
which is the SUN's own azimuth. The removed `FB_AP` block sampled it with the ClampToEdge tile
sampler, i.e. with a filtered seam through the brightest part of the far field.

Ephemerides (`core/FBEphemeris.h`, pure functions — moved down out of `render/` in the C2 round
because `core/`/`sensors/` may not include `render/` and visual acquisition needs the sun,
[`../missions/syntax.md`](../missions/syntax.md)): `FBSunPos` is a verbatim port of the NOAA
approximation formulas (< ~0.5° error); `FBMoonPos`/`MoonPhase` are a port of Paul Schlyter's
approximation (public domain) **without** its long perturbation-term table — good to about a degree,
enough for a disc plus phase, not for navigation. The phase is `(1 − cos(elongation))/2`.

---

### 5 The cloud chain

Moved out into [`clouds.md`](clouds.md) — the cloud chain has a spec of its own (rebuild) and a state
of its own.

---

### 6 The terrain stage in detail

`FBTilesStage` is the only stage with real per-frame CPU state.

**Vertex layout** (`render/FBChunkVtx.h`, `w3_vtx`): `pos[3]`, `uv[2]`, `norm[3]` = 32 B, tightly
packed, with a `_Static_assert` on size and offsets 0/12/20. The reason is in the header: writer (tile
worker) and reader (draw call) used to agree only **by hand** — when the normals were added, the stride
moved from 20 to 32 and every hand-written number had to move with it; an error there renders garbage
instead of breaking.

**Geometry**: `pos` is the ECEF **offset to the tile origin** (float; at z14 < 2 km → sub-centimetre),
`origin` is the double anchor the frame subtracts. The construction is in `render/FBChunkMesh.h`
(`w3_chunk_build_ecef`): the same regular grid structure as the ENU path, but every node is projected
through the exact inverse Mercator and geodetic→ECEF — no tangent-plane error, no dependency on a home
origin. Normals are real ECEF cross products of the neighbouring offsets and therefore carry the tile's
curvature for free. `err` = maximum height error of the decimation in metres — projection-independent,
and exactly the number the streamer's LOD builds on.

**The frame uniform** (`U`, 36 floats = 144 B, written once per frame in `Encode`): the camera-relative
MVP + sun (the `FBFrameContext`'s own `Mvp20`), then the ATMOSPHERE as numbers —
`haze = (σ₀, camera altitude ASL, ground radius under the camera in Mm, –)` and one `vec4` per weather
deck, `(baseM, topM, sun optical depth, cover)`. The decks arrive through `FBRenderer::SetCloudSky`,
which hands the SAME `FBCloudSky` to the cloud stage and to this one — the seam that makes "deck and
ground see one atmosphere" a fact of the wiring rather than a claim.

**Light and air in the fragment shader** (all of it out of `stages/FBAtmoHaze.h`, shared verbatim with
the cloud march):

| Step | Formula | Note |
|---|---|---|
| deck attenuation | `sunThru = Π_i [(1−cover_i) + cover_i·exp(−τ_i·frac_i)]`, `frac_i = clamp((top_i − z_frag)/thick_i, 0, 1)` | statistical, per deck, per fragment — **not** a shadow map. `cover` is calibrated to be an area fraction, so `1−cover` really is the share of sun rays that miss the deck; `frac` is what makes a ridge inside the deck partly lit and one above its top fully lit |
| lit albedo | `albedo · (0.4 + 0.15·(1−sunThru) + 3.0·N·L·sunThru) · light` | the direct term is what a deck takes away; `0.15` is what it gives back as diffuse (derivation in the shader comment: overcast diffuse illuminance ≈ 1.0–1.5× clear-sky diffuse) |
| haze | `T₁₃ = exp(−[β_R·exp(−z̄/8000) + σ_A·exp(−z̄/1200)]·d)`, `c = c·T + inscatter·(1−T)` | σ₀ = 3.912/visibility (Koschmieder) split into a fixed molecular part β_R and the aerosol remainder σ_A; two scale heights, and β_R is a **vec3** (λ⁻⁴) so the extinction colours. `z̄` = mean altitude of the sight line, `d` = the camera-relative fragment distance. Derivations + citations: [`clouds.md`](clouds.md) |
| inscatter | `skyViewSample(dir↓horizon) + the sun halo × EVS` | identical to what the sky pass paints, so the horizon has no edge; a below-horizon direction is projected onto the horizon, because the LUT's below-horizon branch answers a different question (`FBAtmoHaze.h` comment) |

Telemetry: `FBLog::Debug("render","terrain_light")` on change — `visM`, `sigma0PerM`, the three decks'
`τ`, `groundSunThru` and `groundLitFrac` (the share of ground radiance that is direction-dependent
direct light). That last number is how "flat and grey under a closed deck" is measured instead of
eyeballed: 0.882 clear → **3.6·10⁻⁶** under 100 % low cover.

**Per-draw data** (storage buffer `TileBuf`, one `Tile{a: vec4f, b: vec4f}` per draw, 32 B):

| Field | Meaning |
|---|---|
| `a.xyz` | `origin_ecef − cam_ecef` (float, camera-relative) |
| `a.w` | layer in the albedo array |
| `b.x` | per-tile brightness gain of the imagery (1.0 for OSM) |

The draw selects its entry via `firstInstance`, so `instance_index == draw index`.

**Albedo**: `texture_2d_array`, edge length 512 (client's choice), **growing** up to the adapter's real
`maxTextureArrayLayers` (target device 2048; default cap 256). What is uploaded is always a **complete
sRGB mip pyramid** (`render/FBMips.h`: colour averaging in LINEAR light — decode, average, re-encode,
so that distance does not darken; alpha is already linear). Two layers per tile are possible: the
eagerly baked **base** layer and the lazily loaded **photo/overlay** layer.

**Sampler** (`FBRenderer::CreateTileTexture`, shared with the tonemap): ClampToEdge (a bake IS a tile —
nothing wraps), linear/linear, **trilinear** over the mip chain, `maxAnisotropy = 16`.

**The grazing mip bias** (terrain fragment shader): at grazing view angles the UV footprint anisotropy
exceeds the 16:1 hardware cap → vertical streaks. Correction:
`gbias = clamp(1.0 · (−log2(grazeV) − 2.5), 0, 1.2)` with `grazeV` = downward component of the view
ray. It only kicks in below ≈ 10° of depression, cap 1.2 (≈ 2.3× the footprint). The comment calls this
explicitly a **user decision** (2026-07-23): sharpness beats freedom from streaks; a residual streak in
the outermost horizon band is accepted.

**Render bundle**: signature = FNV-1a over the draw **structure** (count, bind-group handle after an
array growth, per tile the vertex-buffer handle + vertex count). `TileBuf` and uniform **contents**
change every frame, but the bundle references those buffers only by handle — which is why only a
structural change triggers a re-record (a few per second on approach, **zero** when parked). Counter:
`GetBundleRecords()`.

**Two-phase commit**: a tile may only be drawn **one pass AFTER** its GPU upload
(`FrameNo > PhotoUpTick + 1` for the overlay layer; the streamer side mirrors this, see
`world-and-terrain.md`). Otherwise a draw references a layer whose `WriteTexture` is not yet visible →
a black frame.

**Invariant counters** (once per second as `FBLog::Debug("render","present")` with a `violation` flag):

| Counter | Meaning — should stay 0 |
|---|---|
| `notReadyDraws` | a draw without a committed layer |
| `wrongModeDraws` | SVS showed EVS or vice versa (mode bleed) |
| `blackDraws` | layer index < 0 |

---

### 7 The HUD backend

Moved out into [`hud.md`](hud.md) — geometry buffer, WebGPU backend and font system.

---

### 8 Camera and ground truth

#### 8.1 `FBCamera.h` — attitude in, basis out

The file is deliberately **pure maths**: no GL, no globals, no side effects. It deliberately does
**not** compute the eye POSITION — that depends on the terrain and belongs to the tile side.

Render space is ENU with `E = +X`, `up = +Y`, `N = −Z`. The basis comes from yaw/pitch, then it is
rolled about the forward axis:

```
up = u·cos(roll) + s·sin(roll)
sr = s·cos(roll) − u·sin(roll)
```

The `+s` is the place that **mirrors silently**: it once read `−s`, and a right bank looked like a left
bank. Nothing crashed, nothing looked broken — a tilted horizon is a tilted horizon in either
direction. Precisely for that reason the logic exists ONCE (`w3_basis_from`) and is used by both paths
instead of copied.

**`FBCameraBasisEcef`** is the ONE attitude→ECEF basis that native and WASM share. It used to exist
character-identically twice — in `FBAppNative.cpp` and `FBAppWasm.cpp`, each with a comment asking that
it be kept identical to the other. That made "the oracle's frames" and "the browser's frames" the same
camera only **by hand**. It computes in **doubles** (the eye lies in the 6.4e6 m range) and rotates the
render basis into ECEF via `FBEnuAxesEcef` (`core/FBGeodesy.h`, closed form, right-handed, det +1 →
triangle winding is preserved); the render→ENU mapping is `(e, n, u) = (x, −z, y)`.

The renderer knows three camera sources, in this order of precedence:

| Source | Setter | Property |
|---|---|---|
| Full basis | `SetCameraBasis(eye, fwd, right, up)` | carries **roll** — the horizon tilts in a bank. Wins over both others. |
| Scripted camera | `SetCamera(eye, target)` | up is derived radially, **no** roll |
| Default orbit | — | circles above the terrain field's centre (1500 m high, 6500 m radius, 0.2 rad/s) — bring-up/still image only |

**Horizon dip** (`w3_horizon_dip_rad`): `acos(R/(R+h))` with `R = 6 371 000 m` (mean spherical radius —
the terrain stands on the WGS84 ellipsoid, but the dip is a small-angle spherical result for which the
mean radius is the standard value). A HUD horizon drawn level reads too HIGH at altitude; it has to
fall by exactly this angle so that it overlays the real, curved-away horizon (MIL-STD-1787, conformal).
It is consumed by the symbology (`systems/FBDisplaySystem`,
`modules/f16/displays/FBF16Hud`), not by the terrain pass — there the curvature arises by itself,
because the ECEF tiles really do tilt away.

#### 8.2 Eye height from the model geometry

The eye height at ground level does **not** come from a fixed number but from JSBSim's gear geometry:
`FBFdm::GetGroundClearanceM(gearDown)` (`sim/src/fdm/FBFdm.cpp`) walks all ground-reaction units and
takes the largest `GetBodyLocation(3)` (body z, positive downward, ft below the CG) — with
`gearDown = false` only non-retractable contacts count (the belly).

It is used at the spawn seam: if `HeightOffsetM < 0` ("sit on the gear"), the boot path sets the
altitude to `GroundElevM + GetGroundClearanceM(true)` after the first `RunIC()` — once the CG is valid
— and runs the IC again. The spawn altitude is thereby geometry-true instead of guessed; there is no
"held→live" jump.

The camera itself is, in today's client, **the aircraft's eye**: `FBGeoToEcef(pose)` →
`SetCameraBasis`. An additional clamp "never below the surface" is **no longer wired** in the client
code — see *Gaps*.

#### 8.3 "Crash → engine off, no freeze"

The physical judge (`core/FBFlightMonitor`) belongs to the client, not to the module. When it trips,
`units/FBSimUnit::RunMonitors` cuts the engine **through the same controls path a pilot would use**
(`Module_->Controls().EngineCutoff()`) — and does nothing else. No freeze, no special case in the
renderer: JSBSim's own ground reactions keep computing, the wreck slides, the renderer draws it. The
same coupling applies to damage (`ApplyDamageToAirframe`: cutoff, throttle cap, control authority,
additional drag). A mission FAIL verdict cuts the engine ONLY if the unit was still combat-effective —
for a kill that would be the verdict acting on the aircraft instead of the damage.

---

### 9 The rule `grep -c 'R"(' FBRenderer.cpp` == 0

**Satisfied today: 0.** Not a single raw WGSL string literal remains in `FBRenderer.cpp`.

What the rule states: the render stage split is **complete**, and verifiably so with a one-liner rather
than with a judgement. Every piece of WGSL lives in exactly one `stages/` file; `FBRenderer` is
therefore **only** an orchestrator — device, targets, pass boundaries, order, shared resources. A new
shader idea can no longer wander "just quickly" into the orchestrator, because the rule would make it
visible immediately.

Distribution today: **13 WGSL blocks over 12 stage files** (one carries two: `FBHudStage`, stroke and
text) plus **4 pure splice headers** without a class of their own (`FBAtmoCommon.h`, `FBAtmoSample.h`,
`FBAtmoHaze.h`, `FBCloudDensityWGSL.h`). Two of the four carry a C++ half of the same formula and are
checked against their shader twin by `gpu_native --cloudcheck`.
