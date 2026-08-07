# Clouds — the render chain

**Origin:** moved out of `rendering.md` §5 (state `793e1fe`). Neighbouring files:
[`renderer.md`](renderer.md) (the pass topology the chain hooks into) and
[`../world/weather.md`](../world/weather.md) (the `/wx` source these decks are built from).

## Spec

**Rebuild, specified by the project owner** (roadmap R5). The six existing `Cloud*` stages are
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
| Shared evaluation | the density function must be evaluable in WGSL **and** C++ (shared constants) — "how far can I see" is a question the picture and any future perception ask alike |
| **ONE atmosphere** (added 2026-07-29, project owner) | the deck and the TERRAIN under it dissolve into the same air: one σ₀ = 3.912/visibility from the same weather sample, one inscatter colour, in ONE shared function both shaders splice. And the ground must know that a deck stands between it and the sun — direct light attenuated, diffuse raised, **no shadow map** |
| **TWO scale heights** (corrected 2026-07-29, project owner — the single 8 km constant was the owner's own directive and is withdrawn) | σ₀ is split into a molecular term at ~8 km and an aerosol term at ~1.2 km, summed. Both scale heights must be **published and cited**, and the rule that divides σ₀ between them must be argued, not tuned. The molecular term may carry its own wavelength dependence; the shared-function requirement stands (`--cloudcheck AIR_RESULT` = AGREE) |
| **The FIELD and its PICTURE are two things** (added 2026-08-06, project owner: *„bei den Wolken an Witcher 3 und Fallout 4 orientieren. Das muss billig gewesen sein und sah gut aus."*) | The field — `cloudSunThru`, the ground's shadow — stays exactly as built: one cheap query, no integration. The PICTURE is a **sheet on the dome by default**, which is what both reference titles drew and why they were cheap. The march is not deleted, it is not driven: `SetCloudQuality > 0` brings it back for the camera that needs it. **Binding condition: ONE field.** The sheet reads the same `CloudSkyU`, so cloud and shadow still belong to each other — which is more than the reference titles ever had |

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

**Built (roadmap R5; re-measured and extended in the R5 follow-up round).** The six `Cloud*` stages
are gone; the chain is ONE class, `render/stages/CloudLayerStage`, over ONE shared density function,
`core/CloudDensity.h`. Armed by default; `FB_CLOUDS=0` disarms it at boot.

Three things changed in the follow-up round, each with its measurement below: the march integrates by
**composite trapezoid** instead of a jittered rectangle rule, the erosion octaves are **band-limited
against the march's own step** (a new deck parameter `ErodeFlat` in the shared function), and the one
reported ceiling is spent on the étages by a **continuous ownership weight** instead of a clamp.

### 2026-08-06 — the march is built and not driven; the sheet is what is drawn

`CloudQuality` defaults to **0**, which draws the sheet. Measured on ONE binary, demo scene, 1280×720,
`--warm 60`, min of 3 × 200 frames, yaw 270:

| | frame | Begin*Pass | the cloud draw alone |
|---|---|---|---|
| march (`--cloudq 1`) | **11.222 ms** | 8 | 4.906 ms |
| **sheet (default)** | **6.946 ms** | **7** | **0.630 ms** |
| no cloud draw (`FB_CLOUDS=0`, field still written) | 6.316 ms | 7 | — |

**−4.28 ms of frame, −7.8× on the draw, one pass back.** What it costs in the image, over the five
standpoints (yaw 0/90/180/270/315), in display-linear luminance:

| | mean \|Δ\| |
|---|---|
| whole frame | **0.0158** |
| sky pixels only | 0.0348 |
| **ground pixels** | **0.00000000** — the maximum over any single ground pixel in five frames is zero |

Tonal spread (0.5/99.5 pct of linear luminance over the frame) moves **up**, 6.22–6.58 EV marched to
6.25–6.75 EV as a sheet, and the reason is visible in the PNGs: at this sun elevation the march
integrated the deck to a nearly **structureless orange wash** (95 % of the visible sky is deck at over
6 km slant, where a 55 % field with 16 km features is opaque), while the sheet draws a layer with
readable bands and blue between them. `localSunThru` at the standpoint is **0.698577 in both**, identical
to every printed digit across all five yaws — the field was not touched and the world-fixedness test
still passes.

**What the sheet is worse at, stated rather than hidden:** the clouds read as smooth cut-outs. One node
gives no brightness gradient ACROSS a cloud and no self-shadowing within the deck, so each patch is
nearly uniform in tone; what varies is only its shape and its density. From a camera at deck height
that will not hold, and that is what `SetCloudQuality > 0` is for.

**2026-07-29 — the air stopped being the cloud's private property.** The chain now spans two stages:
the deck's haze and the TERRAIN's haze are one function in one header (`render/stages/AtmoHaze.h`),
and the ground is lit through the deck instead of under it. The disabled `FB_AP` clear-air aerial
perspective is deleted. Its measurements are their own section below.

| Piece | Where | What it is |
|---|---|---|
| The density function | `core/CloudDensity.h` | C++ half: hashed gradient noise, the coverage FBM, the analytic column, the erosion, plus `CloudSkyFromWeather` (the three decks from one `WeatherProvider` sample). Includes nothing above `core/` — `verify-layers` holds it there. |
| The same function in WGSL | `render/stages/CloudDensityWGSL.h` | a literal transliteration whose **constants are printed from the C++ ones** (`CloudDensityConstsWGSL()`), so a number cannot drift between the picture and a measurement |
| **The air** | `render/stages/AtmoHaze.h` | the second shared function, same construction: Koschmieder σ₀, the molecular/aerosol **split rule**, the two scale heights, `kMinSunUp`, `hazeOpticalDepth3`/`hazeTransmittance3` (per channel) and their photopic scalar face, the inscatter colour, and the deck's sun optical depth + transmittance. C++ half and WGSL half in one file, constants emitted by `HazeConstsWGSL()`. Spliced by **both** `CloudLayerStage` and `TilesStage` — the terrain haze is not a copy of the cloud haze, it is the same three lines |
| The stage | `render/stages/CloudLayerStage.{h,cpp}` | ray ∩ shell per deck, ≤ 3 segments front-to-back, 6–12 **nodes** of a composite trapezoid, blue-noise entry jitter on the interior nodes, 2 sun taps, HG phase, sky-LUT ambient, Koschmieder haze, dither out. Blends **premultiplied straight into `HdrTex`**. |
| **The sheet** (default) | `render/stages/CloudLayerStage.cpp`, `kCloudSheetWGSL` | the same stage's second pipeline: ONE node of the same integrand, at the ray's crossing of the deck's mid shell, with the analytic chord as the interval. Same `cloudDensity`, same three Wrenninge octaves against the same `S.tau` the ground shadows itself with, same phase, same `kSunIntensity`/`kAmbientFloor`, same `AtmoHaze`. Erosion band-limited by DISTANCE with the march's own two constants. It rides in the SCENE pass over sun/moon/stars and under the terrain, so **it costs no pass** |
| The weather seam | `clients/AppNative.cpp`, `clients/AppWasm.cpp`, `clients/AppWalk.cpp` | the CLIENT samples `World::Weather()` where the camera is and hands the renderer an `CloudSky` (`Renderer::SetCloudSky`). The renderer never sees a provider; the same call is what a future IR sensor makes. The pedestrian bench's provider is `clients/SceneWeather.h`: the scene's one wind and its one `cloudCover`, the cover going to the LOW deck **[SET]** because that is the deck that shadows the ground. |
| **The field, as a buffer** | `Renderer::WriteCloudSky` → `SceneLight::CloudSky` | ONE uniform (`render/stages/CloudShadow.h`, 5 vec4 + 3 decks = 272 B) holding the anchor frame, the sun geometry and the three decks. The march binds it and so does every lit surface — terrain, buildings, blades — because a shadow computed from a second field would not lie under its cloud. |
| **The shadow** | `render/stages/CloudShadow.h` | `cloudSunThru` = `DeckSunTransmittance` with the deck's area-mean `cover` replaced by `cloudCoverage` **at the point where the sun ray pierces the deck**, which at a 1 200 m base and an 11° sun is 7.6 km downsun. `cloudMeanThru` is the same expression at the area mean, and that is what the deck's downward re-emission uses: a hole in the deck does not stop the rest of it glowing overhead. The mean of the first over the plane IS the second, because `CloudCalibrate` makes `cover` an area fraction. |
| The numeric gate | `--cloudcheck` — **the binary that carried it is deleted, so this gate does not run today** | evaluated both halves over 12 288 samples and prints the largest disagreement — **twice**, once for the density (`RESULT`) and once for the air (`AIR_RESULT`, the haze × deck-light product over 0.2–200 km visibility, ±14 km altitude, sight lines to 250 km and sun elevations crossing the `kMinSunUp` floor). Also measures the three constants that are claimed as MEASURED — the coverage FBM's distribution, the erosion FBM's mean (the value the band-limit fades toward), and the **cirrus streak axis** against the deck's own wind, with an isotropic control |
| The weather instruments | `--vis KM` / `--cover FRAC` — **also gone with `gpu_native`** | overrode the `--wx` sample's visibility / low-deck cover for ONE screenshot, deck geometry untouched. Screenshot venue only — a mission's weather is its own. Without them "the same camera under two atmospheres" is not measurable at all, which is how the missing terrain haze stayed invisible |
| The frame gate | **deleted with its mod** | it was four legs above / inside / below one real GFS deck. The SHAPE is what to rebuild: a camera that crosses a declared deck, one frame per leg |

**Pass topology.** The cloud pass is a separate render pass because it must SAMPLE the depth texture
that was an attachment a moment earlier; it writes back into `HdrTex` with premultiplied blending, so
`TonemapStage` has nothing to composite any more and collapsed from two pipelines to one. The pass
exists only when the weather actually has a deck — `render/passcount` logs `passes`, `clouds` and
`cloudPass` so a frame's topology is readable from the telemetry. On the pedestrian bench that is
**7 passes without a deck and 8 with one**.

**The dome's noise sheet yields to the deck.** `SkyStage` still owns a direction-space cloud sheet, and
it is a stand-in: it has no altitude, so nothing can stand under it and it can cast no shadow. The
moment `Clouds->Active()` is true, `Renderer` passes `cloud = 0` into `skyExtra.z` and the sheet is
gone — otherwise the picture would carry two cloud fields and the ground would be shadowed by neither
of the ones it shows.

**World-fixed, and measured that way.** The field's horizontal origin is `CloudSky::AnchorLat/LonDeg`,
a PLACE, resolved once in `Renderer::WriteCloudSky`. It used to be the first frame's EYE, which nailed
the pattern to wherever a session started (`renderer.md` §1.9, the same failure class). Proof, from
`render/cloud_shadow` — the same expression the fragment shader evaluates, on the same field, at the
camera's own ground point:

| Test | Result |
|---|---|
| four yaws, one standpoint (`--yaw 0/90/180/270`) | `eastM −2.7e−11 · localCover 0.301423 · localSunThru 0.698577` — **identical to every printed digit** |
| three standpoints (`--stepE 0/500/4000`) | `eastM 0 / 501.05 / 4008.42` → `localSunThru 0.698577 / 0.725793 / 0.907707` |
| does the IMAGE follow | at `--stepE 500` the field predicts **+0.0278 EV** on the near field from `E = E_sun·sunUp·thru + E_sky + deck diffuse`; the render measures **+0.028 EV** |

**No weather = no cloud.** The old chain invented a default deck when `State.Env.Cloud*` was zero;
this one does not. A mission without a `wx` line renders exactly as before the rebuild.

**What it costs.** Measured on this machine (M-series, native Dawn, 1280×720, 120 frames):

| Build | ms/frame |
|---|---|
| no deck declared | 5.30 |
| deck declared, per-fragment shadow, `FB_CLOUDS=0` (no march) | 5.98 |
| deck declared, march on | 11.06 |

The per-fragment shadow costs **0.68 ms** (+13 %) for one coverage FBM per lit fragment. The march
costs **5.08 ms**, and that is the open number: it nearly doubles the pedestrian frame to draw a deck
that, from 1.7 m under a 1 200 m base, reads as an almost featureless ceiling — 95 % of the visible sky
is deck at more than 6 km slant, where a 55 %-covered field with 16 km features integrates to opaque.
Correct, and a poor trade. `Renderer::SetCloudQuality` scales the step counts and no client sets it.

### Does the everyday path actually show a deck?

The browser's default is LIVE weather (`clients/AppWasm.cpp` fetches `/wx` once per session), and a
scenario that declares no weather takes it — so the answer depends entirely on what GFS reports where
you are. Measured against the committed fixture (`assets/wx-2026-07-27T00Z.wxb`) with
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
weather. And a scene that wants a GUARANTEED sky has to say so, by pinning the fixture
`wx-2026-07-27T00Z.wxb`. **No declaration surface for that exists** — the format that carried it was
deleted with the mission layer ([`../mods.md`](../mods.md) `## Gaps`).

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

### The shared air (2026-07-29 round: terrain haze + light under the deck)

The terrain had **no** reference to visibility, haze or extinction at all — 3 uses of the visibility in
the whole renderer, all three inside `CloudLayerStage.cpp`. What stood in its place was `FB_AP`, a
complete but **disabled** clear-air aerial perspective (a Rayleigh/Mie transmittance-LUT ratio) that
could not see the weather. It is deleted, switch and block; what replaced it shares
`AtmoHaze.h` with the cloud march.

**Measured before, then after, at the same cameras** (`gpu_native`, since deleted, 1280×720, `--albedo osm`, the
180-frame converged still of `tools/capture_cloud_proofs.sh`; `--vis`/`--cover` are the instruments):

| Question | Before | After |
|---|---|---|
| **Two frames, one camera, visibility 5 km vs 80 km** (46.84 N/6.92 E, 8 450 m, −35°) | **sha256 identical**, 0 of 921 600 pixels differ | 100 % of pixels differ; mean sRGB luminance of the near terrain band +57.6 %, of the far band +47.2 % |
| the fixture's own 24.1 km vs 80 km | — | far band +32.5 %, near band +6.0 % |
| **Two frames, one camera, cover 0 % vs 100 %, in a terrain-only crop with no cloud in it** (46.84/6.92, 1 500 m, −10°; deck 2 991…3 891 m, crop 750×180 px at y 520) | **0 pixels differ** | 100 % differ, mean luminance **−20.9 %**; at 50 % cover −4.8 % |
| the same on RELIEF, cloud pass disarmed so only the terrain lighting is in it (46.62/7.95, 2 950 m; deck 2 411…3 311 m) | — | terrain **below** the deck −30.8 %; summits **above** its top **+0.8 %** — the per-fragment `frac` at work, not a global dimmer |
| how much of the ground's radiance is direction-dependent DIRECT light (`render/terrain_light`) | 0.882 always | 0.882 at cover 0 · 0.762 at 0.5 · **3.6·10⁻⁶** at cover 1 |
| C++ ↔ WGSL agreement of the shared air | — | max \|Δ\| **1.19·10⁻⁷**, mean 7.6·10⁻⁹ over 12 288 samples; tolerance 10⁻⁴ → **AGREE** |

**Cost**, same camera and method as the table above but timed by the DIFFERENCE of a 5 s and a 35 s
run (1 800 frames apart, which cancels device bring-up and the terrain stream-in), 5 repetitions per
configuration, and the two full configurations additionally re-run alternating to rule out thermal
drift:

| Configuration | ms/frame (min…max over 5) | Δ |
|---|---|---|
| terrain only (`FB_CLOUDS=0`), before | 1.27…1.67, median **1.45** | — |
| terrain only, after | 1.62…1.97, median **1.85** | **+0.36 ms** |
| full frame with the 76 % deck, before | 15.64…15.81, median **15.72** (alternating re-run 15.88) | — |
| full frame with the deck, after | 16.27…16.55, median **16.36** (alternating re-run 16.54) | **+0.65 ms, +4.1 %** |

So the terrain haze itself costs **0.36 ms** and the remaining **≈0.29 ms** is the cloud pass paying
for the shared inscatter — it now carries the sun halo it did not have, which is the price of deck and
ground converging on one colour. Against the cloud march's own ~14 ms at this camera, +4 %.

Pass topology unchanged: `passes=7 clouds=1 cloudPass=1` with a deck, `passes=6 … cloudPass=0`
without. Telemetry unchanged: **11 telemetry CSVs over 6 mission runs byte-identical** to the binary
built from `HEAD` (`payerne-full`, `wx-gfs-fixture`, `wx-clouds-proof`, `bvr-duel`, `attack-ccip`, plus
`wx-clouds-proof --interval 30` with the GPU device live), `events.log` identical except `wallS`,
`speedup` and the `--out` path; `--threads 1/2/4` identical.

### The scale-height correction (2026-07-29, same day, project owner)

The round above was specified with ONE scale height and it was the wrong one; the correction is Gap 5.7
and 5.8, both now closed. The mechanism did not move — one shared header, both shaders splice it — only
the law inside it: **σ₀ is split into a molecular term at 8 km and an aerosol term at 1.2 km**, and the
molecular one carries λ⁻⁴ per channel. Derivations and citations are in the constants table below.

| Measurement | Single 8 km term | Two terms |
|---|---|---|
| terrain 13.9 km away, 4 450 m mean sight-line altitude, fixture visibility 24.1 km (the `p1` geometry) | T = **0.2743** | T = **0.8532** green — R **0.9076**, B **0.7302** |
| the deck 9.0 km away, 5 943 m mean altitude, same air | T = **0.4991** | T = **0.9347** green — R 0.9663, B 0.8597 |
| horizontal at the surface, 24.1 km (Koschmieder's own definition) | T = **0.0200** | T = **0.0200** — the split is exact at z = 0, by construction |
| `p1` gap crops: luminance detail (σ of Y) in the Lac de Neuchâtel gap / the NW terrain gap / the NE gap | 5.86 / 2.96 / **0.34** (a dead flat wash) | 14.32 / 20.08 / 9.03 — **×2.4 / ×6.8 / ×26.5** |
| `p1` gap colour, mean R/G/B | 163/175/195 (that is the haze, not the ground) | 178/186/191 |
| two frames, one camera, 5 km vs 80 km visibility — **1 200 m**, Jura→Alps, 40 km sight line | — | **99.99 %** of pixels differ, mid band −6.4 % |
| the same pair at 8 450 m (the `p1` camera) | — | 99.70 % differ, but only 0.6 % in luminance — Gap 5.11 |
| under a closed deck, terrain-only crop, cover 0 % vs 100 % (46.84/6.92, 1 500 m) | −21.0 % (−4.9 % at 50 %) | **−21.3 %** (−4.6 % at 50 %) — the deck light is untouched, as intended |
| the same on RELIEF, cloud pass disarmed (46.62/7.95, 2 950 m, deck 2 411…3 311 m) | valley −22.7 %, summits above the deck top **+1.7 %** | valley **−31.8 %**, summits **+3.8 %** — the per-fragment `frac` survives and reads stronger now that the haze no longer washes the crop |
| the channel split, isolated against a control binary identical except `hazeTransmittance3` forced to its own green channel | — | 99.5–100 % of pixels differ, max **35/255**; green channel identical to the last bit |
| C++ ↔ WGSL agreement of the shared air (`--cloudcheck AIR_RESULT`) | max \|Δ\| 1.19·10⁻⁷ | max \|Δ\| **1.19·10⁻⁷**, mean 1.17·10⁻⁸ → **AGREE** |
| `--cloudcheck RESULT` (density, untouched) | 1.90·10⁻⁵ | 1.90·10⁻⁵ → AGREE |

**Cost of the second term**, same 5 s vs 35 s difference method, but INTERLEAVED A/B (one pair per
binary per round, alternating) because the effect is close to the noise floor of the venue:

| Configuration | rounds | one term | two terms | Δ |
|---|---|---|---|---|
| terrain only (`FB_CLOUDS=0`) | 9 | median **2.289** ms | median **2.309** ms | **+0.050 ± 0.027 ms** (mean of the 9 paired differences ± sem) |
| full frame with the 76 % deck | 5 usable of 6 (one round thermally throttled and is discarded, both binaries) | median **16.96** ms | median **17.25** ms | mean of the paired differences **+0.13 ms**, median +0.33, one of five negative — **not separable from noise; bounded at \|Δ\| < 0.4 ms ≈ 2 %** |

That is what the arithmetic predicts: the change is 3 extra `exp` per terrain fragment (2 → 5) and
nothing else — ~4 M extra transcendentals per frame at 1280×720. The round's TOTAL against `HEAD`
therefore still stands at the ≈ +0.65 ms measured above.

Telemetry re-checked against the single-term binary: **10 CSVs over 5 `gpu_native --mission` runs (the client and the missions are both deleted; the number stands as a record, not as a re-runnable check)
byte-identical** (`payerne-full`, `wx-gfs-fixture`, `wx-clouds-proof`, `bvr-duel`, `attack-ccip`),
`events.log` identical except `wallS`, `speedup` and the `--out` path. `passcount passes=7 clouds=1
cloudPass=1` unchanged.

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
2. **`ErodeFlat`, the band-limit** (`core/CloudDensity.h` + `erodeFlatness` in the stage). Cells
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

**Re-taken on 2026-07-29** for the shared-air round: all 12 frames moved (the terrain now hazes and the
deck's inscatter colour changed with it), and the reproducibility claim was re-measured the same way —
**0 of 12 differing between two fresh independent runs.** What the set shows now that it did not:
`p2f-below` and `p1` carry real depth cueing instead of a full-brightness horizon, and `p3`'s
underside shows the sun's position through the air in FRONT of the deck (the haze inscatter's halo),
where before it was a featureless dark base.

**Re-taken again the same day** after the scale-height correction, and the reproducibility claim is no
longer a footnote: `VERIFY=1 tools/capture_cloud_proofs.sh` takes the set TWICE into sibling
directories and fails on any frame that moved. Result: **12/12 byte-identical across two independent
runs.** All 12 frames moved against the single-term set (62.8–100 % of pixels), and the luminance
detail rose in 11 of them — most at `p5-dusk-banding` (σ of Y **×2.09**: the dusk terrain is visible
through the deck's gaps instead of a flat purple wash) and `p4a-cirrus-across-wind` (×1.23). The one
frame that did not gain detail is `p3-underside-into-sun` (×0.98, mean luminance **−12 %**) — the milky
veil in front of the deck is largely gone at 2 000 m looking up, which is right for a 1.2 km aerosol
profile and is the single largest look change in the set.

The script now **ABORTS the whole set** on any `gpu_error` other than the deliberate teardown
`device_lost`, on a non-zero exit, on a missing frame, and on a residual `pending`. That is the direct
lesson of the round before it, in which a WGSL type error (`step(f32, vec3f)`) invalidated the terrain
pipeline and the venue produced a complete set of PLAUSIBLE frames — sky and HUD, no terrain — costing
a full round of measurements. Proved by pointing the script at a deliberately broken binary: it dies on
shot 1 with exit 1 and writes no frame. Note in the script's own comment: the checks read their
evidence through command substitution rather than `grep | grep -q`, because under `pipefail` that
pipeline returns the first `grep`'s SIGPIPE and an `if` then reads a real error as "no error".

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
| 5.7 | ~~**The haze's 8 km scale height is the ISA DENSITY scale height, and aerosol does not thin that slowly.**~~ **CLOSED 2026-07-29** (project owner: the constant came from the brief, not from the implementation). Measurement that stands: a single `kHazeScaleHM = 8000 m` thinned a Koschmieder SURFACE extinction — 92 % aerosol at the fixture's 24.1 km — at the molecular rate, and at the `p1` camera the terrain 13.9 km away below the aircraft transmitted **0.275** at H = 8 000 m against **0.946** at H = 1 200 m, the deck 9 km away **0.499** against **0.990**. `p1` was a white-out. Replaced by the two summed terms below; the same two geometries now transmit **0.853** and **0.935** (green), and `p1` shows the Lac de Neuchâtel through the gaps again (2.4–26.5× the luminance detail in the three gap crops). |
| 5.8 | ~~**The haze is GREY, and the aerial perspective it replaced was not.**~~ **CLOSED 2026-07-29 — the split of 5.7 paid for it.** Once the molecular term is carried separately it can carry its own λ⁻⁴, so the extinction is per-channel again and the reddening comes from the physics rather than a table: at the `p1` geometry T = (**0.908**, 0.853, **0.730**) for R/G/B, at 60 km and z̄ = 1 500 m (0.058, 0.040, 0.015). Isolated against a control binary whose only difference is `hazeTransmittance3` forced to its own green channel: **99.5–100 % of pixels differ, max 35/255** (dusk). One correction to this gap's own premise, and it matters: the extinction reddens the transmitted beam, but the *picture* still goes BLUER with distance, because the channel that loses the most transmittance gains the most of a sky-blue inscatter that outshines the terrain behind it — measured d(R−B) = −5 … −14 against the grey control at every band of a 40 km Jura→Alps sight line. That is why real distant mountains are blue, and the deleted `FB_AP` had the same sign for the same reason. |
| 5.9 | **The under-deck light is a per-frame statistical scalar, so a broken deck dims the whole ground evenly** instead of drawing cloud shadows. That is deliberate (the round's brief: attenuation plus diffuse, explicitly no shadow map) and it is right on the average — `cover` is a calibrated area fraction — but at 30–70 % cover the eye expects moving patches and gets a uniform 4.8 % dimming (measured at 50 % cover). A shadow would need the same march the cloud pass already pays for, sampled toward the sun; it stays under "no cloud shadows on the terrain" in the omissions below. |
| 5.10 | **The aerosol profile is a pure exponential, and a real mixed layer is not.** `exp(−z/1200)` has already lost a third of the aerosol at 500 m, whereas the boundary layer is roughly well mixed to the inversion and then drops hard. Measured consequence at the fixture's 24.1 km: a 20 km sight line whose mean height is 500 m transmits **0.109** where a strict surface path transmits 0.047 — a low-level view hazes visibly less than its reported visibility alone would suggest, while the exactly-horizontal surface path is still 0.020 at 24.1 km by construction. Fixing it means a mixed-layer height, and no published universal value exists for it; a weather sample that reported its own inversion height could carry one. |
| 5.12 | **The sheet has no gradient across a cloud.** One node per deck means the only thing that varies over a cloud's face is its own density; there is no self-shadowing inside the deck and no brightness ramp from lit edge to shaded core, so the patches read as smooth cut-outs. It is what the default draws today and it is a named loss, not an oversight — measured against the march it is worth 0.0348 mean \|Δ\| on sky pixels and it buys 4.28 ms. A second node (entry + exit of the chord) would give the ramp for roughly double the sheet's 0.63 ms; nobody has measured whether that reads better. |
| 5.13 | **A camera at or above deck height is outside what the sheet can express** — there the deck's parallax and interior structure are the picture, which is exactly what one node throws away. `SetCloudQuality > 0` is the answer and no scene drives it yet, so the switch is untested in a client. |
| 5.11 | **Aloft the air is now almost empty.** At 8 km the aerosol term is `exp(−6.7)` = 0.12 % of its surface value, so cruise altitude sees essentially the molecular atmosphere alone and the visibility the weather reports barely moves the picture: the same camera at 8 450 m over the Payerne deck changes the near band's luminance by only **0.6 %** between 5 km and 80 km reported visibility (at 1 200 m the same pair moves **100 % of pixels** and the mid band by 6.4 %). Physically that is what the profile says; whether a cockpit at FL280 should still see a haze deck below it (it does, and it is the top of the layer, not the layer) is a separate feature — a layer TOP, not a scale height. |

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
| No cloud shadows on the terrain | a second consumer of the same function (the terrain shader would sample it toward the sun); not in this round. **The 2026-07-29 round did the half of it that needs no march**: the ground's DIRECT light is attenuated and its diffuse raised, per deck and per fragment, from `cover` and the deck's mean-field optical depth. What is still missing is the spatial pattern — Gaps 5.9 |

### Inventory (from the previous `Open points` section)

(see [`renderer.md`](renderer.md) — the collected list of the renderer round is preserved there in
full; the points that belong here are above under Gaps.)


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### The density function, in full

Per deck, `CloudDensity(deck, eastM, northM, h)` where east/north are metres in the tangent plane of
a fixed anchor and `h ∈ [0,1]` is the height fraction inside the deck. **Which anchor is now carried by
the sample** (`CloudSky::AnchorLatDeg/AnchorLonDeg`), and **not per observer** — anchoring per observer
would nail the field to whoever looks at it, which is the camera-fixed defect
([`renderer.md`](renderer.md) §1.9) one layer up. The cloud STAGE pins its own ECEF anchor at the first
camera frame, so any second consumer of the field would share its statistics and every deck's geometry
and differ in its PHASE — a gap whose consumer was deleted, not closed here, because closing
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

**The remap is calibrated, not guessed.** `CloudCalibrate` places the threshold against the FBM's own
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
| haze σ₀ | 3.912 / visibility | **derived**: 3.912 = ln(1/0.02) is the contrast threshold that DEFINES meteorological visual range. The visibility comes from the weather sample, so the haze is weather-driven and not tabulated. `AtmoHaze.h`, shared with the terrain |
| `kHazeScaleRM` (molecular) | 8 000 m | **published**: the ISA density scale height is R·T/(M·g) = 8 434 m at 288 K, and its standard exponential fit over the mass-bearing 0–30 km is 8 km (Bucholtz, *Appl. Opt.* **34**(15), 1995). Air scatters in proportion to its own density, so nothing here is a choice. It is also literally the number this renderer's own sky is built on — `kAtmoCommon`'s `exp(−h/8.0)`, from Bruneton & Neyret, *Precomputed Atmospheric Scattering*, EGSR 2008 |
| `kHazeScaleAM` (aerosol) | 1 200 m | **published**: boundary-layer aerosol is confined to the mixed layer and thins ≈ 6.7× faster than the air carrying it — Elterman's measured attenuation profiles (AFCRL, 1968), the fit Bruneton & Neyret 2008 adopt as H_M = 1.2 km and `kAtmoCommon` already uses as `exp(−h/1.2)`. Simplification named as Gap 5.10 |
| the SPLIT of σ₀ between the two | σ_R = min(σ₀, **1.3558·10⁻⁵ /m**), σ_A = σ₀ − σ_R | **derived, no free parameter.** Clean air's molecular coefficient at 550 nm is a constant of nature, and it is the *same* number the sky LUT uses (`rayleighScatteringBase.g`, Bruneton & Neyret 2008 for 550 nm; ⇒ a Rayleigh-limited visual range of 3.912/1.3558·10⁻⁵ = **288 km**). So the molecular part is fixed and the AEROSOL carries whatever the weather adds on top: 1.7 % molecular at 5 km visibility, **8.4 % at the fixture's 24.1 km**, 27.7 % at 80 km, 100 % beyond 288 km where the aerosol term is simply zero. The two sum to σ₀ **exactly** at z = 0 and 550 nm, so the split never changes the reported visual range — measured: T = 0.0200 at 24.1 km horizontal, before and after. The WGSL half reads `rayleighScatteringBase` directly and a **`const_assert`** ties the C++ mirror to it (proved: perturbing the mirror by 0.4 % fails the shader compile) |
| the CHANNELS | σ_R × (5.802, 13.558, 33.1)/13.558 | **derived**: those are `kAtmoCommon`'s per-channel coefficients for (680, 550, 440) nm, and their ratios are λ⁻⁴ exactly — 13.558/5.802 = (680/550)⁴, 33.1/13.558 = (550/440)⁴. Aerosol extinction stays grey (Mie on a broad size distribution is near-neutral across the visible). The scalar face of the law is its **green channel by construction**, which is the wavelength Koschmieder's definition is about and the one `--cloudcheck AIR_RESULT` measures |
| `kMinSunUp` | 0.20 | [SET]; floor on the sun's elevation cosine in EVERY Beer path — the march's sun taps and the terrain's deck attenuation, which is why it moved into the shared header |
| terrain ambient / direct weights | 0.4 / 3.0, plus **0.15** under a closed deck | the first two are the terrain's own, unchanged. The 0.15 is **derived**: measured overcast diffuse illuminance is ~1.0–1.5× the clear-sky diffuse, so the 0.4 diffuse term gains ~35 % ⇒ +0.14, and the resulting ground under full overcast is 0.55/2.52 = **22 %** of the sunlit clear case — inside the 10–25 % that overcast/clear global illuminance measures |

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

### ONE deck, decided 2026-08-06 — the three étages are removed

> Owner: *„unseren Wolken-Shader auch vereinfachen. Eine gute Schicht anstelle von drei."* ·
> *„Witcher 3 und Fallout 4 sehen teilweise so gut aus, nur die Details sind weniger."*

**One well-lit deck beats three mediocre ones**, and the second quote is the calibration: the reference
photograph's impact comes from *light*, not from layer count. W3 and F4 reach that impact with less
detail — so detail is not where the budget belongs.

**What the single deck keeps:** the GFS ceiling drives its base directly, with no ownership arithmetic ·
the full lighting chain (closed-form optical depth toward the sun, Wrenninge multi-scatter octaves,
dual-lobe HG, powder term, sky-view ambient) — that chain is what carries the image and is **not**
simplified · the shared-air haze and the light under the deck.

**What goes:** `window_i` / `exists` / `own[i]`, the mid and cirrus bands, and the handover. One base,
one thickness, one march. The march cost falls with the layer count; the lighting cost per lit sample
does not change.

**The price, stated:** no cirrus above cumulus. The reference photograph has both, and a single deck
cannot reproduce that stratification. Accepted — a second deck is re-addable as a pure repetition of
the same declaration if a frame ever proves it is missed, and the measurement below is kept so that
decision does not have to be re-derived.

**The ownership measurement is kept in the section below** because it is knowledge, not history: it
records what the three-étage handover cost and what the two rejected alternatives cost. Nothing about it
was wrong; it is removed because one layer is enough, not because it failed.

### Superseded: one ceiling, three étages (measurement retained)

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

Six classes — `CloudMipDownStage`, `CloudBaseBakeStage` (128³ Perlin-Worley),
`CloudDetailBakeStage` (32³ Worley), `CloudCellBakeStage` (512² F1 cells), `CloudMarchStage`
(quarter-res march), `CloudResolveStage` (temporal reprojection, ping-pong history + weight sum) —
plus `CloudNoiseCommon.h`, `TonemapStage`'s second pipeline, and the `--cloudlab` / `--cell`
parameter-sweep harness in `clients/AppNative.cpp`. Its studies stay in `doc/render/clouds-legacy/01`–`10` as the
record of what was learned; the code is gone. Its cost is in the table above.
