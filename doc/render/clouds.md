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

**Built (roadmap R5).** The six `FBCloud*` stages are gone; the chain is ONE class,
`render/stages/FBCloudLayerStage`, over ONE shared density function, `core/FBCloudDensity.h`. Armed by
default; `FB_CLOUDS=0` disarms it at boot.

| Piece | Where | What it is |
|---|---|---|
| The density function | `core/FBCloudDensity.h` | C++ half: hashed gradient noise, the coverage FBM, the analytic column, the erosion, plus `FBCloudSkyFromWeather` (the three decks from one `FBWeatherProvider` sample). Includes nothing above `core/` — `verify-layers` holds it there. |
| The same function in WGSL | `render/stages/FBCloudDensityWGSL.h` | a literal transliteration whose **constants are printed from the C++ ones** (`FBCloudDensityConstsWGSL()`), so a number cannot drift between the picture and a measurement |
| The stage | `render/stages/FBCloudLayerStage.{h,cpp}` | ray ∩ shell per deck, ≤ 3 segments front-to-back, 6–12 steps, blue-noise entry jitter, 2 sun taps, HG phase, sky-LUT ambient, Koschmieder haze, dither out. Blends **premultiplied straight into `HdrTex`**. |
| The weather seam | `clients/FBAppNative.cpp`, `clients/FBAppWasm.cpp` | the CLIENT samples `FBWorld::Weather()` where the camera is and hands the renderer an `FBCloudSky` (`FBRenderer::SetCloudSky`). The renderer never sees a provider; the same call is what a future IR sensor makes. |
| The numeric gate | `gpu_native --cloudcheck` | evaluates both halves over 12 288 samples and prints the largest disagreement; also measures the FBM distribution the coverage remap is calibrated against |
| The frame gate | `sim/missions/wx-clouds-proof.fbm` | four legs above / inside / below one real GFS deck, for `gpu_native --mission --interval` |

**Pass topology.** The cloud pass is a separate render pass because it must SAMPLE the depth texture
that was an attachment a moment earlier; it writes back into `HdrTex` with premultiplied blending, so
`FBTonemapStage` has nothing to composite any more and collapsed from two pipelines to one. Per frame:
**6 passes with no weather, 7 with a deck** (was 6 / 8), and the pass exists only when the weather
actually has a deck — `render/passcount` logs `passes`, `clouds` and `cloudPass` so a frame's topology
is readable from the telemetry.

**No weather = no cloud.** The old chain invented a default deck when `FBState.Env.Cloud*` was zero;
this one does not. A mission without a `wx` line renders exactly as before the rebuild.

### Measured (Apple A18 Pro, native Dawn, 1280×720, `--albedo photo`, 600 frames per run)

| Configuration | ms/frame | cloud cost |
|---|---|---|
| clouds off (`FB_CLOUDS=0`) | 3.0–3.7 | — |
| **R5, one stage, FULL resolution** | 11.9–12.7 | **≈ 8.8 ms** |
| the old six-stage chain, QUARTER resolution + temporal resolve | 25.8–26.8 | ≈ 23 ms |

Same scene, same camera, same cover: **2.6× cheaper than the chain it replaces while marching four
times as many pixels**, with no bake at boot and no history textures. The scene is deliberately the
worst case — the camera sits just above a 76 %-cover deck, so nearly every pixel marches a full
segment. 8.8 ms is still more than half a 60 Hz budget on this iGPU; `SetCloudQuality` scales the step
count (0.25…8) and is the knob for it.

| Other measurement | Value |
|---|---|
| C++ ↔ WGSL density agreement | max |Δ| **1.87·10⁻⁵**, mean 4.3·10⁻⁷ over 12 288 samples (3 decks, ±300 km, h ∈ [0,1] incl. the endpoints); tolerance 10⁻⁴ |
| coverage remap calibration | FBM mean 0.4999, σ 0.1323 measured over 40 000 samples; requested cover 0.75/0.40/0.95 → realised area 0.733/0.408/0.960 |
| cirrus streak axis | field axis **133.4°** vs the fixture's 250 hPa wind **137.2°** (3.8° residual = the per-octave shear); orientation coherence **0.949** anisotropic vs **0.261** for the same field at stretch 1 |
| banding (dusk scene, smooth 500×160 px cloud region) | jitter+dither **88 levels, longest run 3 px, 0.9 % identical neighbours**; the same frame without them **83 levels, longest run 70 px, 61.6 % identical neighbours** |
| telemetry | 14 telemetry CSVs over 7 missions byte-identical to the base tree; `events.log` identical except `wallS`/`speedup` |

### Frame proofs

Deterministic in their LIGHTING and their weather: pinned `--utc`, the committed
`assets/wx-2026-07-27T00Z.wxb`. NOT in their terrain — the screenshot venue streams tiles while it
renders, so a short run frames a half-built quadtree and the deck marches to the far plane instead of
onto ground. A frame is only comparable to another once `fbworld` reports `pending=0`; the p1 camera
needs ~180 frames for that (`--seconds 3.0 --interval 0.75`). Written to `sim/build/r5-proofs/`
(gitignored).

| # | File | What it shows |
|---|---|---|
| 1 | `p1-undercast-from-above.png` | 46.84 N/6.92 E, 8 450 m, −35°: the 76 % deck from above, terrain and a lake through the gaps, the far field dissolving into the reported 24 km visibility |
| 2 | `p2-flythrough/mission_00{28,36,44,68}.png` | the fly-through: above → entering (ceiling overhead, deck below) → inside (white-out) → below (terrain back, clear air) |
| 3 | `p3-underside-into-sun.png` | 2 800 m, +40° toward the sun: the base damped by Beer, with the HG forward lobe as a silver patch around the sun direction |
| 4 | `p4-cirrus-horizon.png`, `p4-cirrus-plan.png`, `p4-cirrus-field-plan.png` | 49 N/14 E, 64 % high cloud: fibres along the 250 hPa wind, not isotropic mush. The third is the C++ half of the same function dumped as a plan view — that is where the 133.4° above is measured, free of the perspective and of the mid deck underneath. |
| 5 | `p5-dusk-banding.png` vs `p5b-dusk-nojitter-nodither.png` | the banding A/B above |

## Gaps

### Open work

| # | Thing |
|---|---|
| 5.1 | **Residual march noise.** 6–12 steps against an optically thick deck put the first-hit position one step apart between neighbouring pixels; the jitter turns that into grain rather than banding, but the grain is visible (std/mean 0.043–0.070 over a flat deck). No temporal resolve exists to average it, by spec. Options if it must go: a single bisection refinement at the entry, or a fourth "quality" step budget. |
| 5.2 | **One ceiling, three decks.** GFS reports ONE ceiling; the rule spends it on the lowest broken deck and CLAMPS it into that étage's band. When the reported ceiling walks out of the band along a track the base sticks at the clamp instead of following — visible as a deck that stops rising. The alternative (rejecting it) was measured and is worse: a 2.7 km jump in one frame. |
| 5.3 | **Undercast relief is gentle.** The sun taps span at most one deck thickness horizontally, which is physically right for a 900 m deck, so a closed deck from above reads flat and bright. Cumulus-topped decks would need a taller column model, which is the `kCloudTopMin` knob and nothing else today. |
| 5.4 | **The mid deck has no character of its own** — it is the low deck's constants with a bigger feature size. Altocumulus banding (its actual signature) is not modelled. |
| 5.5 | **The stored proof PNGs are older than the source.** `p1-undercast-from-above.png` was captured mid-tuning and the committed tree no longer reproduces it (99.9 % of pixels differ). Re-measured on the merge with `origin/systems`: the pre-merge R5 binary and the merged one write BYTE-IDENTICAL frames for both the 4-frame and the 180-frame p1 invocation, so the drift is tuning history, not the merge. The whole set wants one re-capture run against the committed source. |

### Deliberate omissions of the rebuild

| Thing | Why |
|---|---|
| No impostors | accepted price: broken cumulus reads as a patchy sheet |
| **Single dramatic formations** — towering Cb, anvil, storm front | a formation is a different object than a layer. Not an extension of the three étages; if ever wanted, a fourth thing of its own with its own spec. |
| No temporal accumulation, no bakes | one pass, full resolution — the banding is handled by jitter + dither instead |
| **No 64³ erosion texture** | the spec allowed one *optionally*; erosion is instead two procedural 3-D octaves in the shader. That keeps "no bakes" literal AND makes the C++ mirror exact — a texture would have had to be reproduced bit-for-bit on the CPU side for `--cloudcheck` to mean anything. |
| Clouds draw in SVS as well as EVS | the old chain gated the march on the EVS flag. Weather is weather; a database view of the terrain is not a reason to delete the sky. The SVS sun (fixed 45°/180°) lights them, which is what makes the SVS proof frames deterministic. |
| No cloud shadows on the terrain | a second consumer of the same function (the terrain shader would sample it toward the sun); not in this round |

### Inventory (from the previous `Open points` section)

(see [`renderer.md`](renderer.md) — the collected list of the renderer round is preserved there in
full; the points that belong here are above under Gaps.)


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### The density function, in full

Per deck, `FBCloudDensity(deck, eastM, northM, h)` where east/north are metres in the tangent plane of
a fixed anchor and `h ∈ [0,1]` is the height fraction inside the deck. Separable, exactly as specified:

```
  x, y   = (east - driftE) / featureM , (north - driftN) / featureM     advection
  a      = ( x·wE + y·wN) / stretch                                     along the wind, stretched
  b      =  -x·wN + y·wE                                                across the wind
  a,b   += (noise(a,b · warpFreq) - ½) · warp · {1, warpCross}          domain warp (mares' tails)
  f      = Σ_{i<4} 2^-i · noise( a + shear·i·b , b )  / Σ 2^-i          FBM, sheared and ROTATED per octave
  c      = smoothstep(edge - width, edge + width, f)                    the coverage remap
  top    = mix(topMin, 1, c)                                            a stronger column reaches higher
  dens   = c · smoothstep(0, profBase, h/top) · smoothstep(1, 1-profTop, h/top)
  dens  -= erosionFbm(a·erodeFreq, b·erodeFreq, h·erodeVert) · erosion · mix(erodeBase, 1, h)
```

Three things in there are not decoration, and each was forced by a measured artefact:

1. **Gradient noise, not value noise.** Value noise puts its extrema on the lattice points, so the
   coverage threshold draws the lattice as square cells.
2. **Perlin's own gradient sets** (8 directions in 2-D, the 12 cube edges in 3-D). Four diagonal
   (±1,±1) gradients still draw an axis-aligned grid once the remap amplifies the finest octave.
3. **A per-octave rotation** by the 3-4-5 angle (cos 0.8 / sin 0.6, exact in binary floating point).
   Without it every octave shares one lattice.

And one that is not in the field at all but in the *interface*: **the advection offset is wrapped**
(`kCloudDriftWrapM = 4·10⁶ m`). Handing this function a unix timestamp as its time produces a drift of
~1.8·10¹⁰ m, at which point one f32 ulp of the finest octave's coordinate is a whole lattice cell — the
fractional part is gone and the field degenerates into flat rectangles. That was the single worst
artefact of the round and it is not a shader bug; it is an argument-range bug.

**The remap is calibrated, not guessed.** `FBCloudCalibrate` places the threshold against the FBM's own
measured distribution using the logistic approximation of the normal quantile,
`edge = μ − σ·ln(p/(1−p))/1.702`, so `Cover` is an AREA FRACTION (realised 0.733 for 0.75, 0.408 for
0.40, 0.960 for 0.95). The half-width runs from 0.35 σ at cover → 0 (discrete, hard-edged elements) to
2.2 σ at cover → 1 (a closed, soft veil) — the spec's "coverage drives the character".

### The constants

| Constant | Value | Provenance |
|---|---|---|
| `kCloudOctaves` / lacunarity / gain | 4 / 2 / 0.5 | [SET] |
| `kCloudShear` | 0.30 | [SET] — the fall-streak lean; costs 3.8° of streak-axis error, measured |
| `kCloudRotC/S` | 0.8 / 0.6 | [SET], exact in f32 |
| `kCloudFbmMean` / `Sigma` | 0.4986→0.5007 / 0.1331 | **measured**, 40 000 samples, `--cloudcheck` |
| `kCloudRemapHard` / `Soft` | 0.35 σ / 2.20 σ | [SET] |
| `kCloudProfBase` / `Top` / `TopMin` | 0.18 / 0.60 / 0.45 | [SET]; `Top` was raised from 0.35 because a sharp lid terraces under a 6–12 step march |
| `kCloudErodeOct/Freq/Vert/Base` | 2 / 10 / 3 / 0.35 | [SET] |
| `kCloudDriftWrapM` | 4·10⁶ m | derived from f32 precision, see above |
| deck defaults (base / thickness) | low 1 200 / 900 m, mid 4 200 / 1 400 m, high 9 000 / 500 m | [SET]; high = the spec's ~9 km, 500 m = "a few hundred metres" |
| étage bands for the ceiling | low 150–4 000, mid 2 000–7 500, high 5 500–13 000 m | [SET]; NOT the WMO limits — GFS' low-cloud diagnostic runs to ~642 hPa, and a 2.5 km low limit threw away a measured 2 991 m ceiling |
| feature size | low 16 km, mid 26 km, high 12 km | [SET] |
| stretch | 1.0 / 1.35 / **7.0** | [SET]; cirrus inside the spec's 5–10:1 |
| warp / erosion | 0.25/0.35/0.30 · 0.35/0.25/0.15 | [SET] |
| extinction σ at density 1 | 0.022 / 0.018 / 0.0060 m⁻¹ | chosen from the optical depth over the deck's own thickness: 20 / 25 (opaque) and 3.0 (a translucent veil = cirrostratus) |
| 250 hPa sample level | 10 800 m | [SET]; the level the spec names for the cirrus axis, and the fixture's own 250 hPa geopotential in mid-latitudes |
| `kSunIntensity` / `kAmbientFloor` | 18 / 0.18 | [SET] against `kSkyExposure` = 8 |
| `kMaxSegM` | 60 km | [SET] longest marched span through one deck |
| `kErodeFadeNear/FarM` | 8 km / 45 km | [SET] — erosion is the highest frequency and the first thing a 12-step march undersamples |
| haze | σ₀ = 3.912 / visibility, thinned by exp(−z/8000) | **derived**: Koschmieder + the ISA density scale height. The visibility comes from the same weather sample. |

### The march

One segment per deck (that is the spec's "at most three short segments"), from
`shellSegment(rIn, rOut)` — two quadratics, and the "camera below / inside / above" cases fall out of
them rather than out of a branch. `t1` is clamped to the scene depth, reconstructed from the
reversed-Z infinite projection as `t = zNear / (depth · cos)`, `zNear = 0.05 m`.

Step count `clamp(segLen / (thickness·0.35), 6, 12) · quality`: a near-vertical crossing gets 6, a
grazing one 12, and the step size adapts by itself. Entry offset = interleaved-gradient noise of the
pixel — **spatial only**, no frame term anywhere in the shader, because without a temporal resolve a
per-frame jitter is flicker rather than antialiasing.

Light, per lit sample, no secondary march: optical depth toward the sun in closed form over the
remaining thickness `(1−h)·thickness / max(sunUp, 0.20)`, times the mean of the local density and two
taps into the 2-D field at the horizontal legs of that slant path; then three Wrenninge multi-scatter
octaves against a dual-lobe HG phase (g = 0.8 / −0.5), a powder term, and hemispheric ambient from the
existing sky-view LUT (zenith + a warm horizon bounce toward the sun).

### The chain that was removed

Six classes — `FBCloudMipDownStage`, `FBCloudBaseBakeStage` (128³ Perlin-Worley),
`FBCloudDetailBakeStage` (32³ Worley), `FBCloudCellBakeStage` (512² F1 cells), `FBCloudMarchStage`
(quarter-res march), `FBCloudResolveStage` (temporal reprojection, ping-pong history + weight sum) —
plus `FBCloudNoiseCommon.h`, `FBTonemapStage`'s second pipeline, and the `--cloudlab` / `--cell`
parameter-sweep harness in `clients/FBAppNative.cpp`. Its studies stay in `doc/render/clouds-legacy/01`–`10` as the
record of what was learned; the code is gone. Its cost is in the table above.
