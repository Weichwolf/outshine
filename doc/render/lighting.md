# Lighting — the one equation, its four terms, and where each is measured

> Owner, 2026-08-06: *„du renderst einfach realistisch. Wer oder was betrachtet ist dir egal."* ·
> *„etwas Ambient ist auch nicht verkehrt. Die Erde leuchtet ja selbst und auch in der Nacht ist es
> nicht komplett dunkel. Ist natürlich epochenabhängig."* · *„Lighting ist ja essentiell für das
> Compositing."*

Cross-cutting, like [`lod.md`](lod.md): every pass either produces a term of the equation below or
consumes it. Before this file the subject lived in seven documents at once — `renderer.md`,
`visual-target.md`, `stages/{atmosphere,shadow,ao,tonemap}.md` and `clouds.md` — and nowhere as a whole.

## Spec

### 1. Render the physics. There is no viewer.

**No camera model, no eye model, no adaptation.** The scene has a radiance; it is displayed. The observer
in front of the screen brings their own adaptation, and a photograph does not adapt either.

One consequence, and it is the whole reason exposure exists at all: night to noon spans roughly 20 EV
and a display carries about 8. So the **mapping** from scene radiance to display follows the scene's
**illumination** — sun elevation and cloud cover — and **never the image content**. Turning the head
does not change the physics of the scene, so it must not change the picture's exposure.
Details and the measured curve: [`stages/tonemap.md`](stages/tonemap.md).

### 2. The equation, and it is shared by every lit surface

One header, `render/stages/SurfaceLight.h`, so terrain, buildings, ground cover and everything later
cannot drift apart:

```
L = exposure/π · albedo · ( E_sky·(1+n·up)/2  +  E_bounce·(1−n·up)/2  +  E_sun·(N·L)·shadow )
```

Four terms, four owners:

| Term | What it is | Produced by | State |
|---|---|---|---|
| **E_sun** | direct solar irradiance through the atmosphere | transmittance LUT → [`stages/atmosphere.md`](stages/atmosphere.md) | built |
| **E_sky** | the hemisphere integral of the sky-view LUT — the sky as an area light | `IrradianceStage` | built |
| **shadow** | occlusion of the direct term | [`stages/shadow.md`](stages/shadow.md) (CSM), [`stages/ao.md`](stages/ao.md) for the sky term | built |
| **E_bounce** | light returned by everything already lit | `litRadiance`, in TWO half-spaces (§2.1) | built |

Measured at sunEl 39.15°, in TOA-solar = 1: `sunDirectNormalY 0.844 · skyDiffuseHorizY 0.0671 ·
totalHorizY 0.600`. From those two the shadow ratio follows as
`E_sky/(E_sky + E_sun·sinθ) = 0.0671/0.600 = 0.112`, and the renderer measures **0.108** — agreement to
4 %. The lighting is consistent with its own irradiances; where the picture still disagrees, the cause
is downstream (the tone curve) or a missing term (§4), not the equation.

### 2.1 E_bounce is two terms, because "what returns light here" is two questions

A surface's lower half-space and its own sub-pixel relief are lit by different things and reflect with
different spectra, so one weight cannot carry both:

| Half-space | Weight | Reflectance | Lit by |
|---|---|---|---|
| **far** — below the normal's horizon | `(1 − n·up)/2` | `kGroundBounce` 0.12, GREY | the AREA-MEAN deck transmittance; a wall does not know what colour the street is |
| **near** — the surface's own relief | `(1 + n·up)/2 · kSelfShelter` | `alb`, the fragment's OWN | this fragment's LOCAL sun and shadow |

The near term is the one that was missing, and it is what the geometric formula cannot express: on flat
ground `(1 − n·up)/2` is zero, a plane cannot see itself, and yet a shadow on soil is a darker soil and
not a blue hole — the millimetre relief that CASTS the micro-shadow is also what FILLS it, with light
off the same material. Idso's own soil is Munsell 10YR 5/3 dry and 10YR 3/3 wet: same hue, other value.
`kSelfShelter` = 0.35 is **[SET]** — the sky fraction a clod, a furrow or a sward hides from a point
between them, i.e. what SSAO measures one scale below what SSAO can resolve. It has no measurement
under it and that is a named gap.

The near term takes `(1 − kSelfShelter)` off the sky weight and hands it to the bounce, so a LOW-albedo
surface ends up darker than before and a high-albedo one about the same. That is the intended
asymmetry, and it is where the tonal spread comes from.

**Measured** at the reference scene (`mods/demo/scene.json`, sun 11.20°, 1280×720), isolated by
building `kSelfShelter` at 0 and 0.35 against one material table:

| | 0 (as before) | 0.35 |
|---|---|---|
| hue difference, darkest 10 % vs brightest 10 % of the near field (< 30 m) | 33.1° | **29.9°** |
| B/R of those darkest 10 % | 0.487 | **0.400** |
| tonal spread, 0.5/99.5 percentile of linear luminance over ground pixels | 4.98 EV | **7.07 EV** |

Of the +2.19 EV the whole round gained, **+2.09 EV is this term** and +0.10 EV the cloud deck: the
spread the reference photographs have comes from bounce-starved dark material, not from cloud shadow.

### 2.2 The air under a deck is darker too

`deckHazeFactor` scales the terrain's aerial-perspective inscatter by the ratio of the horizontal
irradiance under the deck to the clear-sky one — the same quantity `litRadiance` builds from
`I.sun`, `I.sky`, `thruMean` and `kDeckDiffuse`, so there is no second model and no new constant. It
exists because of a measurement: with the inscatter left at clear-sky strength, a **forced 98 %** ground
shadow beyond 4 km moved the 5–15 km band by **1.4 %** of display luminance (0.6880 → 0.6783). The far
field at this sun elevation is inscatter, not ground.

### 2.3 An occluder is a surface, not a hole

`litRadiance` writes into the HDR target's alpha the fraction of the pixel's radiance that screen-space
occlusion may **not** take, and `TonemapStage` applies `mix(ao, 1.0, alpha)`. That fraction is the
direct beam — the shadow map already ruled on it — **plus `kGroundBounce` of the ambient**, because
whatever the AO estimate found in the way is a surface and returns light like any other piece of
neighbourhood. The composite cannot know what occluded the pixel, only that something did, so the same
grey neighbourhood figure the far half-space uses is the whole answer available:

```
alpha = 1 − ambFrac·(1 − kGroundBounce)
```

Without it a surface whose direct fraction is zero — every sun-averted wall — is handed the raw
occlusion estimate on **all** of its radiance, and an AO error becomes a black hole rather than a
darker version of the neighbourhood.

**Measured** at the reference scene, camera 210 m from a free-standing farm building, sun 11.20°,
1280×720, on a 9 488 px mask of its sun-averted walls:

| | before | after |
|---|---|---|
| wall pixels below display code 64 | 1 548 (16.32 %) | **135 (1.42 %)** |
| display code p01 / p10 / p50 / p90 | 43 / 51 / 101 / 118 | **63 / 72 / 104 / 118** |
| HDR log2 L, p01 / p10 / p50 (`FB_TONE_PROBE=-14,2`) | −6.249 / −5.578 / −4.555 | **−5.773 / −5.373 / −4.524** |
| sky and cloud pixels changed (rows 0–300) | — | **0, bit-exact** |
| whole-frame mean display code | 146.084 | 146.226 |
| metered anchors `blackLog2 / whiteLog2 / contrast` | −6.5387 / 5.14729 / 1.09241 | **unchanged** |

The lift is `(ao + (1−ao)·0.12)/ao`: **+0.50 EV** where the estimate sits at the AO floor 0.25,
**+0.06 EV** at the median, and exactly zero where `ao = 1`.

### 3. Ambient is not a fudge — it is the light that is actually there

`kNightAmbient` exists today and its own comment calls itself a crutch for the auto-exposure. As
**residual illumination** it stops being a crutch and becomes a term with a cause. In falling order of
contribution:

| Source | Note |
|---|---|
| **Skyglow** — artificial light scattered by the atmosphere | dominant near settlement, and **the epoch axis** (§3.1) |
| **Moonlight** | strongly variable; full to new moon is orders of magnitude |
| **Airglow** — the upper atmosphere emits by recombination | literally „the earth glows by itself"; why a moonless night is not black |
| Starlight, zodiacal light | the remainder |

**None of these numbers is sourced yet.** They are named here as terms, not as values — a value without
provenance is a defect (`CLAUDE.md`).

### 3.1 Residual light is an epoch parameter

The same dial that drives decay and building type ([`classification.md`](classification.md)) drives what
the night looks like — it is an axis, not a switch:

| Epoch | Night |
|---|---|
| pre-industrial | no grid. Moon, stars, fire. A night in which one genuinely cannot see |
| present | street lighting, windows, skyglow over the town; the cloud deck is lit **from below** |
| decay | the grid is gone, the geometry remains. Darker than the present — and the skyglow is missing exactly where the town is |

That the deck is lit from below in the present and not in the other two is the kind of difference a
single parameter should produce, and it is why residual light belongs on the epoch axis rather than in a
constant.

## State

- The equation and its three built terms are in `render/stages/SurfaceLight.h`, shared by terrain,
  buildings and ground cover.
- Irradiance is read back by `Renderer::ReadIrradiance()`; the numbers in §2 are from that path.
- CSM (4 cascades, one atlas) and SSAO are built; the AO composite is weighted by the fraction
  occlusion may not take (§2.3), so occlusion darkens sky light and not sunlight, and what it darkens
  keeps `kGroundBounce` of itself.
- **Exposure is placed from the irradiance** (`ExposureStage`, one compute dispatch on eight floats in
  the sky-view pass, reading `IrradianceStage`'s new `sky.w` = E on a horizontal surface). Measured:
  four frames at yaw 0/90/180/270 give the SAME anchor to every printed digit
  (`black −6.53538 · white +5.15062 · contrast 1.09241`), where the image meter moved white by
  0.281 EV and the exponent by 0.126 across the same four. There is no histogram left to meter with.
- `kNightAmbient` is a `[SET]` constant with no sourced value.

## Gaps

- ~~E_bounce does not exist~~ — **closed**, §2.1, in two half-spaces. What replaces it as a gap:
  `kSelfShelter` 0.35 is `[SET]` with no measurement, and the near term uses the fragment's own albedo
  as the reflectance of its own neighbourhood, which is right for ground and vegetation and wrong for a
  painted door in a brick wall. **Now measured on a smooth wall** (reference scene, sun-averted facade,
  `alb_Y` 0.373, `E_sky,h` 0.0946, `E_sun,h` 0.0306 through the deck): the isotropic bound
  `0.5·E_sky,h + 0.5·0.12·(E_sky,h + E_sun,h)` = 0.0548, delivered 0.0444 — the term costs a smooth
  vertical wall **−0.303 EV** and gives back only `kSelfShelter·alb` = 0.13 of what it took. It is not
  what makes walls black, and it is not fixed here because the constant is shared with terrain and
  vegetation; moving it is its own measured round.
- **The AO estimator is resolution-starved long before its own guard fires.** MEASURED on a
  free-standing wall with nothing within 0.9 m of it, where the true answer is ~1, by rendering the
  identical camera at three resolutions and reconstructing the AO factor from an `FB_GEOM` pair:
  disc radius **2.67 px → mean AO 0.662**, **5.34 px → 0.861**, **8.02 px → 0.885**, with a fixed
  moiré of ±0.35 at 2.67 px. `kAoMinPx` = 2.5 admits exactly the band where the estimate carries no
  geometry, and `aoHash`'s per-pixel rotation is never resolved by anything. §2.3 stops the result
  being black; it does not make the estimate right. Owner of the estimator:
  [`stages/ao.md`](stages/ao.md).
- ~~**The deck's downward re-emission carries the GROUND-LEVEL solar spectrum.**~~ **CLOSED
  2026-08-06 — and the diagnosis under it was wrong, which is the more useful half.** `litRadiance`
  now takes the deck term from `I.sunDeck`, a third irradiance `IrradianceStage` publishes: the beam
  at the deck base, brought down the VERTICAL column instead of the sun's 11° slant. Measured at the
  reference scene: `sun 0.7832/0.5610/0.3423` against `sunDeck 0.8212/0.6077/0.4025`, i.e. **+4.9 % R,
  +8.3 % G, +17.6 % B**, and B/R rises 0.437 → 0.490. It is MARCHED, not tapped — the transmittance
  LUT has 64 rows over 100 km, so its first two texel centres are 781 m and 2 344 m and a 1 200 m deck
  base falls inside one of them; tapping it gave a blue lift of 4.7 % where the model's own extinction
  gives 19.6 % (hand integral: Rayleigh + Mie over the slant leg minus the vertical leg). The march
  uses `getScatteringValues`, the same model the LUT is baked from, 16 steps, once per frame in a
  one-thread dispatch.
  **What it did NOT fix:** the green-under-both regression it was supposed to explain. A control build
  with the deck term left on `I.sun` and only the specular corrected already measures **0.00 %** in the
  30–200 m band. The 9.39 % was the environment specular (below), not the deck's spectrum.
- **The far field cannot be structured by ground shading at this sun elevation.** MEASURED: forcing the
  terrain's cloud transmittance to 0.02 beyond 4 km moves the 5–15 km band's display luminance by 1.4 %
  and its large-scale modulation not at all. A 0.15-albedo ground under 0.164 horizontal irradiance has
  radiance ≈ 0.0078 while the horizon inscatter it is seen through is 0.03–0.10, so the pixel is ~85 %
  air. Whatever structures that band, it is not the light on the ground.
- **The display span is the one number in the exposure chain that is not sourced end to end**, and it
  costs one acceptance band: 5.77 EV shown against a band of ≥ 6. `stages/tonemap.md` `## Gaps`.
- ~~The far field draws cartographic colours as reflectance~~ — **closed.** The raster colour is an index
  and the reflectance comes per class from `ground-materials.json`; the ground also carries a GGX/Smith
  specular lobe and an environment reflection sampled in the MIRROR DIRECTION on top of `litRadiance`, which is where
  roughness becomes an image ([`stages/terrain.md`](stages/terrain.md) `## Spec`). What
  replaces it: the table is BROADBAND reflectance fed into a visible-band pipeline, and `E_bounce`'s
  absence now shows as a specular that overpowers a diffuse floor that is too low — both in that file's
  `## Gaps`.
- **The DIRECT specular still carries 15–44 % of the ground's display luminance, and it is not
  budgeted against the diffuse.** Measured on the corrected build, five yaws, blades removed, share of
  ground luminance that disappears with `FB_GROUND_SPEC=0`: 15–23 % with the sun behind the camera,
  38–44 % looking into it. The cause is the microfacet Fresnel at the half vector: at sun 11.2° and a
  near-horizontal view the half vector sits 83° off the microfacet normal, so Schlick returns F = 0.56,
  and the GGX lobe at α = 0.95 comes out at 0.455 sr⁻¹ against the diffuse's 0.031 sr⁻¹. Two things
  are wrong with it and they pull in opposite directions: the diffuse is reduced by the ENVIRONMENT
  Fresnel (0.016) instead of by `1 − F(v·h)`, which is an energy leak in the specular's favour; and a
  loose particulate soil is not a smooth dielectric interface, which is what F0 = 0.04 with a Schlick
  grazing ramp assumes. Fixing either is a BRDF decision, not a tuning, and it is what stands between
  the measured ground saturation (0.306–0.334) and the material's own (waldboden 0.52, sand 0.50).
- **No sourced value for any residual-light term**, and no epoch wiring for skyglow.
- ~~**The tone curve costs a factor of 3 in the shadows**~~ — the *clip* half is **closed**: the black
  anchor is a knee with a toe under it and no longer a wall (`stages/tonemap.md` `### The toe`), which
  took the demo frame's code-0 share from 1.517 % to 0.002 % and its shown spread from 5.11 to 8.02 EV
  without moving one code of sky or far terrain. The compression the curve applies *between* the
  anchors is untouched and remains a display-mapping question.
- `NvisStage` and `SpritesStage` derived their constants against the superseded ACES fit and are
  **wrong**, not merely undocumented. Both are inactive in the demo scene.
