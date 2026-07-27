# Clouds — the render chain

**Origin:** moved out of `rendering.md` §5 (state `793e1fe`), taken over unchanged. Neighbouring
files: [`renderer.md`](renderer.md) (the pass topology the chain hooks into), `../world-and-terrain.md`
(weather source, once wired).

## Spec

**Rebuild, specified by the project owner** (roadmap R5). The six existing `FBCloud*` stages are
**demolition material, not a base.** What the owner asks for: how much and how far you can see,
flying through with consequences, and the fog underneath you.

**Bounded-volumetric, but simple:**

| Element | Specification |
|---|---|
| Density function | ONE separable function per deck: 2D coverage FBM (wind-advected from the weather provider) × analytic vertical profile, optionally a small 3D erosion noise generated at startup (~64³) |
| March | only inside the layer band: ray ∩ spherical shell analytically → at most three short segments, 6–12 steps per segment, blue-noise jitter, 16F accumulation, dither at the output (the banding recipe) |
| Light | no secondary march: Beer over the remaining thickness in closed form from the profile + 2–3 sun taps into the 2D field, ambient from the existing sky LUT |
| Composition | full resolution, ONE pass, no temporal, no bakes, `t1` clamped to scene depth (clouds in front of mountains, fog below the jet over terrain for free) |
| Camera inside the band | the segment starts at the camera — ONE code path for outside/inside/transition, fly-through seamless by construction |
| Explicitly NOT | impostors. Price: broken cumulus reads as a patchy sheet. Accepted. |
| Shared evaluation | the density function must be evaluable in WGSL **and** C++ (shared constants) — "how far can I see" is the same question sensors/IR will ask later |

Acceptance: a frame proof through the deck (fly-through without a seam), and a C++ evaluation of the
same function agreeing with the shader for a given sample set.

### High layer (cirrus) — why 2D is the RIGHT approximation

A sheet a few hundred metres thick at ~9 km has negligible parallax from any normal viewpoint, so a
2D advected field is **physically apt, not a cheat**. The one weak angle — edge-on at layer altitude —
is covered by the same unified shell logic as the other decks: the high layer gets its real thickness
(a few hundred metres), and the bounded march takes **1 analytic tap when far away, 2–4 steps near
layer altitude**. Same shell as low and mid, only thinner. No special case.

The feared "soft blobs" look is a property of **isotropic FBM**, not of the 2D method. Cirrus reads as
cirrus through three cheap properties:

| # | Property | How | Cost |
|---|---|---|---|
| 1 | **Fibres along the wind** | stretch the noise domain 5–10:1 along the `/wx` wind vector at 250 hPa — which *is* cirrus altitude, so the streak orientation comes from the real jet stream instead of an invented direction | zero, only the sample coordinate |
| 2 | **Hooks and fall-streaks** ("mares' tails") | domain-warp the sample position with a second low-frequency noise, and shear the high-frequency octave against the low-frequency one along the wind | +2–3 taps |
| 3 | **Sharp edges** | steep smoothstep remap instead of raw FBM; coverage drives the character — low coverage → hard remap, discrete streaks (cirrus uncinus); high coverage → soft remap, closed veil (cirrostratus). Both are real forms of the same étage. | zero |

Sharpness is **procedural** (3–4 octaves evaluated in-shader), not texture magnification — there is no
resolution ceiling to run into.

Lighting: the layer is thin, so forward scattering dominates; the HG phase term brightens the sun side
to silver — a large part of what the eye accepts as a real high cloud.

**Explicit non-goal:** single dramatic formations (towering Cb, anvil, storm front) are a **different
object** than a layer. If they are ever wanted, they become a fourth thing of their own, never an
extension of the three étages — recorded under Gaps as a deliberate omission.

## State

Six `FBCloud*` stages exist and work (three bake once, two run per frame, one is an init helper); the
whole branch is only built when `FB_CLOUDS=1`.

Owner's verdict: **expensive and ugly** — the chain is demolition material, not a base. Default is off
(`FB_CLOUDS=0`, user decision 2026-07-23), and no weather source is wired: `FBState.Env.Cloud*` stays
zero, and `FBCloudMarchStage` reads "0" as "no weather report" and sets its own default deck.

The distilled description of the existing chain is kept below under Knowledge — it is the record of
what is being replaced, plus the studies in `doc/clouds/01`–`10`.

## Gaps

### Open work (from the retired `TODO.md` §4)

| # | Thing |
|---|---|
| 4.4 | clouds off by default **and no weather source wired** — the rebuild depends on the weather provider of roadmap R4 |
| 4.9 | the rebuild itself: the Spec above is the contract, nothing of it is built |

Order: R2 (server `/wx`) → R4 (`FBWeatherProvider`) → R5 (this rebuild). Building the marcher before
the data source would mean tuning against an invented sky.

### Deliberate omissions of the rebuild

| Thing | Why |
|---|---|
| No impostors | accepted price: broken cumulus reads as a patchy sheet |
| **Single dramatic formations** — towering Cb, anvil, storm front | a formation is a different object than a layer. Not an extension of the three étages; if ever wanted, a fourth thing of its own with its own spec. |
| No temporal accumulation, no bakes | one pass, full resolution — the banding is handled by jitter + dither instead |

### Inventory (from the previous `Open points` section)

(see [`renderer.md`](renderer.md) — the collected list of the renderer round is preserved there in
full; the points that belong here are above under Gaps.)


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### The existing chain

Six classes, one per shader. **Three bake once, two run per frame, one is an init helper.** The whole
branch is only built when `FB_CLOUDS=1` — otherwise it costs neither boot time nor VRAM.

```
FBCloudMipDownStage  (shared box downsample, init time)
   ├─ FBCloudBaseBakeStage   128³ Perlin-Worley  ─┐
   ├─ FBCloudDetailBakeStage  32³ Worley         ─┤
   └─ (FBCloudCellBakeStage   512² F1 cells)     ─┤
                                                  ▼
                                        FBCloudMarchStage  → CloudLowTex (quarter resolution)
                                                  ▼
                                        FBCloudResolveStage → CloudHist/CloudWSum (ping-pong)
                                                  ▼
                                        FBTonemapStage (composite)
```

Bind-group order (for the same pinning reason as with the atmosphere): first the bakes, then march
(whose bind group pins their views), then resolve (which pins march's `CloudLowTex` view).

**The march** (`FBCloudMarchStage`):

| Quantity | Value | Provenance |
|---|---|---|
| Target resolution | `Width/4 × Height/4`, rgba16float | cost budget; the resolve reconstructs from it |
| Shell radii | absolute: `groundR = \|eye\| − AltM`, `rBase = groundR + baseAGL`, `rTop = rBase + thick` | deliberately computed against the REAL WGS84 ground, not against Hillaire's simplified 6360 km |
| Default base | 8000 m AGL (a high, broken cellular layer) | accepted setting 2026-07-23; `FB_CLOUD_BASE_M` sweeps it |
| Thickness | `2600 + 1400 · CloudHigh` m | a setting; `FB_CLOUD_THICK_M` |
| Material | density 18, extinction 0.06, sun intensity 18, detail 1.3 | settings, overridable via `SetCloudLab` |
| Coverage | max(CloudCover, Low, Mid, High); 0 → 0.4 in EVS | "no weather report" = a presentable default deck |
| Screen jitter | exact 4×4 sub-raster, `FrameNo % 16` | every full-resolution sub-pixel gets a direct sample exactly once every 16 frames |
| Ray dither | `frac(FrameNo · 0.6180339887)` | golden ratio against banding |
| Wind drift | `nowSec · 8` (km scale) | a setting |
| Cell field | 40 km per tile (≈ 4 km cells), dome subtraction 0.5 | `FB_CELL_KM` / `FB_CELL_DOME` |

Optional GPU timing: if the device feature `TimestampQuery` is present, **both** indices bracket the
march pass (only this one — leaving one index at `kQuerySetIndexUndefined` makes this Dawn build
discard the whole command buffer). Resolve before `Finish`, poll after `Submit`; the mean is logged
every 120 frames.

**The resolve** (`FBCloudResolveStage`): reprojection of the history over the camera motion at the
**shell midpoint** (`CloudMidR = (rBase + rTop)/2`), two ping-pong pairs (`CloudHist` rgba16float,
`CloudWSum` r32float = accumulated splat weight per full-resolution pixel). `ResetHistory()` and
`SetAccumMode(true)` (a true 1/N mean instead of an exponential blend) are lab tools for the parameter
sweeps.

**The composite** lives in the tonemap, not in the cloud: `scene = scene·(1−cl.a) + cl.rgb`
(premultiplied) before the ACES fit. `FBTonemapStage` therefore holds **two pipelines from one source**
— the plain variant does not bind the cloud texture at all, so the disabled path cannot sample stale
history either. Which one applies is a boot-time constant and is never switched mid-run.

Deeper derivations on noise, density, lighting, march strategy, temporal reprojection and the iGPU
budget are in `doc/clouds/01`–`10`.
