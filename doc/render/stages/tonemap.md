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
| **the chain is exposure THEN curve, and the curve is fixed.** One metered scalar multiplies scene radiance; a filmic curve with no free parameter shapes what is above and below middle grey. The resolve shader owns no constant of the display chain | a curve whose shape moves with the light is two variables where one will do, and the shape is what carries the look. The scalar carries the light, the curve carries the look |
| the **AO composite belongs in the resolve shader**, not in a pass | that shader already reads every scene pixel, and it is the only place with both the radiance and the direct fraction the surfaces wrote into the HDR alpha |
| **the exposure comes from the ILLUMINATION, never from the picture.** `IrradianceStage`'s horizontal irradiance decides the scalar; where the camera points does not | owner, 2026-08-06: *„diesen komischen HDR-Effekt weg der alles dunkler macht wenn ich gegen was Helles schaue. Das ist unrealistisch."* An image meter darkens the world when the sun enters frame, which is a camera's iris, not a viewer. See `## Knowledge` — the eye's own iris is worth **3.32 EV** of a **46.5 EV** operating range [24] |
| **the key is a SURFACE and not a statistic**: the scene's declared mean land-cover reflectance under this frame's horizontal irradiance, seen face-on, placed at the curve's middle grey | [24] §4.1.2 takes the adaptation level for stills from a known reflectance under the scene light — one fifth of a Macbeth chart's paper-white patch. `kGroundBounce` is that known reflectance without the chart, and it is measured |
| **no exposure is ever declared by hand** | owner: *„exposure möchte ich nicht angeben müssen, später haben wir dynamische Tageszeiten und Wetter."* Sun elevation and cloud already move the irradiance; a declared stop would have to be maintained against both |
| **no time-dependent adaptation is modelled** | owner, 2026-08-06: *„Der Mensch regelt die Blende nach Helligkeit selber."* The viewer in front of the screen brings his own adaptation state, and [24] §4.3 assumes exactly that of a display observer. A photograph does not adapt either and is still legible |
| the frame's **latitude above the key is the curve's**, not the container's | a display's contrast ratio transplanted onto the scene side is what put the white anchor at scene radiance 2^7.51 while the frame's brightest pixel was 1.5 — 6.9 stops of empty headroom, and **0.000** pixels above L 200 in six of six reference frames |
| the exposure computation is **compute-only and rides an existing pass** | the per-frame render-pass count may not change for it |
| the exposure is **stops relative to the irradiance-derived key**, never absolute lux | the HDR target has no photometric calibration anywhere, so an EV100 field would be a number with nothing under it |
| the curve runs **per channel** | the shoulder desaturating a channel that runs past white is what a photograph of a clear sky does — its blue saturates while red and green climb. Applied to luminance alone the ratio is kept and the channel clips square |
| the frame is rendered at **fixed 720p** and upscaled to the display | [`../visual-target.md`](../visual-target.md) §2: a quarter of 1440p's pixels, and the film look is what hides the resolution |
| **anti-aliasing is the priority investment** and it lands here | at 720p, edges are the dominant defect; alpha-cutout foliage in motion is the worst case in the scene |
| **a reference photograph is a ruler for RATIOS INSIDE ~2 EV and for nothing wider** | MEASURED at the nebelhorn fit pose: the photograph puts the clear sky of row 2 at **1.74×** the sunlit karst of the same frame, where this renderer's own probe puts it at **0.162×** and a hand check off the irradiances puts it at **0.23…0.36×** (`E_sky/π` against `ρ·E_global/π`, at the model's own `0.0539/0.841` and again at 100/980 W m⁻² from the literature). The webcam is **2.3…3.4 EV** out on that one pair. Ground against ground it is usable; ground against sky it is not, and an acceptance number taken across that span measures the camera |

## State

**The chain is exposure, curve, upscale.** `ExposureStage` (one compute dispatch, riding the sky-view
pass right after the dispatch that writes its input) publishes four floats; `TaaStage`'s resolve
fragment multiplies by the first of them and applies `stages/Filmic.h`; `UpscaleStage` resamples the
fixed-720p `FrameTex` onto the display. `passcount` stays **7** (measured, `render/passcount`, before
and after this round).

```
expScale = kFilmicMid / (kGroundBounce · E_horiz / π · kSceneExposure)      ← ExposureStage
out      = filmic(lit · expScale)                    per channel, clamped   ← TaaStage::resolved()
```

| Quantity | Value | Origin |
|---|---|---|
| key radiance | `kGroundBounce · E_horiz / π · kSceneExposure` | [24] §4.1.2's known reflectance under the scene light; `kGroundBounce = 0.12` is the measured mean visible reflectance of Central European land cover |
| `kFilmicMid` | **0.13017527** | derived: the positive root of `2.0726 x² − 0.0762 x − 0.0252 = 0`, i.e. `filmic(x) = 0.18` solved exactly |
| `kFilmicWhite` | **7.24166**, = **5.798 EV** over middle grey | derived: `filmic(x) = 1` at `x² − 7x − 1.75 = 0`. This is the frame's latitude above the key, and it replaces the 11.686 EV sRGB-container span |
| the curve | Narkowicz' rational fit of ACES RRT + sRGB ODT (2015), per channel | scene-referred in, display-LINEAR out; the sRGB surface format does the encode |
| night floor on E | `kNightAmbient` | the residual illumination `SurfaceLight.h` already adds to every surface. **Unexercised — no night frame** |
| measured, nebelhorn fit pose 2026-07-28 11:00Z | `horizE 0.840962`, `keyLog2 −1.50085`, `expScale 0.368407` | `walk/exposure`, binary md5 `ac7ebf39409b80d65384bcdd0455e60a` |
| `FB_GEOM` freeze | **1.886873** | derived from the demo scene's own irradiance (`horizE 0.164203`): `kFilmicMid / (0.12 · 0.164203 / π · 11)`. Checked: `FB_GEOM=1` and the metered path render the demo frame to the same mean 163.5, p1 84.9, p99 242.1 |

**The curve's local gamma is measured, because it is what turns a scene ratio into a display ratio.**
`d log filmic / d log x`, evaluated at the two levels the nebelhorn ground actually occupies:
**1.561** at `x = 0.0535` (the sward, 1.9 EV under the key) and **0.780** at `x = 0.3124` (the karst,
1.25 EV over it). A 2.53 EV scene ratio therefore leaves the curve as **3.01 EV**. That expansion is
an S-curve doing what an S-curve does — it is the frame's contrast — and it is a term in every
„two classes are too far apart" accounting, never the whole of one.

**What replaced what, and it was one decision with four measurements.** The curve that stood here was a
logarithmic ramp between an irradiance-derived black anchor and a white anchor **11.686 EV** above it —
the sRGB container's own code-1-to-code-255 ratio, transplanted onto the scene side. Against the six
measured reference poses (`sim/tools/skyaudit.py`, `sim/web/cams/<slug>-fit.png` vs `-fit.jpg`) that put
the white anchor 6.9 stops above anything in the frame:

| | before (md5 `5e0d6ae4…`) | after (md5 `ac7ebf39…`) | photograph |
|---|---|---|---|
| pixels with L > 200 | **0.0000 in 6 of 6** | 0.0000 / 0.0000 / 0.0274 / 0.0970 / 0.0090 / 0.0210 | 0.199 / 0.154 / 0.104 / 0.314 / 0.032 / 0.126 |
| luminance stddev, whole frame | 24 … 37 | **33 … 53** | 46 … 75 |
| mean local gradient, lower half | 9.24 / 12.55 / 7.66 / 14.15 / 13.83 / 10.92 | **9.10 / 15.64 / 10.95 / 22.47 / 14.01 / 12.70** | 22.56 / 12.79 / 29.17 / 12.21 / 37.18 / 15.63 |
| frame price, 360-frame turntable at 1280×720 | p50 **4.586** p95 **5.436** p99 **5.756** ms | p50 **4.262** p95 **4.520** p99 **4.655** ms | — |

Order of the cameras throughout: nebelhorn · herzogstand · innsbruck · hochries · zugspitze · hochkoenig.

### The exposure is the illumination, and that is checkable

`expScale` is a pure function of `E_horiz`, so a change of light moves it by exactly the light's own
stops and a change of heading does not move it at all. `walk/exposure` publishes both numbers, and the
before/after of this round moved neither: `horizE 0.840962` and `keyLog2 −1.50085` are unchanged, which
is the point — only what is done with them changed.

### The probe is the only ruler, and it survived the change

`FB_TONE_PROBE=black,white` swaps the curve for `clamp((log2 Y − black)/(white − black))` with the
channel ratio carried through, so a PNG read back through the sRGB decode **is** the frame's HDR
histogram, per channel. It is baked as a `const`, so exactly one of the two branches is compiled.
Reading it back needs the range wide enough that the sky stays inside the ruler; the reconstruction is
`Y = 2^(black + y·(white − black))` with `y = dot(rgb_linear, luma)` and `L_channel = rgb_linear · Y/y`.

Measured with it on the nebelhorn fit pose (`FB_TONE_PROBE=-40,30`), scene radiance in `kSceneExposure`
units, band x = 120…200:

| image row | elevation | R | G | B | Y | B/R |
|---|---|---|---|---|---|---|
| 2 | +22.75° | 0.546 | 0.608 | 0.836 | 0.612 | **1.53** |
| 45 | +8.72° | 0.578 | 0.800 | 1.238 | 0.784 | **2.14** |

against the same model computed offline at 512 march steps with the same LUT chain — 0.129 / 0.275 /
0.608 (B/R **4.73**) and 0.279 / 0.555 / 1.063 (B/R **3.81**). The whole difference is
(0.417, 0.333, 0.228) and (0.299, 0.245, 0.176), whose ratios are 1 : 0.80 : 0.55 — the tint of the sun
halo `SunStage` used to add, at exactly the amplitude its own formula gives at those two scattering
angles. That is how `celestial.md`'s glow row got its measurement.


## Gaps

- **Two of the four acceptance numbers of 2026-08-08 are not met, and both are content, not curve.**
  Mean local gradient in the lower half reaches half the photograph's in **3 of 6** (herzogstand,
  hochries, hochkoenig). Split by depth band it is not the far field that is short: in rows 90–120 the
  render exceeds the photograph at herzogstand (19.1 / 7.2), hochries (32.8 / 9.4) and hochkoenig
  (12.4 / 9.6); it is short in the NEAREST band, rows 150–179, at nebelhorn (10.3 / 23.1), innsbruck
  (10.8 / 37.3) and zugspitze (11.6 / 36.6) — where there is no aerial perspective at all and the
  photograph has limestone, scree, footpaths, cable cars and individual trees that this renderer does
  not model. That is the rock/scree class round, not this one.
- **The darkest 2 % is not blue-shifted, and for the population the frame darkens most it cannot be.**
  Measured on the nebelhorn fit pose through the probe: the darkest 2 % has scene radiance
  (0.0223, 0.0345, 0.0181), which is the product of the integrated sky (0.0262, 0.0553, 0.1220 — B/R
  4.66) and a surface whose blue reflectance is 0.24 of its green. B/G comes out 0.52 and is measured at
  0.52. Those pixels are near-field cast shadow at a few hundred metres; the photograph's darkest 2 % at
  nebelhorn sits at rows 92–144, i.e. shaded slopes 5–10 km out where inscatter dominates and the colour
  is the air's. **The reference photographs themselves satisfy the criterion in 2 of 6.**
- **A clear-sky shadow is 4.2 % of the sunlit level in this model and the literature says 10–15 %.**
  Derived from the model's own parts on flat ground: `E_sky/E_global = 0.0539/0.841 = 6.4 %`, times the
  `(1 − kSelfShelter) = 0.65` the sky term is weighted by, plus a neighbourhood bounce that is exactly
  zero because `(0.5 − 0.5 n·up) = 0` on a horizontal surface. 6.4 % is itself low: measured diffuse
  fractions at 2 000 m under a clear sky with the sun at 61° run 8–12 %. Neither half is fixed here and
  neither is a `[SET]` that can simply be moved: see [`../lighting.md`](../lighting.md).
- **The frame's brightest ground is exactly the key and its median is 1.25 EV under it.** Measured on
  nebelhorn: ground p100 `Y = 0.350` against a key of 0.353, ground p50 `Y = 0.148`. The key is the
  radiance of a FLAT surface at the mean reflectance in full sun, so a landscape of slopes sits below it
  by its own mean cosine. The lower-half median comes out 64 … 103 against the photographs' 77 … 114.
  **Whether the key should carry a mean-cosine term is still open, and it is now bounded from both
  sides.** Upper bound, derived: total flux onto a closed rough surface is `E_horiz · A_horiz`, so the
  mean per unit TERRAIN area is `E_horiz · ⟨cos slope⟩` and the key falls by at most
  `−log₂⟨cos slope⟩` = **0.21 EV at a 30° mean slope, 0.38 EV at 40°** — the frame gets that much
  brighter, no more. Measured against what it would have to buy: the rock/sward display ratio at
  nebelhorn reaches the photograph's 2.50 only at **+2.05 EV** of exposure, at which point the karst
  sits at display-linear **0.856** (code ≈ 240) against the photograph's **0.320** and the frame mean
  at **0.56** against **0.312**. The mean cosine is worth a sixth of that and spends all of it making
  the rock whiter. So it is not the lever for the spread; it is worth having for its own sake if and
  when a landscape-wide slope statistic exists that is not a statistic of the picture.
- **The per-channel/luminance question is answered and the answer is „keep per channel", by 0.03 EV.**
  A/B offline on the delivered nebelhorn frame (the curve is invertible, so the frame IS its own HDR
  source): applying `filmic` to luminance with the channel ratio carried through moves the rock/sward
  display ratio **8.08 → 8.23**, i.e. the WRONG way. What it does buy is chroma — the sward's
  `R/G 0.480 → 0.620` and `B/G 0.429 → 0.549` against the photograph's `0.840 / 0.549` — and the
  whole-frame mean/sd move 0.1963/0.1567 → 0.1979/0.1591 against 0.3120/0.2209, both marginally
  toward the photograph. A 0.03 EV cost against a Spec row that has a measurement of its own (the
  clear sky's blue pinning while red and green climb) is not enough to move the row. It is recorded so
  that the next round does not re-run it.
- **`kSceneExposure = 11.0` is now a pure unit choice with a stale derivation.** Its comment still ends
  at „placing that at ACES input 0.32", which was a different placement in a different chain. Under the
  present chain `expScale` divides it straight back out: the product `kSceneExposure · expScale` is the
  only thing the picture sees. It could be 1.0. It stays only because moving it changes every shader's
  numbers.
- **`ExposureParams` has no consumer and neither mode has been exercised since the round that redefined
  them.** `keyEv` is now the scene radiance, log2, that shall sit at middle grey, and `compEv` is stops
  on top of the irradiance-derived key. `mods/demo/scene.json` declares no exposure block and must not be
  given one.
- **The night case is untouched.** `E_horiz → 0` is floored at `kNightAmbient`, a `[SET]` display crutch
  with no sourced value, and no night frame has ever been rendered through it.
- **Rejected: metering the picture.** Owner, 2026-08-06 — *„diesen komischen HDR-Effekt weg der alles
  dunkler macht wenn ich gegen was Helles schaue. Das ist unrealistisch. Weißabgleich macht das Auge."*
  The mechanism is not a bug in the meter; it is what any meter over the picture does, and it is a
  camera's iris. Measured cost of the mechanism: the same position and day meters **11.62 EV** of span
  at 18:25Z against **7.97 EV** at 11:00Z, i.e. the curve's shape was a function of where the camera
  looks as much as of the light. Do not re-propose a histogram meter, a spot meter, or a centre-weighted
  average — all three are the picture.
- **Rejected, with its measurement: an 11.686 EV span taken from the sRGB container.** It is a display's
  contrast ratio used as a scene's range, and against the six reference poses it left **0.000** of every
  frame above L 200 while the photographs carry 0.032 … 0.314 there, with luminance stddev 24 … 37
  against 46 … 75. A display's range is not a scene's range.
- **Rejected: a declared exposure.** Owner — *„exposure möchte ich nicht angeben müssen, später haben
  wir dynamische Tageszeiten und Wetter."*
- **There is no anti-aliasing debt left here but there is no bloom either.** With the authored sun halo
  gone (`celestial.md`) a 0.5° disc is a hard three-pixel dot at 320×180. Glare is an optical property of
  a lens or an eye, not of the scene, and modelling it is a pass.
- **The upscale is bilinear.** Its own source carries the TODO: bicubic/sharpen. At a 720p source that is
  the difference between „soft by intent" and „soft by accident", and nothing measures which one the
  frame currently is.
- **„Cinematic" has no anchor.** [`../visual-target.md`](../visual-target.md) §2 states it in words —
  grain, gentle depth of field, colour grading, subtle motion blur. None of it is built and none of it is
  pinned to a reference frame, which is the kind of thing that drifts into taste.
- **The fixed-function raster path is *less* specified than the shaders**, and it lands exactly on this
  pass's priority: edge coverage „not defined", the **multisample resolve algorithm is not specified at
  all**, and alpha-to-coverage is „platform-dependent and can vary for different pixels" and not
  monotonic in alpha. See [`../gpu-determinism.md`](../gpu-determinism.md).
- **Two stages derived numbers against curves that no longer exist.** `SpritesStage`'s sub-pixel energy
  floor is calibrated on „it reaches 231/255 at radiance 1.0 and 250 at …", which was true of neither the
  log ramp nor this curve. It is self-gating and not active in the demo scene, which is why it was not
  re-derived here — but it is wrong now, not merely undocumented.


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

**What is taken from [24] is A, and only A.** Its reference white = 5 A and reference black = A/32
(§4.2, eq. 8) are instantiated on its own display — adaptation luminance 25 cd/m², REFwht 125 cd/m²,
REFblk 4 cd/m², *„a maximum contrast of 32:1"* (§4.3) — and that is a **display's** contrast, measured on
a 1995 CRT. This tree used it as a scene range twice, once at 5.000 EV and once at the sRGB container's
11.686 EV, and both are in `## Gaps` with what they cost. The latitude above the key now comes from the
curve that has to carry it (`kFilmicWhite`, 5.798 EV), which is the only place it can honestly come
from.

**Where [24] gets A is the interesting part, and it is not an image meter.** *„Several methods are
plausible, and the best choice may depend on the application"* (§4.1.2). For its own still images it took
the goal value as **one-fifth of the paper-white reflectance patch in a Macbeth chart** — a *known
reflectance under the scene's light*, not a statistic of the picture. Only for the driving sequence did
it meter the image, with the 1-degree foveal weighting of Ward-Larson et al., *aimed by hand at the
roadway*.

The irradiance is the same quantity as the Macbeth patch, without the chart: a known reflectance times
the light falling on it. Substituting it for A is *our* step and [24] does not take it — but it is the
step [24] takes for stills with a physical object standing in for the light:

```
E_horiz  = sunDirectNormalY · sin(sunEl) + skyDiffuseHorizY   ← IrradianceStage, LUT units, TOA solar = 1
keyL     = ρ̄ · E_horiz / π · kSceneExposure                   ← Lambertian ground at the mean reflectance
expScale = kFilmicMid / keyL
```

Worked for the nebelhorn fit pose (`sunEl 61.035°`, `sunDirectNormalY 0.899566`,
`skyDiffuseHorizY 0.0539189`, `ρ̄ = kGroundBounce = 0.12`):

| Step | Value |
|---|---|
| `E_horiz` | `0.899566 · sin 61.035° + 0.0539189` = **0.840962** (the stage's own `totalHorizY`) |
| `keyL` | `0.12 · 0.840962 / π · 11` = **0.35326**, log2 = **−1.5013** (`walk/exposure` reports −1.50085) |
| `expScale` | `0.13017527 / 0.35326` = **0.36849** (`walk/exposure` reports 0.368407) |
| what clips | `kFilmicWhite / expScale` = scene radiance **19.65**, i.e. **5.80 EV** over the key |

**Lightness constancy is NOT cited here.** The intuition that a white sheet stays white in shade is the
usual justification for anchoring on the illumination rather than the pixel, and the literature for it
(Gilchrist's anchoring theory, Adelson) **was not read** — the round was cut short before it. The
argument above stands on [24]'s scalar adaptation luminance and on the owner's decision, not on
constancy. If a later round wants constancy as a reason, it has to go and read it.

**This is a placement, not a design.** Two things it does not settle, and neither may be filled in by
feel: whether the key should carry the mean cosine of a landscape of slopes rather than a horizontal
plate (`## Gaps`), and what the key does at night when `E_horiz → 0`. [24] gives A; it does not give
this project's display target.

### Why the ACES fit is back

It was thrown out on a measurement that is real and was read the wrong way round. The finding was that
the demo frame spans 11.8 EV with its ground 8 stops under its sky, that Narkowicz saturates at an input
of 7.24, and that at the gain which lifts the ground to 45/255 everything above 0.583 clips — about half
the frame. Every number there is right. What is wrong is the conclusion: an 8-stop gap between ground
and sky is not a property of a sunny day, it is a property of a frame whose exposure was placed 6.9
stops below its own content. On the six measured reference poses the same scene puts its brightest
ground at the key and its horizon sky **1.33 EV** above it — a 5.798 EV shoulder is not tight, it is
roomy.

The curve, both constants derived from the fit itself:

```
filmic(x) = clamp( x(2.51x + 0.03) / (x(2.43x + 0.59) + 0.14), 0, 1 )
kFilmicMid   = 0.13017527   filmic(x) = 0.18  →  2.0726 x² − 0.0762 x − 0.0252 = 0
kFilmicWhite = 7.24166      filmic(x) = 1     →  x² − 7x − 1.75 = 0
```

**Per channel and not on luminance.** The reason the per-channel form was rejected — the blue channel of
99.96 % of the foreground came out at exactly 0 — was a property of the ramp it was tried in, which
subtracted a black anchor before it divided. `filmic` is a rational through the origin: `filmic(0) = 0`
and nothing else reaches 0. What per-channel buys is the highlight desaturation a photograph shows,
measured on the reference photographs themselves: nebelhorn's sky runs (155, 194, 253) at row 2 and
(204, 225, 253) at row 45 — blue pinned, red and green climbing.

### The one scene-referred constant, and it is a unit and not an exposure

`kSceneExposure = 11.0` (`sim/src/render/stages/SceneScale.h`). Its derivation belonged to a chain in
which it WAS the exposure; under the present chain `expScale = kFilmicMid / (ρ̄ · E/π · kSceneExposure)`
divides it straight back out, so only the product reaches the picture and the constant is a choice of
unit for the HDR target. It matters for one thing only — the numeric range the `rgba16float` scene
target has to carry — and it is in `## Gaps`.

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
"exposure": { "mode": "manual", "keyEv": -1.5 }
```

| Field | Meaning |
|---|---|
| `mode` | `auto` \| `manual`. Required if the block is present |
| `compEv` | auto only, required: stops on top of the irradiance-derived key |
| `keyEv` | manual only, required: the **scene radiance, log2**, that shall sit at the curve's middle grey. Units are the atmosphere LUTs' (TOA solar = 1) times `kSceneExposure` |

A mode declared without its own stop field is refused by name (`missing or non-numeric field: keyEv`),
because a mode without its number is a scene that did not finish saying what it wanted.

Both modes end in the same one line — `expScale = kFilmicMid / anchor` — so a scene can move the
exposure and cannot bend the curve. **Neither mode has been exercised since it was redefined**
(`## Gaps`).

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
