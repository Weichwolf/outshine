# Clouds — the render chain

**Origin:** moved out of `rendering.md` §5 (state `793e1fe`). Neighbouring files:
[`renderer.md`](renderer.md) (the pass topology the chain hooks into) and
[`../world/weather.md`](../world/weather.md) (the `/wx` source these decks are built from).

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

**Built (roadmap R5; re-measured and extended in the R5 follow-up round).** The six `FBCloud*` stages
are gone; the chain is ONE class, `render/stages/FBCloudLayerStage`, over ONE shared density function,
`core/FBCloudDensity.h`. Armed by default; `FB_CLOUDS=0` disarms it at boot.

Three things changed in the follow-up round, each with its measurement below: the march integrates by
**composite trapezoid** instead of a jittered rectangle rule, the erosion octaves are **band-limited
against the march's own step** (a new deck parameter `ErodeFlat` in the shared function), and the one
reported ceiling is spent on the étages by a **continuous ownership weight** instead of a clamp.

| Piece | Where | What it is |
|---|---|---|
| The density function | `core/FBCloudDensity.h` | C++ half: hashed gradient noise, the coverage FBM, the analytic column, the erosion, plus `FBCloudSkyFromWeather` (the three decks from one `FBWeatherProvider` sample). Includes nothing above `core/` — `verify-layers` holds it there. |
| The same function in WGSL | `render/stages/FBCloudDensityWGSL.h` | a literal transliteration whose **constants are printed from the C++ ones** (`FBCloudDensityConstsWGSL()`), so a number cannot drift between the picture and a measurement |
| The stage | `render/stages/FBCloudLayerStage.{h,cpp}` | ray ∩ shell per deck, ≤ 3 segments front-to-back, 6–12 **nodes** of a composite trapezoid, blue-noise entry jitter on the interior nodes, 2 sun taps, HG phase, sky-LUT ambient, Koschmieder haze, dither out. Blends **premultiplied straight into `HdrTex`**. |
| The weather seam | `clients/FBAppNative.cpp`, `clients/FBAppWasm.cpp` | the CLIENT samples `FBWorld::Weather()` where the camera is and hands the renderer an `FBCloudSky` (`FBRenderer::SetCloudSky`). The renderer never sees a provider; the same call is what a future IR sensor makes. |
| The numeric gate | `gpu_native --cloudcheck` | evaluates both halves over 12 288 samples and prints the largest disagreement; also measures the three constants that are claimed as MEASURED — the coverage FBM's distribution, the erosion FBM's mean (the value the band-limit fades toward), and the **cirrus streak axis** against the deck's own wind, with an isotropic control |
| The frame gate | `sim/missions/wx-clouds-proof.fbm` | four legs above / inside / below one real GFS deck, for `gpu_native --mission --interval` |

**Pass topology.** The cloud pass is a separate render pass because it must SAMPLE the depth texture
that was an attachment a moment earlier; it writes back into `HdrTex` with premultiplied blending, so
`FBTonemapStage` has nothing to composite any more and collapsed from two pipelines to one. Per frame:
**6 passes with no weather, 7 with a deck** (was 6 / 8), and the pass exists only when the weather
actually has a deck — `render/passcount` logs `passes`, `clouds` and `cloudPass` so a frame's topology
is readable from the telemetry.

**No weather = no cloud.** The old chain invented a default deck when `FBState.Env.Cloud*` was zero;
this one does not. A mission without a `wx` line renders exactly as before the rebuild.

### Does the everyday path actually show a deck?

The browser's default is LIVE weather (`clients/FBAppWasm.cpp` fetches `/wx` once per session) and its
default mission, `payerne-full.fbm`, declares no `wx` line — so the answer depends entirely on what GFS
reports where you fly. Measured against the committed fixture (`assets/wx-2026-07-27T00Z.wxb`) with
`build/fb-test-weather <blob> <lat> <lon> <alt>`, one probe per grid point:

| Survey | n | median total cover | ≥ 25 % in some étage | ≥ 50 % | nothing at all |
|---|---|---|---|---|---|
| global, 10° grid, 80 S…80 N | 612 | 94 % | 67.6 % | 63.2 % | 9.8 % |
| Europe, 35–60 N / 10 W–30 E, 2.5° | 187 | 7.5 % | 41.2 % | 39.0 % | 27.8 % |
| **the Swiss box** (45.9–47.9 N / 6–10.5 E, 0.25°×0.5°) — where the daily flying happens | 90 | 43 % | **60.0 %** | 41.1 % | 7.8 % |

So: **yes, and by a clear margin** on this run. Over the Swiss box the low étage carries a mean 39.7 %
cover with a ceiling reported at 71 % of points; at Payerne itself (46.84 N/6.92 E) the fixture reports
75.7 % low cloud with a 2 991 m ceiling — the undercast in `p1`. The distribution is strongly bimodal,
which is a property of GFS' own LCDC/MCDC/HCDC diagnostics rather than of anything here: a point tends
to be near 0 % or near 100 %, and the 27.8 % "nothing at all" over Europe is that same bimodality
sampled on a coarse grid, not a missing feature.

Two honest caveats. **One run is one atmosphere** — this is the 2026-07-27 00Z cycle, and a different
cycle would give different numbers; the survey shows the mechanism works, not that every session has
weather. And a mission that wants a GUARANTEED sky has to say so: `wx fixture wx-2026-07-27T00Z.wxb`
pins this atmosphere (`doc/missions/weather.md`), which today only `wx-clouds-proof`, `wx-gfs-fixture`
and `mig29-irst` do.

### Measured (Apple A18 Pro, native Dawn, 1280×720, `--albedo osm`, 600 frames per run, min of 5)

Same camera throughout: 46.84 N/6.92 E, 8 450 m, −35° over the fixture's 76 %-cover deck — deliberately
the worst case, nearly every pixel marches a full segment.

| Configuration | ms/frame | cloud cost |
|---|---|---|
| clouds off (`FB_CLOUDS=0`) | 3.68 | — |
| R5 as committed (jittered rectangle rule) | 13.22 | 9.5 ms |
| **R5 + trapezoid + erosion band-limit** | 13.67 | **10.0 ms** (+5 %) |
| the old six-stage chain, QUARTER resolution + temporal resolve | 25.8–26.8 | ≈ 23 ms |

Still **2.3× cheaper than the chain it replaces while marching four times as many pixels**, with no bake
at boot and no history textures. 10 ms is more than half a 60 Hz budget on this iGPU; `SetCloudQuality`
scales the node count (0.25…8) and is the knob for it.

| Other measurement | Value |
|---|---|
| C++ ↔ WGSL density agreement | max \|Δ\| **1.90·10⁻⁵**, mean 4.0·10⁻⁷ over 12 288 samples (3 decks, ±300 km, h ∈ [0,1] incl. the endpoints, and the `ErodeFlat` branch crossed at 0.0/0.55/1.0); tolerance 10⁻⁴ → **AGREE** |
| coverage remap calibration | FBM mean 0.4999, σ 0.1323 measured over 40 000 samples; requested cover 0.75/0.40/0.95 → realised area 0.733/0.408/0.960 |
| erosion FBM mean | **0.4988** (σ 0.162) over 40 000 samples against the constant 0.5 — the value `ErodeFlat` fades toward, so the band-limit removes the erosion's VARIANCE without moving its mean |
| cirrus streak axis (`--cloudcheck CIRRUS_AXIS`) | structure tensor over a 256² plan grid, 120 km span: field axis **56.3°** vs the deck's own wind axis **61.9°** → residual **5.6°** (the per-octave shear); coherence **0.942** at stretch 7 against **0.155** for the identical field at stretch 1 |
| **march grain** (p1 flat-deck box 900×200 px, high-pass std/mean) | committed **0.0329** (3×3) / **0.0357** (9×9) → now **0.0285 / 0.0324**, i.e. **−13.5 % / −9 %** |
| **march bias** (same box, deck luminance against the converged q = 8 render) | committed **−3.7 %**, now **+0.8 %** — the rectangle rule renders a thick deck too dark because it applies the entry density over a whole step and then early-terminates |
| banding (dusk scene, deck-only 650×180 px box) | committed 73 levels / longest identical run 10 px / 4.1 % identical neighbours; now **85 / 10 px / 5.3 %** — more levels, same run length, so the extra dither headroom is not spent on banding |
| telemetry | 14 telemetry CSVs over 6 missions (`payerne-full`, `wx-gfs-fixture`, `wx-clouds-proof`, `bvr-duel`, `attack-ccip`, `payerne-pair`) byte-identical to `HEAD`; `events.log` identical except `wallS`/`speedup` and the `--out` path |
| pass topology | unchanged: `passes=7 clouds=1 cloudPass=1` with a deck, `passes=6 … cloudPass=0` without |

### The two march changes, and why the grain only fell 13 %

The grain was attacked with a measurement first, and the measurement said something other than what the
gap text assumed. Removing the erosion term entirely dropped the grain from 0.0263 to **0.0149** — so
the erosion, not the profile ramp, is what a 6–12 node march undersamples. Its lattice is
`featureM/kCloudErodeFreq` = 1.6 km wide and `thickness/kCloudErodeVert` = 300 m tall against a ~260 m
step: at Nyquist looking down, comfortably resolved looking along the deck.

1. **Composite trapezoid** (`marchDeck`). Nodes at the segment's two TRUE ends plus the jittered
   interior, so the sum telescopes and the total optical depth depends on the jitter phase only at
   O(h²). It is worth its ~5 % because of the BIAS, not the grain: at equal node count the rectangle
   rule needs 12 nodes to reach the brightness the trapezoid reaches at 6 (measured above).
2. **`ErodeFlat`, the band-limit** (`core/FBCloudDensity.h` + `erodeFlatness` in the stage). Cells
   crossed per step — the same quantity a mip footprint measures — fades the erosion toward its own
   MEAN. Fading the AMPLITUDE instead measured better (grain 0.0201) and is **rejected**: erosion
   subtracts density, so an amplitude fade brightens the deck by 3.5 % wherever the filter bites, and
   the "better" number was that brightening in the metric's denominator.

The honest ceiling: the remaining grain IS the entry jitter, and the jitter is what keeps the march
from banding. The same camera without jitter measures 0.0162 and shows visible contour rings in the far
field. Everything below Gaps' rejected list is an attempt to have both.

### Frame proofs

**Reproducible, and that is the point of how they are taken.** The lighting and the weather were always
deterministic (pinned `--utc`, the committed `assets/wx-2026-07-27T00Z.wxb`); the TERRAIN was not,
because the screenshot venue streams tiles while it renders and a short run frames a half-built
quadtree. The recipe that fixes it: hold the camera still for **180 frames** (`--seconds 3.0`) and write
only the last one (`--interval 3.0`). By then `fbworld` reports `pending=0` — the converged tile set is
a pure function of the camera, so a second, independent run writes the same bytes.

**Measured: all 12 frames byte-identical (sha256) between two independent runs**, in both `--albedo osm`
and `--albedo photo`. The capture script is committed — `sim/tools/capture_cloud_proofs.sh`, analysis
rather than a build target — and logs the camera, the sun, the deck geometry, `pending=` and the sha256
per frame. Written to `sim/build/r6-cloud-proofs/` (gitignored, `build/` is).

The fly-through is a LADDER through one column rather than a flown mission, and deliberately so: a
mission-driven `--interval` run moves the camera, so its terrain never converges and its frames cannot
be reproduced. The property under test — "one code path for outside / inside / transition" — is a
function of the camera's altitude relative to the band, which is exactly what the ladder varies.

| # | File | Camera / time / weather | What it shows |
|---|---|---|---|
| 1 | `p1-undercast-from-above.png` | 46.84 N/6.92 E, 8 450 m, pitch −35°, yaw 090; utc 1785146400 (sun el 55.7° az 135.5°); low 75.7 %, deck 2 991…3 891 m, vis 24.1 km | the deck from above, terrain and the Lac de Neuchâtel through the gaps, the far field dissolving into the reported visibility |
| 2 | `p2a-above.png` 5 800 m · `p2b-at-top.png` 3 950 m · `p2c-inside-hi.png` 3 500 m · `p2d-inside-lo.png` 3 060 m · `p2e-at-base.png` 2 900 m · `p2f-below.png` 1 900 m | all 46.76 N/6.90 E, pitch −10°, yaw 050; utc 1785146400; low 65.0 %, deck **2 982…3 882 m** | the ladder: above → the top surface at eye level → white-out inside → the base → below, terrain back and clear air. No frame switches code path; the shell quadratic does it all |
| 3 | `p3-underside-into-sun.png` | 46.84 N/6.92 E, 2 000 m, pitch +25°, yaw 277 (into the sun); utc 1785171600 (sun el 20.3° az 276.6°) | the honest result for a THICK deck: the base is Beer-dead. Slant optical depth to the sun is σ·thickness/sunUp ≈ 0.022·900/0.35 ≈ **57**, so all three multi-scatter octaves (att 1, ½, ¼) are zero and the base is the ambient floor times the sky — dark blue-grey, no silver rim. Recorded under Gaps |
| 3b | `p3b-cirrus-underside-into-sun.png` | 49 N/14 E, 7 000 m, pitch +18°, yaw 270; utc 1785168000 (sun el 25.8° az 269.9°); high 63.9 % | the same test where the deck is thin enough for light to get through (cirrus τ ≈ 3 over 500 m): Beer damping across the streaks AND the HG forward lobe as a broad silver halo around the sun |
| 4 | `p4a-cirrus-across-wind.png` | 49 N/14 E, 7 000 m, pitch +20°, yaw 047 = across the 250 hPa wind axis; utc 1785168000 | the fibres themselves, running across the frame. The AXIS is measured numerically, not off this picture — `--cloudcheck CIRRUS_AXIS`, 5.6° residual, coherence 0.942 vs 0.155 isotropic |
| 4b | `p4b-cirrus-from-above.png` | same point, 10 600 m, pitch −25° | the counter-case, kept because it is a finding: a 64 % cirrus veil seen from ABOVE is a flat white sheet. The fibres are a property of the underside view, where the Beer path through the sheet varies along the streak |
| 5 | `p5-dusk-banding.png` | 46.84 N/6.92 E, 8 450 m, pitch −35°, yaw 090; utc 1785178800 (sun el **0.8°** az 297.6°) | the banding check at the worst light: the sun below `kMinSunUp`, so the deck is self-shadowed and the tone range is long. 85 levels, longest identical run 10 px, 5.3 % identical neighbours over a 650×180 px deck box |

## Gaps

### Open work

| # | Thing |
|---|---|
| 5.1 | **Residual march noise — reduced 13 %, not solved.** Flat-deck grain went 0.0329 → **0.0285** on the 3×3 high-pass and 0.0388 → **0.0328** on the 9×9, against converged references of 0.0126 / 0.0188. What is left IS the entry jitter, and the jitter is the only thing keeping the march from banding — the same camera without it measures 0.0162 (9×9) and shows contour rings. Inside the spec (no temporal, no bake) the remaining honest lever is the node budget: `--cloudq 2` measures **0.0206** (9×9) at **+9 ms/frame**, which is the `SetCloudQuality` knob and nothing new. |
| 5.2 | **The underside of an optically thick deck receives no light.** Measured at p3: slant optical depth to the sun is ≈ 57, so all three Wrenninge multi-scatter octaves (attenuation 1, ½, ¼) evaluate to zero and the base is `kAmbientFloor` (0.18) × the sky colour — a dark BLUE base where a real overcast is a bright grey. A real τ=20 cloud still transmits ~10–20 % diffusely; three octaves cannot represent that at this τ. Fixing it is an AMBIENT/multi-scatter model change (more octaves, or a diffusion term that does not go through `exp(−od·att)`), not a march change, and it would move every frame in the set — so it is named, not smuggled in. `p3b` shows that the same code does produce Beer + a silver rim as soon as the deck is thin (cirrus, τ ≈ 3). |
| 5.3 | **Undercast relief is gentle.** The sun taps span at most one deck thickness horizontally, which is physically right for a 900 m deck, so a closed deck from above reads flat and bright. Confirmed against the converged q = 8 render, which is just as flat — this is the model, not the march. Cumulus-topped decks would need a taller column model, which is the `kCloudTopMin` knob and nothing else today. |
| 5.4 | **The mid deck has no character of its own** — it is the low deck's constants with a bigger feature size. Altocumulus banding (its actual signature) is not modelled. |
| 5.5 | **A flown fly-through cannot be a reproducible proof.** The reproducibility recipe needs a STILL camera for ~180 frames; a `--mission --interval` run moves, so its terrain never converges and its frames differ run to run. The ladder (p2a…p2f) proves the same code path at the same cost, but "seamless while moving" is now an eyeball claim, not a hash. A settle-then-fly venue (converge at the spawn, then fly with the streamer's budget raised) would fix it and does not exist. |
| 5.6 | **Grazing views are the worst-sampled case and the band-limit cannot help them.** A ray nearly tangent to the shell gets a segment up to `kMaxSegM` = 60 km long over at most 12 nodes — 5 km steps, against a 16 km feature size. That is where the residual weave sits in `p2c`. A per-segment length cap would need more nodes, i.e. the quality knob again. |

### Rejected, with the measurement that rejected it

All at the p1 camera (46.84 N/6.92 E, 8 450 m, −35°, `--albedo osm`), grain = 9×9 high-pass std/mean
over the 900×200 px flat-deck box, banding = longest identical run over the same box. Baseline (the
committed R5 march) 0.0388 grain / 18 px run.

| Approach | Measured | Why rejected |
|---|---|---|
| **No entry jitter** (deterministic node placement) | grain **0.0162** (−58 %) | banding: longest identical run 18 → **72 px**, and the frame shows contour rings across the whole far field. The spec's "blue-noise jitter" is right |
| **Half-amplitude jitter** (span 0.6 / 0.35 of a step) | grain 0.0275 / **0.0218** | banding run 18 → **45 / 72 px**. The grain/banding trade is continuous and there is no free point on it |
| **Geometric (front-loaded) step ladder**, ratio 1.6 — the spec's "density-dependent step count inside the same budget" | grain **0.0455 (+39 %, worse)** | it is right for a steep crossing of a thick deck and wrong for the grazing far field, where the whole segment contributes and the ladder's deep steps become kilometres long. Would need real per-ray lookahead |
| **Stratified 4×4 ordered entry jitter** (every 4×4 block carries all 16 phases, per-block rotated) | grain 0.02848 → **0.02816 (−1 %)** | stratification improves the BLOCK mean; the visible artefact is per-pixel deviation, which a complete stratum set does not reduce (it guarantees the full spread). Correct theory, wrong quantity |
| **Erosion band-limit by fading the AMPLITUDE** instead of fading toward the mean | grain **0.0201** (best of all) | it brightens the deck by **3.5 %** because erosion subtracts density — the metric improvement was the brighter denominator, and the deck no longer matches the converged render. Replaced by `ErodeFlat` |
| **Node budget as 6–12 INTERVALS** (i.e. 8–14 density taps) instead of 6–12 nodes | grain 0.0224 (−42 %) at **+2.4 ms/frame (+25 %)** | the same grain is available from the existing `SetCloudQuality` knob at the same price; putting it in the default budget spends the spec's stated 6–12 samples twice |
| **Rejecting** a ceiling that falls outside an étage band (the pre-R5 rule) | 2.7 km base jump in one frame | replaced by the ownership weight below |

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
a fixed anchor and `h ∈ [0,1]` is the height fraction inside the deck. **Which anchor is now carried by
the sample** (`FBCloudSky::AnchorLatDeg/AnchorLonDeg`): `sensors/FBVisualSystem` marches this field along
a line of sight, and anchoring per observer would nail the field to each aircraft. `FBMissionRunner`
gives a whole cast the primary actor's spawn; the cloud STAGE keeps pinning its own ECEF anchor at the
first camera frame, so picture and sensor share the field's statistics and every deck's geometry and
differ in its PHASE — stated in [`../sensors.md`](../sensors.md) Gaps, not closed here, because closing
it moves every committed cloud PNG. Separable, exactly as specified:

```
  x, y   = (east - driftE) / featureM , (north - driftN) / featureM     advection
  a      = ( x·wE + y·wN) / stretch                                     along the wind, stretched
  b      =  -x·wN + y·wE                                                across the wind
  a,b   += (noise(a,b · warpFreq) - ½) · warp · {1, warpCross}          domain warp (mares' tails)
  f      = Σ_{i<4} 2^-i · noise( a + shear·i·b , b )  / Σ 2^-i          FBM, sheared and ROTATED per octave
  c      = smoothstep(edge - width, edge + width, f)                    the coverage remap
  top    = mix(topMin, 1, c)                                            a stronger column reaches higher
  dens   = c · smoothstep(0, profBase, h/top) · smoothstep(1, 1-profTop, h/top)
  e      = mix(erosionFbm(a·erodeFreq, b·erodeFreq, h·erodeVert), erodeMean, erodeFlat)   band-limit
  dens  -= e · erosion · mix(erodeBase, 1, h)
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
| `kCloudErodeMean` | 0.5 | **measured** 0.4988 over 40 000 samples, `--cloudcheck`; the value `ErodeFlat` fades toward, so the band-limit takes the erosion's variance and leaves its mean |
| `kErodeNyqLo` / `Hi` (stage) | **0.15 / 0.60 erosion cells per step** | [SET]; full detail below 0.15 cells crossed per step, flat by 0.60. The cell is `min(featureM/erodeFreq, thickness/erodeVert)` projected on the step direction, so a near-horizontal step through the deck keeps its erosion and a steep one loses it |
| `kCloudCeilBlendM` | 2 000 m | [SET]; the étage handover width. Measured cost: the worst deck slew along the fixture's steepest ceiling gradient rises from 135 m/km (the data's own gradient, which the old clamp also tracked while in band) to **325 m/km** during the handover |
| `kCloudCeilExistsFrac` | 0.10 | [SET]; a deck below a tenth of the sky cannot own the ceiling, as a ramp rather than a threshold |
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

NODE count `clamp(segLen / (thickness·0.35), 6, 12) · quality`: a near-vertical crossing gets 6, a
grazing one 12, and the step size adapts by itself. The quadrature is a **composite trapezoid** whose
nodes are the segment's two true ends plus `n−2` interior ones, so "6–12 steps" is spent as 6–12
density evaluations rather than 8–14. Entry offset = interleaved-gradient noise of the pixel, applied
to the INTERIOR nodes only — **spatial only**, no frame term anywhere in the shader, because without a
temporal resolve a per-frame jitter is flicker rather than antialiasing.

Why a trapezoid and not the jittered rectangle rule it replaced: the rectangle rule makes each pixel's
optical depth a function of where its samples happened to land inside a density that changes by O(1)
over one step, so it is both noisy AND biased (it applies the entry density over a whole step and then
early-terminates on `transm < 0.02`, which is why the committed R5 rendered a thick deck 3.7 % too
dark). The trapezoid's nodes share their ends, so the sum telescopes to `h·(f₀/2 + f₁ + … + fₙ/2)` and
depends on the jitter phase only at O(h²). Same taps, unbiased estimate.

### One ceiling, three étages

GFS reports cover per étage and exactly ONE ceiling. Which deck it belongs to is a **weight**, not a
choice, because every choice is a discontinuity somewhere:

```
  own[i] = window_i(ceiling) · exists(cover_i) · (1 − Σ_{j<i} own[j])
  base_i = mix(defaultBase_i, ceiling_i, own[i])
```

`window_i` is 1 inside étage i's band and falls to 0 over `kCloudCeilBlendM` = 2 000 m outside it;
`exists` is a ramp over `kCloudCeilExistsFrac` = 0.10 of sky, not a threshold; the outermost edges (the
low band's floor, the cirrus band's ceiling) SATURATE instead of blending, because there is no étage
below the lowest or above the highest to hand a ceiling to. The étages hand over: as a reported ceiling
walks up out of the low band, `own[low]` slides to 0 while `own[mid]` slides to 1, and both bases move
continuously.

It replaces two discontinuities at once, and the second one was undocumented: the predecessor CLAMPED
the ceiling into the band of the lowest BROKEN deck, so (a) the base stuck at the band edge while the
reported ceiling walked on, and (b) the choice of deck flipped the instant a cover crossed 0.5.

**Measured**, 46.70 N along 6.60→8.00 E in the committed fixture, where the reported ceiling climbs
3 023 m → 8 173 m at 135 m/km while low cover falls 48 % → 18 %:

| | old (clamp) | new (ownership) |
|---|---|---|
| low deck base at 7.10 E (ceiling 4 053 m) | 4 000 m | 4 047 m |
| … at 7.50 E (ceiling 8 173 m, low cover 18 %) | **4 000 m — stuck**, a 4.2 km assertion the data does not support | 1 200 m (its climatological base: this ceiling is demonstrably not the low deck's) |
| worst base slew along the track | 135 m/km (= the data's own gradient) | **325 m/km** during the ~15 km handover |
| Payerne proof corridor (ceiling 2 954…3 023 m, low cover 76 %) | 2 991 m | **2 991 m — unchanged**, `own = 1` throughout |

The price is named: during a handover the deck slides, and at this fixture's steepest gradient it
slides 2.4× faster than the ceiling itself moves. The alternatives are worse in kind rather than in
degree — the clamp asserts a base the data contradicts and holds it indefinitely, and rejection jumps
2.7 km in one frame (measured, R5).

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
