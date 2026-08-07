# Tonemap, Exposure and Upscale — the end of the chain

**Passes:** `ExposureStage` · `TonemapStage` · `UpscaleStage` (`sim/src/render/stages/`).
**One document because they are one exposure scale**: the illumination places the curve, the tonemap
applies it, and the upscale is what the result is finally seen through. Splitting them would put a gain,
the curve it feeds and the filter that resamples the outcome in three places.

The anchor's input is `IrradianceStage`, not this chain's own output — see
[`atmosphere.md`](atmosphere.md) for the LUT that produces it and `## Knowledge` for why the picture is
the wrong thing to measure.

Neighbours: [`atmosphere.md`](atmosphere.md) (the scene-referred units everything arrives in),
[`ao.md`](ao.md) (composited here, not in a pass of its own), [`../renderer.md`](../renderer.md) (the
pass topology and the present path), [`../visual-target.md`](../visual-target.md) §2 (the cinematic look
this chain is where it is applied).

## Spec

| Contract | Why |
|---|---|
| **ONE scene-referred scale.** Every shader that produces radiance works in the LUTs' units (top-of-atmosphere solar irradiance = 1) and applies exactly one constant on the way out | two independently fitted scales is what put zenith sky and sunlit ground **2.5–3.6 EV** apart |
| **the display curve is placed, not fitted.** Its black and white anchors are fixed ratios around the irradiance-derived adaptation luminance; the tonemap shader holds no constant of its own except where highlight desaturation starts | a curve fixed in absolute units can only be right for one lighting condition, and the demo scene's own irradiance moves with sun elevation. The ratios are sourced, see `## Knowledge` |
| the **AO composite belongs in the tonemap shader**, not in a pass | that shader already reads every scene pixel, and it is the only place with both the radiance and the direct fraction the surfaces wrote into the HDR alpha |
| **the exposure anchor comes from the ILLUMINATION, never from the picture.** `IrradianceStage`'s horizontal irradiance decides where the curve sits; where the camera points does not | owner, 2026-08-06: *„diesen komischen HDR-Effekt weg der alles dunkler macht wenn ich gegen was Helles schaue. Das ist unrealistisch."* An image meter darkens the world when the sun enters frame, which is a camera's iris, not a viewer. See `## Knowledge` — the eye's own iris is worth **3.32 EV** of a **46.5 EV** operating range [24] |
| **no exposure is ever declared by hand** | owner: *„exposure möchte ich nicht angeben müssen, später haben wir dynamische Tageszeiten und Wetter."* Sun elevation and cloud already move the irradiance; a declared stop would have to be maintained against both |
| **no time-dependent adaptation is modelled** | owner, 2026-08-06: *„Der Mensch regelt die Blende nach Helligkeit selber."* The viewer in front of the screen brings his own adaptation state, and [24] §4.3 assumes exactly that of a display observer. A photograph does not adapt either and is still legible |
| the picture's **shown spread** is an acceptance anchor, not an input | today **7.45 EV** measured (`## State`). It is checked against the frame; it never drives the frame |
| the anchor computation is **compute-only and rides an existing pass**, or is CPU-side off `ReadIrradiance()` | the per-frame render-pass count may not change for it |
| the anchors are **stops relative to the irradiance-derived adaptation luminance**, never absolute lux | the HDR target has no photometric calibration anywhere, so an EV100 field would be a number with nothing under it |
| the curve runs on **luminance**, chroma rides along and is let go of toward white | per channel, the blue channel of **99.96 %** of the demo frame's foreground came out at exactly 0: a 4.6° sun is 6:3:1, so blue falls under the black anchor while luminance does not, and the field went rust |
| **the black anchor is a KNEE and not a clip: below it the curve has a toe and reaches code 0 only asymptotically** | [24]'s reference black is where a *display* stops emitting (its CRT's 4 cd/m², §4.3), not where a scene stops existing. Used as a clip it cost, measured on the bench under `skylight`, **49.06 %** of the `portrait` frame at pure code 0 with only **1.58 %** left in codes 1–16 — a wall, not a shadow. The scene population that has to survive it is measured, `## State` |
| the frame is rendered at **fixed 720p** and upscaled to the display | [`../visual-target.md`](../visual-target.md) §2: a quarter of 1440p's pixels, and the film look is what hides the resolution |
| **anti-aliasing is the priority investment** and it lands here | at 720p, edges are the dominant defect; alpha-cutout foliage in motion is the worst case in the scene |

## State

**Tonemap and upscale are built** and landed with the stage split (`c9206eb`…`2099cb0`, see
[`../renderer.md`](../renderer.md) `## State`): one pipeline from the HDR scene target into `FrameTex`
with the AO composite folded in; then a bilinear resample of the fixed-720p `FrameTex` onto the target at
display resolution, used by both present paths.

**`ExposureStage` is built on the Spec above.** It is not a meter any more and cannot become one by
accident: it binds the irradiance buffer and nothing else, there is no histogram, no HDR texture
binding and no state. One compute dispatch on eight floats, riding the sky-view pass immediately after
the dispatch that writes its input, so the exposure is placed from THIS frame's light before a pixel of
it is drawn. `passcount` stays **7**.

`IrradianceStage` gained one output for it: `sky.w` = the luminance of E on a horizontal surface,
`dot(T_sun,lw)·sin(sunEl) + dot(E_sky,lw)`, published where the anchor is computed rather than
recomputed downstream.

| Quantity | Value | Origin |
|---|---|---|
| adaptation luminance A | `kGroundBounce · E_horiz / π · kSceneExposure` | [24] §4.1.2 takes A for stills from a known reflectance under the scene light; `kGroundBounce` is that reflectance, measured |
| black anchor | **A − 2.678 EV** (= A·5/32) | [24] §4.2 verbatim. **Checked against the meter it replaces: that meter put black at −6.723 for this scene and this rule puts it at −6.535 — 0.19 EV** |
| span | **11.686 EV** | derived from the OUTPUT CONTAINER: 8-bit sRGB code 255 decodes to 1.0 and code 1 to (1/255)/12.92 = 3.0356e-4, a ratio of 3294. The same substitution [24] makes onto its own display, with our display instead of its CRT |
| where A lands | **0.2 display-linear** | [24] §4.3: adaptation luminance 25 cd/m² against reference white 125 |
| exponent | `log(0.2)/log(2.678/11.686)` = **1.0924** | derived; no constant of its own |
| toe knee | `kToe` = **0.0551** of the span, i.e. 0.644 EV above the black anchor | measured, `### The toe` |
| night floor on E | `kNightAmbient` | the residual illumination `SurfaceLight.h` already adds to every surface, so the anchor rests on the same number the ground does instead of on log2(0). **Unexercised — no night frame** |
| cost | one dispatch, 8 floats | the three histogram dispatches and the full-frame texture read are gone |

**The two `[SET]` adaptation time constants are deleted**, together with `ExposureParams::AdaptBrightS`
/ `AdaptDarkS` and the scene fields that fed them. Nothing had ever exercised them.

### The toe, and why it is a toe and not another anchor

```
tlin = (log2 L − black)/span                       // unclamped
t    = tlin                     for tlin ≥ kToe    // the log-linear ramp, untouched
     = kToe·exp(tlin/kToe − 1)  for tlin < kToe    // same value, same slope at the knee
out  = t^contrast
```

`kToe = 0` restores the previous curve **bit for bit** (verified: four bench views identical after the
change with the constant at 0).

**The population the toe has to carry is measured, not assumed.** `FB_TONE_PROBE=black,white` turns the
curve into a ruler — exponent 1, no toe, so the frame's own display luminance *is*
`(log2 L − black)/(white − black)` and a PNG read back through the sRGB decode is the scene's HDR
histogram. Blade pixels are separated from floor and card by a paired render with `FB_COVER=0`. Demo
substrate, `wiese`, `skylight`, delivered anchor at **−6.56535** (`KeyEv −3.88728 − 2.678072`):

| view | blade px | p1 | p10 | p50 | p99 | under the anchor |
|---|---|---|---|---|---|---|
| `portrait` | 79.8 % of frame | −10.030 | **−9.237** | −7.158 | −4.687 | **61.2 %** of blades = 48.84 % of frame |
| `tuft` | 90.3 % | −8.287 | −6.275 | −4.907 | −4.632 | 8.0 % = 7.23 % |
| `sward` | 88.6 % | −8.884 | −7.330 | −5.092 | −4.244 | 17.5 % = 15.48 % |
| `eye` | 42.4 % | −7.474 | −5.497 | −4.940 | −4.215 | 2.9 % = 1.22 % |

The probe is checked against the picture it explains: 48.84 % predicted against **49.06 %** measured
code-0 in `portrait-skylight`, the 0.22 % difference being non-blade pixels under the anchor.

`kToe` follows from one decision and one measurement: **the blade population's p10 shall reach the
container's bottom code.** Code ≥ 1 needs `out ≥ 0.5/255` in sRGB = 1.51764e-4 display-linear, i.e.
`t ≥ 1.51764e-4^(1/1.09224)` = 3.1934e-4; `portrait`'s p10 sits at `tlin = −0.22862`. Solving
`ln(kToe) − 0.22862/kToe = ln(3.1934e-4) + 1` gives **kToe = 0.0551** (±0.00005 for a p10 known to
±0.05 EV). The knee then sits 0.644 EV above the black anchor, at code 58.

**What it costs the rest of the picture: nothing, and that is checkable rather than arguable.** The toe
is the identity above the knee, so every pixel brighter than `black + 0.644 EV` is bit-identical. On the
demo frame, **rows 0–375 do not change by a single code** — the whole sky, the horizon and the far
terrain — and 2.40 % of the frame changes at all, all of it inside the near blade field, max 35 codes.
The depth buffer is byte-identical, so [`../terrain.md`](terrain.md)'s horizon acceptance (topmost
depth hit per column against the DEM maximum along the same bearing) cannot have moved.

| demo scene 17:40Z, band | mean/255 before → after | ≥250 % | code-0 % | shown spread EV |
|---|---|---|---|---|
| sky (rows 0–320) | 195.61 → **195.61** | 0.024 → 0.024 | 0 → 0 | 0.60 → 0.60 |
| far terrain (330–372) | 164.86 → **164.86** | 0 → 0 | 0 → 0 | 2.37 → 2.37 |
| near grass (380–720) | 118.86 → 119.48 | 0 → 0 | **3.207 → 0.003** | 5.35 → **8.33** |
| whole frame | 156.91 → 157.21 | 0.011 → 0.011 | 1.517 → 0.002 | 5.11 → **8.02** |

**Rejected, with its measurement: moving the anchor instead.** `kBlackBelowA` 2.678 → 5.350 is what
`portrait`'s p10 demands of an anchor (2.672 EV, not the 1.88 EV a previous round's histogram implied —
that histogram is not reproducible with the ruler above). It works on the bench (`portrait` code-0
10.45 %) and it wrecks the counter-check, because black and white are one span: white falls the same
2.672 EV, to +2.479, and the exponent is dragged from 1.092 to 2.060 by its own derivation.

| | shipped | anchor at 5.350 | toe at 0.0551 |
|---|---|---|---|
| `portrait-skylight` code 0 | 49.06 % | 10.45 % | **5.35 %** |
| `portrait-skylight` codes 1–16 | 1.58 % | 12.88 % | **35.35 %** |
| demo sky mean | 195.61 | **210.14** | 195.61 |
| demo sky ≥ 250 | 0.024 % | **0.995 %** | 0.024 % |
| demo far terrain mean | 164.86 | **172.14** | 164.86 |

The anchor move lifts the whole frame by 3–15 codes and multiplies the sky's clipping by 41. It also
trades a **sourced** number for a scene-derived one: [24] §4.2's 5/32 is a statement about a display's
black level and it stays. The toe leaves it exactly where it is and changes only what happens *below*
it, which is the part [24] never specified.

### The pointing test, which is the whole point

Four frames, one standpoint, `yawDeg` 0/90/180/270 (the sun is in frame at 270), demo scene 17:40Z:

| | image meter (before) | irradiance anchor (after) |
|---|---|---|
| blackLog2 | −6.718 / −6.777 / −6.752 / −6.723 | **−6.53538 ×4** |
| whiteLog2 | 3.422 / 3.422 / 3.422 / **3.703** | **+5.15062 ×4** |
| contrast | 1.460 / 1.437 / **1.563** / 1.539 | **1.09241 ×4** |
| ≥ 250 | 1.66 % / **5.50 %** / **4.01 %** / 0.84 % | 0.00 / 0.00 / 0.00 / 0.01 % |

The meter's white point moved **0.281 EV** and its exponent **0.126** on a turn of the head, and it
broke the ≤ 2 % band in two of the four views. The new anchor is identical to every printed digit.

### The illumination test

Same standpoint, two times (scene restored byte-identically, SHA `bc43d1b5…` before and after):

| utc | sunEl | horizE | adaptLog2 | black | white |
|---|---|---|---|---|---|
| 17:40Z | 11.202° | 0.164214 | −3.857 | −6.535 | +5.151 |
| 11:00Z | 54.079° | 0.778294 | −1.613 | −4.291 | +7.395 |

The anchor moved **+2.245 EV**, and `log2(0.778294/0.164214) = 2.2447` — the curve follows the
irradiance exactly, because it is the irradiance.

### The three variants, all measured on the same frame

| variant | span | exponent | lower half | ≥250 | ≤25 | shown spread |
|---|---|---|---|---|---|---|
| [24] verbatim, old key target 0.035 on A | 5.000 | 5.370 | **17.6** | **50.80 %** | **44.08 %** | 26.58 |
| [24] verbatim, A → 0.2 | 5.000 | 2.578 | **38.6** | **51.42 %** | **29.39 %** | 10.74 |
| **shipped**: sRGB container span, A → 0.2 | 11.686 | 1.092 | 69.4 | 0.01 % | 0.00 % | **5.77** |
| bands | | | ≥ 45 | ≤ 2 % | < 5 % | ≥ 6 |

**That settles which number was in the wrong system.** `kKeyTarget = 0.035` was the display-linear
target for the METERED KEY — the log-average of the darkest 5–45 % of the picture, which this frame
measured **1.69 EV below A**. Applied to A it underexposes the frame to 17.6/255 against a band of 45.
The [24]-sourced 0.2 survives; 0.035 does not, and it is gone.

**And it settles that [24]'s 32:1 is a DISPLAY number, not a scene one.** Both verbatim variants blow
half the frame, whatever exponent they are given.

### What the frame measures now, demo scene 17:40Z, yaw 270

| | measured | band |
|---|---|---|
| mean luma, lower half | **69.4**/255 | ≥ 45 |
| spread 0.5–99.5 pct | **5.77 EV** | ≥ 6 — **MISSED** |
| ≥ 250, any channel | **0.01 %** | ≤ 2 % |
| ≤ 25, all channels | **0.00 %** | < 5 % |
| passes / draws | **7 / 130** | unchanged |

Foreground colour, bottom quarter mean RGB: **66 / 54 / 35** against the owner's reference
photograph's **44 / 36 / 21** — the frame is now about 1.2 EV brighter than the photograph there, which
is the same fact the shown spread reports and is in `## Gaps`.

**The browser shows the same numbers**, which it did not before this round: 69.60 / 5.77 / 0.01 % /
0.00 % against native's 69.41 / 5.77 / 0.01 % / 0.00 %. See `../renderer.md` — Chrome's canvas offers
no sRGB surface format and the tonemap writes display-linear, so the browser was presenting the linear
buffer and reading **19.8**/255 where native read 69.4.

## Gaps

- **The span is the only number in the chain that is not sourced end to end.** It no longer costs the
  spread band — the toe carries the frame to **8.02 EV** against ≥ 6 — but 11.686 EV is still the sRGB
  container's ratio standing in for a display span nobody sourced. What is owed is either that source or
  a re-derivation of the ≥ 6 EV band, which was itself solved against a curve that no longer exists.
- **`kToe` is derived from ONE population.** It is the `wiese` sward under `skylight` at an 11° sun.
  A different template, a different sun elevation or a night scene will move p10 and nothing re-derives
  the knee. The knee is a constant because it is dimensionless in `t` and the anchors already carry the
  light — but that argument covers the *light*, not the *scene content*, and no second scene has
  exercised it.
- **`ExposureParams` has no consumer and the manual mode is now unmeasured.** `keyEv` was defined
  against the metered key; it is redefined as "the scene radiance that shall sit at the adaptation
  luminance" and **nothing has exercised either mode since**. `mods/demo/scene.json` still declares no
  exposure block and must not be given one.
- **The night case is untouched.** `E_horiz → 0` is floored at `kNightAmbient`, which is a `[SET]`
  display crutch with no sourced value, and no night frame has ever been rendered through it.
- **Rejected: metering the picture.** Owner, 2026-08-06 — *„diesen komischen HDR-Effekt weg der alles
  dunkler macht wenn ich gegen was Helles schaue. Das ist unrealistisch. Weißabgleich macht das Auge."*
  The mechanism is not a bug in the meter; it is what any meter over the picture does, and it is a
  camera's iris. Measured cost of the mechanism: the same position and day meters **11.62 EV** of span
  at 18:25Z against **7.97 EV** at 11:00Z, i.e. the curve's shape is a function of where the camera looks
  as much as of the light. Do not re-propose a histogram meter, a spot meter, or a centre-weighted
  average — all three are the picture.
- **Rejected: a declared exposure.** Owner — *„exposure möchte ich nicht angeben müssen, später haben
  wir dynamische Tageszeiten und Wetter."* The `ExposureParams` declaration (`## Knowledge`) is built and
  measured (`manual keyEv −6.925` and plain auto agree to 0.01/255) and has **no consumer**; it is on its
  way out with the meter, not to be extended.
- **Rejected: the scene-declared stop as a factor on the key target.** Measured: +2 EV written that way
  came out as exponent **0.86** against **1.47** — brighter, with the contrast washed out of it. A
  declaration that bends the curve rather than sliding it is the wrong control whatever supplies the
  anchor.
- **There is no anti-aliasing at all.** [`../visual-target.md`](../visual-target.md) §2 names it the
  priority investment and §2.1 names TAA as the lever the 2015 bar was reached without; the built chain
  has neither TAA nor FXAA nor MSAA. Every argument in [`../lod.md`](../lod.md) about sub-τ transitions
  assumes a temporal filter that does not exist.
- **The upscale is bilinear.** Its own source carries the TODO: bicubic/sharpen. At a 720p source that is
  the difference between „soft by intent" and „soft by accident", and nothing measures which one the
  frame currently is.
- **„Cinematic" has no anchor.** [`../visual-target.md`](../visual-target.md) §2 states it in words —
  grain, gentle depth of field, colour grading, subtle motion blur. None of it is built and none of it is
  pinned to a reference frame, which is the kind of thing that drifts into taste.
- **The fixed-function raster path is *less* specified than the shaders**, and it lands exactly on this
  pass's priority: edge coverage „not defined", the **multisample resolve algorithm is not specified at
  all**, and alpha-to-coverage is „platform-dependent and can vary for different pixels" and not
  monotonic in alpha. Alpha-cutout foliage is the worst case in the scene and three of those four
  sentences aim at it. See [`../gpu-determinism.md`](../gpu-determinism.md).
- **Two stages derived numbers against a curve that no longer exists.** `NvisStage` inverts „the
  Narkowicz ACES fit the tonemap applies" to recover scene luminance, and `SpritesStage`'s sub-pixel
  energy floor is calibrated on „it reaches 231/255 at radiance 1.0 and 250 at …". Both are stale as of
  this round. Neither is active in the demo scene (an MFD bay and an effects billboard, both self-gating),
  which is why they were not re-derived here — but they are wrong now, not merely undocumented.
- **`kSceneExposure = 11.0` is a free scale, and under the new Spec it is worse than free.** Its
  derivation ends at „placing that at ACES input 0.32", a curve that is gone. An anchor derived from the
  irradiance is expressed in the same LUT units the irradiance is (TOA solar = 1), so the constant sits
  between two quantities that are already in one system. It could be 1.0. It stays only because moving it
  changes every shader's numbers.
- **`kNightAmbient = 0.00914` is still in `SurfaceLight.h`.** Its own comment says it exists because no
  auto-exposure existed. Retiring it needs a night frame — and under an irradiance anchor a night scene
  is the case where the anchor's input goes to nearly zero, which nothing has exercised.

## Knowledge

### Why the anchor is the irradiance and not the picture

One source was read in full for this: **[24]**, Pattanaik, Tumblin, Yee & Greenberg, 2000.
Everything numbered below is from it. Where it in turn cites someone, that is marked — those secondary
works were **not** read here and are not to be quoted as if they had been.

| Fact | Value | Where |
|---|---|---|
| operating range of human vision, **across all adaptation states** | ~10⁻⁶ … 10⁺⁸ cd/m², *„or 14 log10 units"* | [24] §1 |
| the same range in stops | **46.5 EV** — `14 / log10(2) = 14 × 3.3219` | derived from the row above |
| what the **iris** contributes | 2–8 mm of pupil moves retinal illumination by *„only about 1 log10 unit"* = **3.32 EV** | [24] §1, citing Wyszecki & Stiles, *Color Science*, Wiley 1982 |
| cone response range | ~10⁻¹ … 10⁺⁸ cd/m² | [24] §3, citing Hood & Finkelstein 1986 |
| rod response range | ~10⁻⁶ … 10⁺¹ cd/m², saturated above ~10⁺² | same |
| receptor response shape | Naka–Rushton `R(I) = Rmax · Iⁿ / (Iⁿ + σⁿ)`, σ the half-saturation intensity | [24] §3.1 eq. 1, Naka & Rushton 1966 |
| where the **display observer's** adaptation comes from | *„the output range of most displays is quite small and usually cannot cause large changes in the visual adaptation values… we assume display observers have fixed, steady-state adaptation amounts"* | [24] §4.3 |

**That last row is the whole argument.** The literature's own time-dependent adaptation operator assumes
the person looking at the screen is fixed-adapted to the room, and models adaptation only on the *scene*
side. The owner's position — *„Der Mensch regelt die Blende nach Helligkeit selber"* — is the same
statement, and it removes the scene-side model too: what is left is a placement, not a controller.

**Three claims that were carried into this round and are NOT supported by [24]**, stated plainly rather
than quietly dropped:

| Claim carried in | What [24] actually says |
|---|---|
| „~14 EV instantaneous" | the only 14 in the source is **14 log10 units**, and it is the **full** range across adaptation, not the instantaneous one. log10 and log2 differ by 3.3219; the two must not be read as the same number |
| „~24 EV with full adaptation" | the source's full-adaptation figure is **46.5 EV**. No source read here produces 24 |
| an instantaneous span at fixed adaptation | **[24] gives no number for it.** It says only that *„most retinal cells vary their response only within a range of intensities that is very narrow if compared against the entire range of vision"* (§3.1). Fig. 5 plots the response family at adaptation luminances 2·10⁻⁵ … 2·10⁺⁷ cd/m² but the caption states no width. **This number is owed and must be measured or sourced, not estimated** |

**The one anchor rule that needs no picture.** [24] §4.2: *„We follow Hunt's suggestion and determine
reference white as five times the current adaptation level and reference black as 1/32 the intensity of
reference white"* (eq. 8). Its own display constants instantiate it — adaptation luminance 25 cd/m²,
REFwht **125** cd/m² (= 5 ×), REFblk **4** cd/m², *„a maximum contrast of 32:1"* (§4.3) — where the 4 is
the rounding of 125/32 = 3.906. As ratios around the adaptation luminance A:

| Anchor | Ratio to A | In stops |
|---|---|---|
| reference white | **5 A** | +2.322 EV |
| reference black | **(5/32) A** — reference white / 32 | −2.678 EV |
| span | 32 : 1 | **5.000 EV** |

**Where [24] gets A is the interesting part, and it is not an image meter.** *„Several methods are
plausible, and the best choice may depend on the application"* (§4.1.2). For its own still images it took
the goal value as **one-fifth of the paper-white reflectance patch in a Macbeth chart** — a *known
reflectance under the scene's light*, not a statistic of the picture. Only for the driving sequence did
it meter the image, with the 1-degree foveal weighting of Ward-Larson et al., *aimed by hand at the
roadway*.

The irradiance is the same quantity as the Macbeth patch, without the chart: a known reflectance times
the light falling on it. Substituting it for A is *our* step and [24] does not take it — but it is the
step [24] takes for stills with a physical object standing in for the light. Applied to
`Renderer::ReadIrradiance()`, with the ground reflectance already measured for `kGroundBounce` below:

```
E_horiz = sunDirectNormalY · sin(sunEl) + skyDiffuseHorizY     ← IrradianceStage, LUT units, TOA solar = 1
A       = ρ̄ · E_horiz / π                                      ← Lambertian ground at the mean reflectance
white   = 5 · A          black = (5/32) · A
```

Worked for the measured frame (`sunEl 39.15°`, `sunDirectNormalY 0.844`, `skyDiffuseHorizY 0.0671`,
`ρ̄ = 0.12`):

| Step | Value |
|---|---|
| `E_horiz` | `0.844 · sin 39.15° + 0.0671` = **0.600** (matches the stage's own `totalHorizY`) |
| `A` | `0.12 · 0.600 / π` = **0.02292**, log2 = **−5.447** |
| white anchor | `5 · A` = **0.11459**, log2 = **−3.125** |
| black anchor | `A / 6.4` = **0.003581**, log2 = **−8.125** |
| span | **5.000 EV**, by construction (`log2 32`) |

**Lightness constancy is NOT cited here.** The intuition that a white sheet stays white in shade is the
usual justification for anchoring on the illumination rather than the pixel, and the literature for it
(Gilchrist's anchoring theory, Adelson) **was not read** — the round was cut short before it. The
argument above stands on [24]'s scalar adaptation luminance and on the owner's decision, not on
constancy. If a later round wants constancy as a reason, it has to go and read it.

**This is a placement, not a design.** Three things it does not settle, and none of them may be filled in
by feel: the exponent between the anchors (`## Gaps`), whether a 5.000 EV span from a 1995 CRT is the
right target for an sRGB screen that shows **7.45 EV** today (`## State`), and what A does at night when
`E_horiz → 0`. [24] gives the ratios; it does not give this project's display target.

### Why the ACES fit had to go, solved and not felt

The demo frame's HDR luminance, read straight off `ExposureStage`'s own histogram, spans **11.8 EV**
(2^−7.89 … 2^3.92). 45 % of the pixels are ground below 2^−6.7; the sky starts at 2^−2.5.

| Curve | Where it saturates | What it costs |
|---|---|---|
| Narkowicz ACES | output 1.0 at input **7.24**; output 250/255 at **3.05** | at the gain that lifts the ground to 45/255 (**+2.39 EV**, derived by inverting the fit on the measured foreground 0.00687), everything above 0.583 clips — roughly half the frame |
| Hable Uncharted-2, white metered | shoulder asymptote **0.9333**, so `f(x)/f(W)` cannot hold the 98th percentile under 0.955 at the needed slope | solved: the two constraints demand a curve ratio of 45.0 between the p98 and the ground, and Hable's best is 53.3 at g = 9.79 falling to 26.9 at g = 20 — the crossing needs `f(W) = 0.946 > 0.9333`. **No white point exists.** Built and measured anyway: **1.93 % clip but only 46.3/255**, on the band's edge on both sides |
| log domain, metered anchors | by construction: the white anchor IS the clip point | shipped |

Six stops of sky have to fit in the top of the range. That needs a logarithmic response there, and a
curve that is logarithmic at the top and not at the bottom is two curves. So the whole curve is one:

```
t   = clamp((log2(Y) - blackLog) / (whiteLog - blackLog), 0, 1)
out = pow(t, contrast)
```

with `contrast = log(kKeyTarget) / log(tKey)` — the exponent is not a look setting, it is what puts the
key at the target inside the range.

**This shape survives the change of anchor input; only where `blackLog` and `whiteLog` come from
changes** — from percentiles of the picture to `log2` of the two irradiance-derived anchors above. The
argument for the log domain is the sky's six stops, and the sky does not move because the meter does.
`tKey` is what does not survive unexamined: see `## Gaps`.

### The one scene-referred constant, derived from the measured frame

`kSceneExposure = 11.0` (`sim/src/render/stages/SceneScale.h`), **derived and not tuned by eye**:

| Step | Value |
|---|---|
| sun elevation of the measured frame | 39.15° |
| direct normal irradiance (model's own) | 0.844 |
| diffuse on horizontal (model's own) | 0.0671 |
| horizontal E_total | `0.844 · sin 39.15° + 0.0671` = **0.600** |
| scene radiance of ground at reflectance 0.15 | `0.15 · 0.600 / π` = **0.0287** |
| target ACES input for a sunlit mid-frame surface | **0.32** (the value whose sRGB output is 0.70) |
| exposure | `0.32 / 0.0287` = 11.2, **taken as 11.0** |

Measurement source: `walk/irradiance`, `sunDirectNormalY` and `skyDiffuseHorizY`.

The two constants that ride with it in the lit-surface splice (`SurfaceLight.h`) and their origins:

| Constant | Value | Origin |
|---|---|---|
| `kNightAmbient` | 0.00914 | **a display crutch, named as one** — it reproduces the ground level the previous model showed at night (`albedo · 0.4 · 0.08 = 0.032 · albedo`, i.e. `kSceneExposure · E / π = 0.032`). It exists because no auto-exposure existed; `ExposureStage` is what retires it |
| `kDeckDiffuse` | 0.5 | derived: a cloud deck is non-absorbing in the visible, so the beam it intercepts leaves it again — roughly half of it downward |
| `kGroundBounce` | 0.12 | **measured mean visible reflectance of Central European land cover** (grass ~0.10, tilled soil ~0.12, sealed ~0.09, broadleaf canopy ~0.05, concrete ~0.25). Without it a shaded wall is lit by the blue sky alone and comes out navy, which no photograph of a street shows |

The diffuse geometry the splice uses is derived rather than fitted: a tilted surface sees the fraction of
a uniform sky dome its normal subtends, which is exactly `(1 + n·up)/2`, and the complementary hemisphere
is bounced ground.

### The declaration, and it is optional

`sim/src/render/ExposureParams.h`, parsed by `Clients::Scene`. **Absent means auto with no
compensation**, which is why `mods/demo/scene.json` says nothing — every other field in that file stays
required, this one block does not exist unless a scene wants it.

```json
"exposure": { "mode": "auto",   "compEv": -0.5 }
"exposure": { "mode": "manual", "keyEv": -6.9, "adaptBrightS": 0.4, "adaptDarkS": 1.2 }
```

| Field | Meaning |
|---|---|
| `mode` | `auto` \| `manual`. Required if the block is present |
| `compEv` | auto only, required: stops on top of what the meter found |
| `keyEv` | manual only, required: the **scene radiance, log2**, that shall sit where the meter's own key would have. Units are the atmosphere LUTs' (TOA solar = 1) times `kSceneExposure` |
| `adaptBrightS`, `adaptDarkS` | optional, both modes: the two adaptation time constants in seconds |

A mode declared without its own stop field is refused by name (`missing or non-numeric field: keyEv`),
because a mode without its number is a scene that did not finish saying what it wanted.

**Measured equivalence of the two modes**, demo scene at 18:25Z (metered key −6.925):
`manual keyEv −6.925` and plain auto give `blackLog2 −8.1054` vs `−8.1057` and a lower-half mean of
48.40 vs 48.41 — one control, two ways of naming the same slide. `manual keyEv −5.925` and
`auto compEv −1.0` likewise agree to 0.01/255.

### Sources

| # | Source |
|---|---|
| 24 | Pattanaik, Tumblin, Yee & Greenberg, *Time-Dependent Visual Adaptation For Fast Realistic Image Display*, Program of Computer Graphics, Cornell University; ACM copyright 2000 — http://www.cs.ucf.edu/~sumant/publications/sig00.pdf |

Read in full. **The retrieved PDF is an author preprint: it carries the ACM 2000 copyright notice but no
venue line and no page numbers, so neither is stated here.** The title is transcribed off the title page
— note *„For Fast Realistic Image Display", not „for Realistic Image Display"*. Works it cites and this tree has **not** read: Wyszecki & Stiles, *Color Science* (Wiley
1982) · Hood & Finkelstein, *Sensitivity to Light*, in *Handbook of Perception & Human Performance* ch. 5
(Wiley 1986) · Hunt, *The Reproduction of Colour* (Fountain Press 1995), pp. 712, 721 · Naka & Rushton
1966. They appear above only as *„[24] citing X"* and may not be quoted directly until someone reads them.

The numbering is [`../visual-target.md`](../visual-target.md)'s; a number means the same paper everywhere
in `doc/render/`.
