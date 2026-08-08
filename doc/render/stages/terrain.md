# Tiles — the terrain draw, the ground material and the stand

**Pass:** `TilesStage` (`sim/src/render/stages/TilesStage.{h,cpp}`) — the only stage with real per-frame
CPU state: the growable albedo array, the render bundle, the two-phase-commit tile table and the
mode-strictness invariant counters. It is also the **only** place the ground is drawn, and that is one
fragment doing three jobs: the tile mesh, the material the class selects, and the stand that material
carries (`render/Sward.h` spliced into the fragment, `render/Graticule.h` for the lattice it is hashed
on).

Neighbours: [`../../world/terrain.md`](../../world/terrain.md) (where the tiles come from: streaming,
the worker, the mesh library, `fb-tiles`), [`../renderer.md`](../renderer.md) §6 (which today carries the
implementation detail of this pass), [`../classification.md`](../classification.md) (what decides the
material), [`../vegetation.md`](../vegetation.md) (the template stack whose declaration this fragment
reads), [`../lighting.md`](../lighting.md), [`atmosphere.md`](atmosphere.md) (the air the ground
dissolves into), [`shadow.md`](shadow.md) (which this pass does not cast into, and why),
[`../lod.md`](../lod.md).

## Spec

### The terrain draw

| Contract | Why |
|---|---|
| **ONE render bundle**, per-tile buffers, camera-relative ECEF positions with a double origin per tile | at z14 a tile is < 2 km, so float offsets are sub-centimetre; a global float position is not |
| the **same weather sample** the cloud pass marches | „deck and ground see one atmosphere" becomes a fact of the wiring rather than a claim ([`../clouds.md`](../clouds.md)) |
| unset weather means **100 km visibility, no deck** | a clear standard atmosphere, not „no atmosphere" — a default that is a physical state rather than an absence |
| **two-phase commit** on the albedo upload: the photo layer draws only once its upload is a pass old | a tile that draws before its texture arrives is a visible hole, and „it usually arrives in time" is not a guarantee |
| the ground reads the **LUT sampler**, not the albedo sampler | the sky-view LUT wraps in azimuth with its seam at the sun's own azimuth; the tile sampler would filter across the brightest part of the far field ([`atmosphere.md`](atmosphere.md)) |
| **aerial imagery is not a visual source** — only a coarse albedo hint at DEM resolution | a photograph carries baked lighting, season, shadows and parked cars; it cannot be relit, and time of day and weather are declared per mission ([`../visual-target.md`](../visual-target.md) §4) |
| the detail ladder is `sse_px` on the ground octave wavelength | [`../lod.md`](../lod.md) — and the terrain mesh's own `err` (maximum decimation height error in metres) is already exactly the object-space quantity that rule projects |
| the mesh the DAG starts from must not be coarser than the tolerance the DAG declares | a level-0 cluster carries `SelfErr` = 0, which asserts the drawn surface IS the DEM. It is not: the tile mesh is a decimation and `err` is its own miss. `kEdgeTau / kGrid` (`world/World.cpp`) is that miss in pixels and it must stay within one factor of `SseTauPx` — **4 px against 1 px** since 2026-08-08, twelve against one before. MEASURED while it was twelve: `FB_TAU` 1 → 0.25 moves the silhouette by under 0.05 px, i.e. the cut had nothing left to select ([`../../world/terrain.md`](../../world/terrain.md) §2.2) |

### The ground is a material, not a colour — and it is its own layer

> Owner: *„sand, erde trocken, feucht, kies, waldboden, laub, etc. foliage und clutter sind über dem
> boden shader."*

A ground material is an independent layer that a vegetation template *references*; it is not a field
inside the template. Foliage, clutter and every grown thing sit ON TOP of it and never replace it —
where density goes to zero, what remains visible is still a material, not a flat fill.

**The class list**, and it is a first cut, not a closed set: `sand` · `erde_trocken` · `erde_feucht` ·
`kies` · `schotter` · `fels` · `kalk` · `kalkschutt` · `firn` · `waldboden` · `laubstreu` · `nadelstreu` ·
`grasfilz` · `moos` · `torf` · `schlamm` · `asphalt` · `pflaster` · `beton` · `wasser`.

**Rock, scree and firn are a class, not an omission.** OSM carries `natural=bare_rock`,
`natural=scree` and `natural=glacier` across the whole Alpine arc and the tile server passes all three
through; a landscape above the treeline that has no rock class draws heath on a limestone wall. `fels`
is the Weserbergland rock — a declared 50/50 of Muschelkalk with red Buntsandstein — and its hue is
wrong for a carbonate massif, so the alpine group is its own three rows and `fels` is left alone.

| Contract | Acceptance / measurement anchor |
|---|---|
| the ground turns to bare rock where **no soil can lie**, not where an elevation says so | the threshold is the winning class's own `slope.plausibleDeg[1]`, the band above it is `alpineLimit.slopeBandDeg`, and the fragment's slope comes from the DRAWN mesh normal — no second slope estimate exists |
| the fallback is a **fraction, never a second class** | a fraction has no boundary; the acceptance is that no horizontal edge crosses a slope that the transition itself created |
| the CPU stand gate and the GPU ground must not disagree about a wall | one expression, `AlpineLimit::BareBySlope`, in C++ and in WGSL; the CPU stencil is the DEM's own z13 post spacing, which measures within 1.9 deg (p90) of z15 |

**What each class declares** — and every one of these is a *procedural* parameter, never a painted
texture ([`../visual-target.md`](../visual-target.md) §2.2 rung 1):

| Field | Why it is not optional | Provenance |
|---|---|---|
| **albedo, linear reflectance** | the physical quantity, NOT the cartographic raster colour. A colour table cannot carry a reflectance ratio | the per-class values live in `sim/assets/world/ground-materials.json` and each must carry its own source there — **this file states no albedo triple.** Brightness and chromaticity are sourced *separately*: the Rec.709 luminance of the triple is `albedoBroadband`, the RGB ratio is a colour and nothing else (`## Knowledge` „The colour of the ground") |
| **`visibleBroadbandRatio`** | `albedoBroadband` is shortwave 0.3–2.5 µm and the renderer is visible-band. Without a per-class ratio the only alternatives are a wrong level or a global gain, and the global gain has already failed once | **measured per class** for eleven of seventeen — the derivation and the full table are in `## Knowledge` „The visible band" |
| **roughness** | wet gravel and dry gravel are the same albedo and a different image | `[SET]` — asserted here, no source found for a roughness-vs-class table |
| **moisture** 0…1 | darkens albedo and lowers roughness on one dial. Wet earth is not a second class, it is `erde` with the dial up. Ties into weather and the epoch | **measured, and the effect size is on record** — see `## Knowledge` „Moisture darkens albedo" |
| **grain size + height amplitude** | drives the procedural normal. A pebble field and a dust road differ in grain, not in hue | **measured** — Idso et al. 1975 separate a raked from a smooth plot of the *same* soil and get an albedo difference of **0.02 (very wet) to 0.04 (drying)** from self-shading alone, at roughness elements of ~1 cm. Surface relief is an independent optical parameter, not a normal-map decoration |
| **detail scale — a LADDER, not a pair** | two octaves 6.7 apart leave the whole decade the eye resolves at 1.70 m empty, and the finer of the two then stands alone in the resolved band as a TONE. MEASURED: peak-to-median power in the 6–70 px band **22 / 46 / 131** at three ground distances, and a spectral slope of **β = 1.05 / 2.21 / 4.92** — β ≈ 5 is one note, not a surface | the ladder is `[SET]` in nothing: it STARTS at the declared coarse scale, STEPS by `sqrt(6.7)` so that the declared fine scale is rung 2 exactly, and STOPS at the class's `grainSizeM`, below which the material has no relief. Every rung is turned against the one above it by the golden fraction of the lattice's own 90° symmetry, so no prefix of the ladder puts two rungs on one orientation |
| **slope and height response** | scree above, soil below; a steep face sheds litter. Ground follows terrain without anyone painting it | `[SET]` |
| **litter overlay** | leaf and needle litter belongs to the ground, not to the tree — and it must match the stand above it. Beech litter under spruce is a defect the `botanist` will call | `[SET]` |

**On the parameter set as a whole.** No published game-engine ground-material schema was located to
compare against (`## Gaps`). What *is* sourced is that the two fields most likely to be dismissed as
decoration — moisture and surface relief — are the two that a field measurement singles out as
independently controlling reflectance. Idso et al. also fix which moisture matters: albedo is linear in
the water content of a layer **less than 0.2 cm thick**, not in bulk soil moisture. A wetness dial that
responds to rain within seconds and dries within minutes is therefore physically right, and a dial tied
to a deep-soil reservoir is wrong.

**Two consequences worth stating.** The OSM raster colour keeps exactly one job — it stays the *index*
that selects the class ([`../classification.md`](../classification.md)), and stops being the thing that is
drawn. And a ground material is resolution-independent, which is what makes the 2.93 m/texel raster
survivable at 1.70 m: the class is coarse, the surface is generated.

### What `roughness` means, and where it becomes an image

The material table declares `roughness` and no BRDF. This file fixes the reading, because two different
readings give two different pictures:

| Decision | Value | Why |
|---|---|---|
| BRDF | GGX / Smith height-correlated, `F0 = 0.04` | every class here is a dielectric; the file carries no metalness |
| **alpha = roughness**, NOT roughness² | direct | the file's own origins read the field perceptually (`0.90` = *"loose fine grains scatter near-Lambertian"*, `0.05` = water). Under alpha = r² the 0.65 of asphalt becomes a 0.42 lobe, which at the reference scene's 11.2° sun puts a **6× diffuse** glare over the whole foreground (measured); alpha = r gives **2.2×** and still leaves water a mirror |
| environment specular | the sky-view LUT sampled in the **MIRROR DIRECTION** `reflect(v, n)`, clamped to the local horizon, blended toward the uniform-dome radiance `E_sky/pi` on the roughness, all times the **split-sum environment BRDF** (Lazarov's analytic (A, B) fit, SIGGRAPH 2013, as published by Karis for mobile UE4; fed `sqrt(roughness)` because the fit assumes α = r² and this shader defines α = r) | this is the ONLY reason water reads as water: its diffuse albedo is 0.060. The dome MEAN is one number for every pointing, so a river seen at 88.8° incidence — which returns the sky 1.2° above the horizon, warm and several times the zenith's luminance — got painted with the zenith's hue instead. MEASURED before: the Weser carried the dome irradiance's own hue to within 4–24° across three yaws and sat 1.02–1.37 EV under the sky it mirrors. The blend on roughness is `[SET]` in its SHAPE and derived at both ENDS: a mirror returns one direction, a hemispherical lobe returns the mean, and a single-mip environment cannot honestly interpolate between them |
| **specular STRENGTH follows `surface` and `moisture`** | `specularScale = surface == 'coherent' ? 1 : smoothstep(0.05, 0.85, moisture)` | Cook-Torrance assumes a **continuous dielectric interface**. Sand, soil, litter, gravel and peat have none — the "surface" is a heap of separate grains and light is multiply scattered between them, which is why planetary photometry built **Hapke** for regolith instead of a microfacet BRDF. A water film IS such an interface: wet earth glistens, dry earth does not. The split is **loose particulate vs. solid**, NOT ground vs. rest — `fels`, `asphalt`, `pflaster`, `beton` keep scale 1. The edges are the table's own dry/wet pair (`erde_trocken` 0.05, `erde_feucht` 0.85), so there is no free parameter. Roughness alone cannot do this: at an 11.2° sun and a near-horizontal view the half-vector stands 83° off the normal, where even α = 0.95 puts 0.455 sr⁻¹ into the lobe against 0.031 diffuse |
| diffuse | ambient scaled by `(1 - F_env · specularScale)`, direct by `(1 - F(v·h) · specularScale)` | energy split; without it a grazing surface returns more than it receives. What stood here damped the DIRECT diffuse with the AMBIENT Fresnel (0.016) while the specular took `F(v·h)` = 0.54 — a leak of 0.52 of the beam in the reference geometry. Because the same scale is on both sides, `specularScale = 0` takes **nothing** out of the diffuse and dry soil is fully Lambertian rather than merely dark |
| unresolved relief | folds into roughness (Toksvig) | an octave the pixel cannot resolve does not vanish, it becomes variance. Without it the far field sparkles |

**The relief is extrapolated, and that is the weakest link in the chain.** The table's
`heightAmplitudeM` is a GRAIN-scale RMS (0.8 mm on asphalt, 4 mm on forest floor) — three orders under
what the eye resolves at 1.70 m. The bridge is `amp(L) = amp_grain · (L/grain)^H` with **H = 0.8
`[SET]`**, and it is bracketed rather than guessed: `H = 1` makes the slope scale-invariant at the
grain's own aspect ratio (14° everywhere on dry earth), `H = 0.5` puts 8 mm of relief on half a metre
of soil and renders flat. No measurement of the exponent was found.

**H fixes the spectrum, and therefore fixes what „flat enough" can mean.** Amplitude `∝ L^H` is power
`∝ L^2H`, i.e. a 2D power spectrum `P(f) ∝ f^(−1.6)` at H = 0.8. Over the 6–70 px band that is a factor
`(70/√(6·70))^1.6 = 6.3` between the top bin and the median bin **for a perfect power law with no
defect at all**. A peak-to-median target under 5 is therefore not a statement about grids; it is a
statement about H, and it cannot be met without contradicting `heightAmplitudeModel`. What separates a
grid from a surface is the residual over a fitted power law, and that is the number this file quotes
next to the ratio.

### The stand IS the ground fragment, and there is only one scale

> Owner, 2026-08-07: *„ich denke der bodenshader und grasshader müssen sich auch 'kennen'. wenn
> geometrie nicht mehr nötig ist, wird sie zur fragmentfarbe des bodens."*

Everything below the size of a tree is a **fragment term** and never geometry
([`../../goal.md`](../../goal.md): *„nothing below tree size moves"*, and grass is last or never). What that
buys is not a saving, it is the removal of a problem: there is no near scale and no far scale, so there
is no hand-off to make invisible and no edge to hide.

| Contract | Why |
|---|---|
| **ONE declaration, and it is `render/Sward.h`** | LAI and density, the leaf-angle population `G(el)`, the two transmittances, the forward transmission lobe, the tip-height profile and the beam's path through the stand are stated once. `TilesStage` splices it and does not own it |
| the aggregate is the **mean of a transmittance, never the transmittance of a mean** | `⟨T_x⟩ = kv/(kx+kv) · (1 − e^−(kx+kv)L) / (1 − e^−kv L)`, closed for the exponential profile. Jensen's inequality points the wrong way: evaluating the same exponential at the mean depth measured **13–24 display codes too dark** |
| the aggregate's normal is the **ground's**, not the canopy envelope's | a canopy-top normal is a second geometric model over the population `G` already carries. Measured with the tilt in: the aggregate read a flat **11 display codes (0.49 EV) under** the geometry it replaced at every distance from 4 to 20 m, because the envelope's slope at the crown lattice runs past 1.0 and every tilted sample loses sky. What the envelope's shape is allowed to change is the beam's PATH |
| the beam splits between the two faces by **`κ = 1 − γ/π`**, `γ = angle(sun, fragment→camera)` | half and half measured **0.48 EV too dark** in a frame looking into an 11° sun. For a spherical normal distribution the two half-spaces cut a lune of solid angle `2(π − γ)`; `E[\|n·s\|] = G(el)` is conserved, so the split moves energy and never creates it |
| the lattice is the **graticule**, `render/Graticule.h` | a cell index is `floor(degrees-of-arc / kCellM)` — a function of the PLACE and of nothing else. Measured on the reference scene at 1.7 m eye: an eye-centred lattice swung the coverage of a FIXED world strip **0.026 → 0.151 → 0.026** with a period of exactly its own spacing |
| the class is read at **level 0, always** | a mip may filter what is DRAWN; it may not pick what is THERE ([`../../goal.md`](../../goal.md)). A mode-filtered mip made level 3 say meadow and level 4 say field, so walking two metres flipped the material underfoot (owner report 2026-08-06) |
| classes interpolate as **weights, never as an index** | four level-0 texels, four template rows, blended as PARAMETERS — so a meadow/field boundary mixes two swards instead of meeting at a line ([`../../goal.md`](../../goal.md)) |
| litter is a **coverage fraction on the same relief field**, not a second class | it lands in the hollows the relief ladder already carves, which is where litter collects |
| the tussock rung is a pattern above its own footprint and its **mean** below it | an unresolved shadow is a mean darkening, so nothing is lost when the rung retires — the same footprint filter the relief ladder uses |
| `kEpoch` / `kDecay` ride this declaration | a material that must weather from intact to overgrown has to weather the same way at every scale, so the parameter belongs to the one declaration both would read. Both are small INDICES, not dials — three epochs, three decay steps — so a later reading is a shader VARIANT and not a blend |

**A flat far field is accepted, and that is the project's own yardstick.** Owner, 2026-08-07:
*„Fallout 4 und Witcher 3 sehen in der Ferne auch rasiert aus."* The standard is **believability, not
fidelity**, measured against those titles — and the primary sources agree that they end grass hard:
Witcher 3 declares *„various cell sizes and draw distances"* per grass type, SpeedTree *„The far
clipping plane is used to keep the grass's population restricted to a short distance."*

### The stand does not move

> Owner, 2026-08-07: everything below the size of a tree stands still; Fallout 4 was wholly static.

Nothing in this fragment reads a wind. The wind machinery STAYS and is not this stage's:
`render/WindField.h`, the elastica solution (`sim/tools/elastica.py`, the closed form baked as
`kKnee*`/`kCauchyK`) and the wind clock are what a branch and a rotor will read
([`../vegetation.md`](../vegetation.md) `## Knowledge`).

## State

**Built**, landed with the stage split (`c9206eb`…`2099cb0`) and reworked when the GUI became a client
(`9c1854f`). Render bundles, two-phase streaming, the atmosphere spliced from `AtmoHaze.h` verbatim with
the cloud march. The ground material, the class path and the sward aggregate arrived with the ground
rounds of 2026-08-06/07 and are the surviving half of the deleted cover stage.

> **Every figure below is a measurement of the GROUND FRAGMENT** — taken with the blade geometry
> removed, on a forced class, or as a paired frame. The two frame-time rows are the exception and say
> so. **The frame as a whole has not been measured since the cover stage was deleted** (`## Gaps`).

**The alpine rock group is built** (2026-08-08, binary `b46d7330`). Three classes were added and the
table is 20 rows: `kalk` (weathered Wetterstein-/Dachsteinkalk face), `kalkschutt` (its talus) and
`firn` (an OSM glacier polygon, firn and bare ice together). `kalk` and `kalkschutt` take their
brightness through a THIRD provenance path beside A and B — a Munsell NEUTRAL chip through the ASTM
D1535 value quintic — and that path hands over the chromaticity for free, because the neutral page has
none: R:G:B = 1.000:1.000:1.000, which is what a >97 % calcite carries by mineralogy (no electronic
transition in 380…780 nm; the CO3 overtones sit at 1.9, 2.16 and 2.35 µm).

**The level is cross-checked against a photograph and it holds to 0.04 EV.** On the `nebelhorn`
reference frame of 2026-07-28 the sunlit karst plateau (4 169 px at 320×180, selected by saturation
< 0.12 and R ≥ G) has linear-sRGB luminance **0.3151** and the sunlit alpine sward in the same frame
(1 150 px, G > R + 0.02, saturation > 0.10) **0.1318** — ratio **2.39**, and a ratio inside one
photograph carries no exposure. It DOES carry a tone curve, and how much is measured in
[`tonemap.md`](tonemap.md) `## Spec`; over a pair of sunlit ground classes, inside ~2 EV of each
other, the carry is small and this is the one comparison these photographs support. Munsell N6 alone
gives 0.2857, i.e. 0.02 EV from the photometric number: two independent routes inside a twentieth of
a stop, and `kalk`'s level is what that fixes.
**The `wiese` sward's 0.1213 is NOT the second half of that pair and must not be read as one** — it
is a LEAF reflectance, and what the picture shows is the CANOPY built out of it, 0.0486. The full
accounting is in `## Gaps`.

**`kalkschutt` lost its one-chip step in the same round it was written.** It first carried N7 against
`kalk`'s N6 (0.4094 against 0.2857, 0.50 EV) on the argument that a talus cone is renewed faster than
lichen colonises it. The argument stands; the step does not, because the one measurement available
cannot see it. The two classes now differ only in grain (0.064 m against 0.4 m), packing (loose against
sealed) and surface (particulate against coherent), which is what this file says separates classes of
equal reflectance.

**The specular declaration was A/B'd and is worth nothing here.** `kalk` declares `surface: coherent`,
i.e. `SpecularScale` 1.0, on `fels`' argument that a cleaved face has a sheen. Measured on the
`nebelhorn` frame, same binary, only the flag changed: rock display luminance **0.4530** coherent
against **0.4516** particulate — **0.004 EV**. The declaration is kept because it is the right
statement about the surface, not because it is visible.

**The slope fallback is built.** `groundMat` mixes every row parameter toward the declared
`alpineLimit.bareRockTemplate` by `smoothstep(slopeMax, slopeMax + 4°, slope)`, where `slopeMax` is the
winning pair's own `slope.plausibleDeg[1]` carried in `VegRow.edge.w` and the two constants ride in the
uniform's previously unused `sgr.zw`. It costs one `acos`, one `smoothstep` and eight `mix` per ground
fragment, inside the existing pass — the Begin*Pass count per frame is unchanged at **7**.

**The material exists.** `TilesStage` does not draw the raster: the texel is a CLASS INDEX, the class
names a row of `sim/assets/world/ground-materials.json`, and what is drawn is that row's linear
reflectance, its roughness and a surface generated from its grain over a ladder of octaves that runs
from the declared coarse scale down to the grain itself. `vegetation.json`'s `ground` block is
`{class, litterClass, contrast}` — no colour. `World::GroundMaterials::Load` multiplies
`visibleBroadbandRatio` into the albedo triple in the same statement as the moisture dial — a SCALE, so
the separately sourced chromaticity does not move, and applied ONCE where the table is read so no
consumer can apply it twice.

**The raster colour cannot reach a fragment any more, and that is structural rather than a
convention:** `World::ClassifyRaster` resolves the decoded bake to one byte per texel on the CPU and
the bytes are dropped; the GPU array is `R8Uint`. Measured, demo scene, 130 resident tiles:

| | before | after |
|---|---|---|
| `albedoVramMB` (the log's own definition, level 0 only) | **130** | **0** — the RGBA8 array is a 1×1 dummy until EVS is switched on |
| `classVramMB` (512², one byte), measured while the class array still carried a mode-filtered mip chain | — | **43.33** = 130 × 512² × 4/3 |
| real VRAM including those mips | 130 × 1.333 = **173.3 MiB** | **43.3 MiB**, i.e. **−130.0 MiB** |

The mip chain has since gone — **the class is read at level 0 and the class texture has no other
level**, which took `classVramMB` a further 40 → 30 and one `WriteTexture` per tile instead of ten
([`../classification.md`](../classification.md) `## State`, which owns that measurement).

`/bake/osm` is still FETCHED — it is the only wired classification input
([`../classification.md`](../classification.md) `## Gaps`), and the OSM-vector classifier that would
retire it is a later step. What stopped is holding it as a picture.

### The material contrast, measured

Method: one class forced over the whole ground, blades removed, everything else identical — same
camera, same geometry, same irradiance, same pixels. Display luminance is linearised sRGB averaged over
rows 600–700 (3–6 m) and 450–500 (10–25 m), columns 300–900.

| pair | scene reflectance | rendered, yaw 90 (sun off-axis) | rendered, yaw 270 (into the sun) |
|---|---|---|---|
| dry earth vs asphalt | 0.1667 / 0.1200 = **+0.474 EV** | **+0.146 EV** near, +0.104 mid | **−0.027 EV** near, −0.116 mid |
| forest floor (70 % leaf litter) vs asphalt | 0.1674 / 0.1200 = **+0.481 EV** | **+0.074 EV** near, +0.072 mid | −0.049 EV near |
| bare `waldboden` vs asphalt | 0.1278 / 0.1200 = **+0.091 EV** | — | — |

**Two findings, and neither is the one that was expected:**

1. **≥ 1.0 EV between asphalt and a meadow is not reachable from this table**, and the reason is
   arithmetic rather than a shader defect: the pair above is 0.474 EV of reflectance, and the tone curve
   then costs the documented factor ~3 ([`tonemap.md`](tonemap.md)) — 0.474/3 = 0.16 EV, and 0.146 EV is
   measured. A displayed 1.0 EV would need 3 EV of reflectance, i.e. a ground at 0.96. What is green
   about a meadow is the STAND, and the stand is the aggregate term, not the material.
2. **Looking into an 11.2° sun the albedo order inverts.** Asphalt becomes the brightest ground in the
   frame by 0.03–0.12 EV. That is the specular lobe and it is the physics — a road at low sun really is
   the brightest thing in a photograph — but it means a mean-luminance material contrast is only
   defined in the diffuse hemisphere, and it is stated here so it is not measured once and called a
   defect.

**Roughness and relief separate the pair that albedo cannot** — 0.091 EV between bare forest floor and
asphalt. The separator is local structure, measured as RMS contrast over the same near band:

| | asphalt | dry earth | forest floor |
|---|---|---|---|
| near-field RMS contrast, yaw 90 | **0.0333** | 0.0433 | **0.1357** (4.1× asphalt) |
| near-field RMS contrast, yaw 270 | 0.1357 | 0.0663 | 0.2222 (1.6× asphalt) |

The 4.1× comes from `heightPacking` exactly as the albedo report predicted: `sealed` (0.10 × grain)
against `loose` (0.40 × grain), through H = 0.8 into a 3.5× slope ratio. Under the sun the gap narrows
to 1.6× because asphalt's own glint speckles on the same relief.

### The surface is world-fixed

Two frames, same heading, camera moved 0.10 m laterally, blades removed and `waldboden` forced;
normalised cross-correlation of the BAND-passed luminance per row band (row minus a 31 px box — a
3-tap high-pass sees only the dither, because the structure is tens of pixels wide).

| rows | z | predicted `f·s/z` | measured | r at the peak | r at zero shift |
|---|---|---|---|---|---|
| 390–410 | 26.5 m | 2.4 px | **1** | 0.45 | +0.35 |
| 430–460 | 12.5 m | 5.0 px | **4** | 0.67 | +0.13 |
| 500–540 | 6.6 m | 9.4 px | **8** | 0.87 | −0.13 |
| 600–660 | 3.9 m | 15.9 px | **15** | 0.92 | −0.15 |
| 680–719 | 3.1 m | 20.0 px | **20** | 0.90 | −0.13 |

`f_px = 360/tan 30° = 623.5`, flat-ground `z = h·f/(y − y₀)`. A camera-fixed field peaks at 0 px with
r ≈ 1; this one peaks at the predicted shift and is *anti*-correlated at zero in every near band. The
rotation test agrees: four frames, one standpoint, yaw 0/90/180/270, near field rows 600–719 — the
largest correlation between any pair is **0.022**.

The frame is the tile's **z10 ancestor**: `Tile.c = origin − anchor` crosses to the GPU as a float, so
every tile within ~39 km shares one origin and the surface is continuous across their seams, at 2.3 mm
of float resolution against a 0.12 m finest octave.

### Why more than one octave

A hash lattice does not tile, so „one octave repeats visibly" is not a statement about this build. What
IS measurable is that one octave leaves the band between the pixel and its own period empty. One octave
against two, forced `waldboden`, energy of the luminance band-passed at 9 px:

| ground distance | coarse octave (1.6 m) | fine octave (0.239 m) | 1 octave | 2 octaves | factor |
|---|---|---|---|---|---|
| 3.1 m | 353 px | 53 px | 0.00033 | 0.00119 | **3.6×** |
| 3.9 m | 280 px | 42 px | 0.00052 | 0.00187 | **3.6×** |
| 6.6 m | 166 px | 25 px | 0.00077 | 0.00541 | **7.1×** |
| 12.5 m | 88 px | 13 px | 0.00269 | 0.02662 | **9.9×** |

At 1.70 m eye height the coarse octave is **280–353 px** across the first four metres, i.e. under four
periods in a 1280 px frame; one octave alone is soft dunes with nothing on them. The second octave is
what puts structure in the 9 px band, by 3.6× at the walker's feet and 9.9× at 12 m.

### Cost of the ground fragment, and the continuity probe

**Measured on the build that still carried the blade geometry**, 1280×720, best of 3 × mean-of-4, the
ground material switched in against the raw raster:

| | before | after |
|---|---|---|
| frame | 5.270 ms | **5.916 ms** (+0.65 ms, +12 %) |
| terrain / building / shadow tris | 51 054 / 17 024 / 8 550 | unchanged |
| draws · terrain draw calls | 130 · 120 | unchanged |
| Begin\*Pass per frame | 7 | unchanged — no pass added, split or removed |
| `albedoVramMB` | 130 | **0**, plus `classVramMB` 43.33 |

The +0.65 ms is the ground fragment: two noise lookups with ANALYTIC gradients (central differences
need three per octave and measured 0.30 ms dearer), four class `textureLoad`s and one GGX lobe.
Switching the specular off costs nothing measurable, so the noise and the loads are the whole bill.

**The ground-profile probe of [`../lod.md`](../lod.md) improves by 2.7×.** Probe: mean absolute 3-tap
horizontal high-pass per row, normalised by the row mean, in 4-row bands over columns 200–1080, ratio
between adjacent depth samples, d = 3.0…133 m.

| | worst adjacent ratio |
|---|---|
| before the ground material | **4.65 at d = 88 m** (then 4.12 @ 44 m, 3.93 @ 133 m) |
| after | **1.72 at d = 33 m** (then 1.58 @ 29 m, 1.30 @ 27 m) |

That is under the `[SET]` gate of ~2, and it confirms `lod.md`'s attribution: the discontinuity was the
ground albedo, not the cluster DAG.

### The blocker this material was built against

`kMaxZ = 14` is **2.93 m per texel** at Hameln's latitude, so from an eye height of 1.70 m **one texel
fills the lower third of the frame** — measured on the R1 walk frames, constant `#d2c6b8` from row ~380
down. Higher zoom is **not** the answer: `fb-tiles` answers 404 for z16/17/18, and imagery is ruled out
as a visual source by the Spec row above. The generated material is resolution-independent by
construction and is what closes it.

**The `vegTable` binding may be null**, and then the material branch is a baked-const OFF, the pipeline
layout has two fewer slots and the whole branch dead-strips into one neutral material.

## Gaps

- **THE ROCK/SWARD SPREAD IS ACCOUNTED FOR END TO END, and the seat of it is the SWARD, not the rock,
  not the light and not the curve.** Measured on `nebelhorn` at 320×180 with binary md5
  `b46d733028f8d4b43d4a6547ba9c44a2`, rock and sward masks frozen on the reference frame (a
  colour-keyed population moves with the light and is not a ruler; the earlier 9.21/2.39 pair was
  taken with a looser key that also admitted sky and haze — the same measurement with `R ≥ B` on rock
  and `G ≥ B` on sward reads **8.08** render against **2.50** photograph, i.e. **1.69 EV** of excess):

  | Step | EV | How |
  |---|---|---|
  | declared pair, `kalk` 0.2823 against the `wiese` sward's `colIn` 0.1157 | 1.287 | the two declarations |
  | **the sward aggregate turns a LEAF reflectance into a CANOPY reflectance** | **1.153** | measured, `FB_TONE_PROBE=-16,4`: sward population mean scene radiance **0.1560** against the flat-sunlit Lambertian **0.3469** at `colIn` |
  | `kSelfShelter`'s near bounce, which is `alb²` and therefore albedo-asymmetric | 0.088 | derived from the equation of [`../lighting.md`](../lighting.md) §2 |
  | = the SCENE ratio | **2.528** | measured: probe, rock **0.8999** / sward **0.1560** = 5.769 |
  | the ACES toe, local gamma **1.561** at the sward against **0.780** at the rock | 0.486 | measured: display 3.014 − scene 2.528, and the two gammas are `d log filmic/d log x` ([`tonemap.md`](tonemap.md) `## State`) |
  | = the DISPLAY ratio | **3.014** | measured: 8.08 |
  | photograph | 1.322 | measured: 2.50 |
  | **unaccounted** | **0.00** | |

  **The rock is exactly where the equation puts it**: probe scene radiance **0.8999** against the
  derived flat-sunlit Lambertian `3.5014 · 0.2823 · (0.82206 + 0.29434 · 0.2823)` = **0.8947**, i.e.
  **0.008 EV**. All of the excess is on the sward side, and the paired control says so directly —
  with `swardAggregate` removed so that the `grasfilz` floor draws as a Lambertian at its own 0.1022,
  the same frozen masks read **2.56** against the photograph's **2.50** (control binary md5
  `cd13bde1ecef60dffa383423c304c44b`, frame `sim/build/out/spread-nebelhorn-swardoff.png`).
  **That control is a diagnosis and NOT a candidate**: the canopy model is not wrong. Its delivered
  reflectance, 0.0486 against `colIn` 0.1157, sits **0.07 EV** from the two-stream semi-infinite
  canopy albedo `(1 − √(1 − ω))/(1 + √(1 − ω))` = 0.0511 at the ω = 0.185 `render/Sward.h` already
  declares in `kScatCut`. A closed green grass canopy has a visible-band luminance factor near 0.05,
  and this one has it.
  **What IS wrong is which stand stands there.** `vegetation.json` declares `osmDefault: wiese`, so
  every unmapped polygon on Earth — including the alpine turf above the treeline at `nebelhorn`,
  2 000…2 200 m — is a 0.30 m mown lowland hay meadow at 800 blades/m², LAI **4.64**, dry fraction
  0.30. `natural=fell` is in no template's `osm` list at all. The next round is the alpine sward: a
  short, half-senescent turf over a stony floor is a different LAI, a different dry share and a floor
  that shows, and every one of those three raises the delivered reflectance for a reason that is not
  the picture. Until it exists, the alpine karst reads as snow — not because the rock is wrong, but
  because its neighbour is 1.15 EV under what a turf returns.
- **`kalk`'s and `kalkschutt`'s `visibleBroadbandRatio` is a stated 1.000 no-op**, the posture `wasser`
  already takes. The sign is known — calcite's only shortwave absorptions are the three CO3 overtones
  beyond 1.8 µm, so the true ratio is slightly ABOVE 1.000 — and the size is not. ECOSTRESS carries
  `rock.sedimentary.limestone.coarse` as crushed particulate only, and that spectrum is already spent
  inside `fels`' mix.
- **`firn` is the weakest of the three and says so in three places.** Its `albedoBroadband` 0.49 is a
  [SET] 50/50 of Cuffey & Paterson's measured firn (0.43…0.69) and clean-ice (0.34…0.51) midpoints,
  i.e. an accumulation-area ratio of one half; its chromaticity 1.000:1.070:1.150 is [SET] inside a
  derived bracket (fine firn 1.000:1.011:1.027, bare ice 1.000:1.15:1.30) read off published curves at
  ±0.03 per channel; its band ratio 1.46 is a three-band integral with [SET] band reflectances at
  ±0.1. Debris cover is not modelled and its sign is known: it only lowers the number.
- **One lithology for the whole planet.** `alpineLimit.bareRockTemplate` names `felsflur`, i.e.
  limestone, everywhere. Granite, basalt and sandstone each carry their own chromaticity and the engine
  has no lithology map. It is one declared name so a mod can restate it, and that is the whole of the
  mitigation.
- **The relief ladder cannot reach the mid field, by construction, and rock is where that hurts.**
  `octWeight` retires an octave whose period is under four pixels; at 2 km with a 63.55° frame at
  320 px one pixel is 13.8 m of ground, so an octave needs a 55 m period to survive and `kalk`'s
  coarsest is 7.0 m. The amplitude law is not the problem — a self-affine rock surface at H = 0.8
  really does carry only ~2 m of relief over 55 m, which is 2°. What the photograph shows at that range
  is buttress-and-gully geometry at 10…100 m of amplitude, and that is the DEM's, not the material's.

- **`## State` LOST ~466 MEASURED LINES ON 2026-08-07 AND THEY ARE NOT COMING BACK.** A tidy-up script
  editing the file this section was migrated from ran past its intended end and deleted that file's
  `## State` from its heading to „The translation test". The tree is untracked, so there is no Git blob,
  no copy and no snapshot. **Nothing is reconstructed from memory — a remembered measurement is not
  one.** What the lost lines said about this pass has to be MEASURED AGAIN, and what stands above is
  only the part that survived.
- **REJECTED, with the measurement: blade geometry for the 0.1–0.8 m layer.** Deleted on 2026-08-07
  (`render/stages/GroundCoverStage.{h,cpp}`, `render/CoverGrid.h`, and the CPU ground field in
  `world/World.cpp`). It cost **10.3 ms of a 17.0 ms GPU frame — 61 %** for a band ten metres deep, and
  [`../../goal.md`](../../goal.md) puts grass last or never. Three further measured failures of that form,
  kept because each is a currently true statement about what does not work:
  - **the transition at 3–7 m missed the bar.** Row means across the hand-off never met
    \|Δ(R−G)\| < 6 and \|ΔL\| < 5 ([`../vegetation.md`](../vegetation.md) `## State` carries the
    distance-binned figures at the fade radius).
  - **the stand's self-shadow stalled far below the target** — see the next entry, which is still open
    because the aggregate inherits it.
  - **the aggregate form is not ours.** Bruneton & Neyret's three-pixel switch criterion guards against
    *„popping on the terrain silhouettes"* — a TEMPORAL artefact that arises only when geometry WITH
    thickness switches to an aggregate WITHOUT it. A far field that consistently has no thickness has
    nothing to pop, which is why deleting the near scale removed the criterion's subject rather than
    violating it.
- **THE STAND STILL CASTS NO SHADOW ON ITSELF WORTH THE NAME.** The beam's slant path is integrated
  along the sun over the canopy-top field, and on the nadir bench (two sun azimuths 205° apart, 80 px
  tiles) it moves the contrast by nothing measurable: p95/p05 **0.326 / 0.402 EV** for the marched form
  against **0.328 / 0.439 EV** for the bearing-free column, where the critic's target is **1.74 EV**.
  The march buys STRUCTURE — without it the far meadow renders as one flat green wash, and the column
  form has no sun bearing in it at all — but it does not buy contrast, and nothing in the aggregate
  does yet.
- **The macro-contrast acceptance number carries no method.** The critic's baseline is p95/p05 = 1.341
  = 0.42 EV with neither the tile count nor the light stated, so it is not known to be the same
  statistic any bench reports. Until that is settled the *change* is the only safe reading of it.
- **The frame has not been measured since the cover stage was deleted**, and the two frame-time rows in
  `## State` are explicitly labelled as predating it. Until a distribution over a moving camera exists
  ([`../../goal.md`](../../goal.md) „Measurement"), this pass has no current cost figure.
- **Three controls lost their switch and kept their constant**: the forced class (`kForceCls`, baked
  −1), the octave cap (`kOctN`, from `kReliefOctaves`) and the direct specular (`kSpecOn`, baked 1.0).
  Every figure in `## State` that rests on one of them is a measurement of a shader that has not
  changed since, but repeating it now costs a recompile rather than an environment variable.
  `FB_GROUND_CLASS_VIZ` is the only diagnostic switch this pass still has.
- **Water is 0.80–1.32 EV under the sky it mirrors, and `wasser.roughness = 0.05` is why.** With the
  direction fixed, the remaining deficit is entirely the split-sum integral: 0.534 at 88.8° incidence
  against a flat mirror's 0.905 (both measured). The table's own origin calls the field a placeholder
  for a water shader that does not exist. Two things are missing before it can move: a wave slope model
  (Cox-Munk gives `σ² = 0.003 + 0.00512·U`, i.e. `α ≈ 0.26` at this scene's 6 m/s wind — four times
  ROUGHER than the table, which would make the deficit worse, not better) and a specular that samples
  the lobe rather than one direction. Neither is a tuning.
- **The environment lobe is one tap and a lerp, not a prefiltered chain.** Between `reflect(v, n)` and
  the dome mean the blend is linear in roughness. Both ends are right and nothing in between is
  measured; what a prefiltered mip chain of the sky-view LUT would cost, and whether it changes any
  class but `wasser` and `asphalt`, is unmeasured.
- **The Lazarov split-sum fit is 4.6× low at grazing for ROUGH surfaces.** Measured against a 200 k
  Monte-Carlo integration of the same GGX/Smith BRDF at `N·V = 0.0202`: `α = 0.35` truth 0.144 / fit
  0.083, `α = 0.65` truth 0.081 / fit 0.018, `α = 0.90` truth 0.062 / fit 0.008. At `α = 0.05` — the
  only place it decides anything today — the fit is 7 % low and harmless. Correcting it would BRIGHTEN
  asphalt, mud and rock at grazing, which is a look decision and was not taken.
- **Peak/median under 5 in the 6–70 px band is unreachable at H = 0.8**, and 6.63–10.79 is delivered
  against a power-law floor of 6.3. Whether the real exponent is 0.8 is the open question underneath
  it — `heightAmplitudeModel` has no measurement for H, and lowering it would flatten the spectrum and
  the peak together.
- **The relief exponent H = 0.8 is `[SET]` and load-bearing.** It alone converts a grain-scale
  amplitude into the relief the eye sees, so it alone decides how strongly the whole surface reads.
  Bracketed in `## Spec`, unmeasured.
- **A coherent surface does not get wetter.** `specularScale` is 1 for `asphalt`, `pflaster`, `beton`,
  `fels` and `wasser` whatever their `moisture`, because what rain does to a sealed skin is fill its
  microrelief — that is the ROUGHNESS field, and this table has one entry per material, not one per
  weather state. So a wet road is exactly as glossy as a dry one here. The mechanism the class model
  already names ([`../classification.md`](../classification.md): weather changes the STATE of a class,
  never the class) has no producer for roughness yet.
- **Oren-Nayar was not built and not measured.** A particulate medium with `specularScale = 0` is drawn
  as pure Lambert, which is the wrong azimuthal shape for a rough diffuser at an 11.2° sun. It was left
  out because the decision was "this material has barely any specular", not "add a second BRDF"; whether
  it changes the image at this sun angle is unmeasured, and it is the first thing to measure if the
  ground still reads flat.
- **`moisture` is applied once at load time and never moves, and it has no producer.** The dial exists
  per class with a measured effect size and a measured time constant; weather has no producer, nothing
  recomputes an albedo when it rains, and the factor is baked into the uploaded row. The weather sample
  this pass already reads is the obvious source and is not wired to it.
- **The class the shader draws still comes from the leaf that is drawn.** It is cheap enough to bound
  and too expensive to fix by binding every level (11 776 array layers). MEASURED with
  `FB_GROUND_CLASS_VIZ=1`, the walking frame against a converged standing frame at the same endpoint:
  **0 pixels of 921 600** differ after 1 400 passes at 1.0/0.7 m per pass, and **20 540 (2.23 %)** after
  1 400 passes at 16/11.2 m — i.e. the drawn class lags only when the eye outruns the streamer, at
  960 m/s. At a cold standing start it is **0.16 %** of the frame at pass 12 and **0.05 %** at pass 20.
  What makes it survivable rather than right is a second measurement: the two classes that actually
  flip resolve to `waldboden` 0.1674 and `erde_trocken` 0.1667 albedo, so 77 % of the flips are
  invisible by construction; the visible remainder is `versiegelt`, i.e. a road appearing. A class
  overlay bound only for the near disc would fix it and is REFUSED — a class that is exact near the
  camera and filtered far away is a class that depends on the viewer.
- **In photo mode the class is classified out of the PHOTOGRAPH.** `World::Update` pulls `/bake/photo`
  when `DefaultPhoto` is set and runs the same 32³ cartographic LUT over it, so every class the shader
  draws in EVS mode is a colour match against an orthophoto. Unmeasured; the demo scene is OSM mode.
- **Past 75.5° latitude the graticule cell stops shrinking.** `Graticule::kMinCosLat = 0.25` clamps the
  east edge, so beyond that the lattice stretches in ground metres instead of renumbering. Never
  rendered.
- **Five of the seventeen band ratios are `[SET]`.** `erde_feucht` and `schlamm` carry `erde_trocken`'s
  0.566 unchanged because no spectrum of a *wet* soil was found — the direction of that error is known
  (water absorbs at 1.45 and 1.95 µm, so the true ratio is **above** 0.566, not below) and the magnitude
  is not. `moos` 0.37 and `torf` 0.44 are midpoints of named measured brackets (grass 0.257 / lichen
  0.474, and black loam 0.375 / forest floor 0.502). `wasser` 1.000 is a stated no-op whose sign is
  known and whose size is not.
- **Three chromaticities are `[SET]`**, and each is a hole in one library: ECOSTRESS carries **no moss
  spectrum** (`moos`), **no peat spectrum** (`torf`) and **no liquid-water spectrum** (`wasser` — snow,
  frost and ice only). `moos` is the worst of the three because it is the only green mineral-side class
  in the file and the whole hue is asserted; the nearest measured neighbours are green grass at hue
  78.9° and dry lichen at 49.1°, and the `[SET]` value sits at **97.4°**, i.e. outside that bracket on
  the green side.
- **No Central European Munsell notation for a loess Braunerde was found**, and the two soil sources
  that were found disagree about chroma. Idso 1975 gives **10YR 5/3** dry for a cultivated Avondale loam
  at Phoenix; the ECOSTRESS Braunerde analogue 88P2535 (a humid-temperate `Dystrochrept`) measures
  **10YR 4.7/6**. Same hue page, chroma 3 against 6, i.e. HSV saturation 0.62 against 0.94 — the widest
  open question in the table. `erde_trocken` takes the Idso chip because it is a *field* soil under
  cultivation and the more conservative of the two. Searched without a hit: German state soil surveys
  (LGRB Baden-Württemberg, LfL Bayern) publish KA5 colour *names*, not Munsell codes; OpenAlex full-text
  search on German loess/Braunerde plus Munsell returned no open-access profile description.
- **Every rock chromaticity is measured on crushed particulate, not on a face.** `fels` draws a cleaved
  outcrop and the only VNIR-covered limestone and sandstone spectra in ECOSTRESS are the `coarse` and
  `fine` particulate splits; the `solid` samples of those lithologies are USGS/JPL thermal-infrared
  only. Particle size moves lightness more than hue, and the level is not taken from these spectra — but
  the hue shift is unquantified.
- **Four mixing weights are `[SET]`.** `kies` is equal thirds quartzite/limestone/sandstone, `fels` is
  50/50 limestone/red sandstone; no clast count of Weser gravel and no outcrop area ratio was found.
  `schotter` and `pflaster` are also 50/50 but that is not a new assertion — each reuses the *same*
  weighting its `albedoBroadband` already declares, so brightness and colour cannot drift apart.
- **`beton`'s chromaticity rests on ONE sample** and it moved a long way: from a `[SET]` near-neutral
  1.00:0.99:0.95 to a measured 1.000:0.801:0.583, HSV saturation 0.22 encoded. Grey portland concrete
  takes its colour from its aggregate and sand, so one sample cannot speak for a class. A second
  measurement would either confirm that saturation or halve it. `sand` and `asphalt` are single samples
  too, but they moved little.
- **The class assignment leaves land cover nearly invisible on the material layer.** `acker` and
  `siedlung` both resolve to `erde_trocken` (0.1667) and `laubmischwald`/`nadelwald` to `waldboden`
  under 70 % litter (0.1674); only `wiese` was separated, by the `grasfilz` row, whose
  `albedoBroadband` 0.20 is `[SET]`. What tells a meadow from a field is therefore the aggregate term
  and not the material. Stated so it is not read as a classification failure: the classifier is right
  and the separation lives one layer up.
- **≥ 1.0 EV of DISPLAYED material contrast is unreachable**, arithmetic in `## State`. After the tone
  curve's documented factor ~3 the band this table's asphalt/earth pair can carry is about 0.16 EV.
  Either the target is scene-referred — then 0.474 EV is the number — or the tone curve owes the factor
  ([`tonemap.md`](tonemap.md) `## Gaps`).
- **Looking into a low sun the albedo order inverts** — asphalt reads 0.03–0.12 EV brighter than earth
  or forest floor at yaw 270. It is the specular lobe and it is physics, but the DIRECT half of that
  lobe is still 38–44 % of the ground's display luminance into the sun and 15–23 % away from it
  (measured 2026-08-06, five yaws, the direct-specular control as the paired control), and that is the
  one number standing between the rendered ground saturation (0.306–0.334) and the material table's own
  (waldboden 0.52, sand 0.50). The environment half of the same lobe was an outright energy error and
  is fixed (`## Spec`); the direct half is a BRDF decision and is named in
  [`../lighting.md`](../lighting.md) `## Gaps`, not here.
- **No published ground-material parameter schema was found** to check the field list against. Searched
  for Frostbite/Far Cry/Horizon terrain-material presentations without reaching a primary document. The
  list is `[SET]` except for moisture and grain, which carry a field measurement.
- **The tiling-repetition threshold has no source, and the claim was REPLACED rather than proved.** A
  procedural hash lattice does not tile, so „one octave repeats visibly" is not a statement about this
  build; what was measured instead is in `## State`. The original question — at what tile frequency a
  repeating TEXTURE becomes visible — is unanswered and moot for a pass that uses no texture.
- **Terrain casts no shadow** ([`shadow.md`](shadow.md)): the tile draw is a render bundle of hundreds of
  per-tile buffers with no per-cascade cull.
- **No screen-space-error LOD on the mesh itself beyond the cluster DAG, and no geomorph.**
  [`../lod.md`](../lod.md) names geomorphing as mechanism 3, the terrain answer (Hoppe 1998), and the
  tile mesh has the topology for it. Nothing implements it, so a level change is a swap.
- **[`../renderer.md`](../renderer.md) §6 owns the implementation detail** — vertex layout, the frame
  uniform, the fragment shader's light and air terms. That section should live here; the split that
  created this file left the renderer document out of scope. **Migrating §6 is outstanding work.**

## Knowledge

The vertex layout, the frame uniform, the deck-attenuation and haze formulas and their derivations are
stated **once**, in [`../renderer.md`](../renderer.md) §6 and [`../clouds.md`](../clouds.md); the tiling,
projection and streaming derivations are in [`../../world/terrain.md`](../../world/terrain.md).

### Moisture darkens albedo — the measurement, and what it constrains

**Source:** Idso, Jackson, Reginato, Kimball & Nakayama, *The Dependence of Bare Soil Albedo on Soil
Water Content*, Journal of Applied Meteorology **14**(1), Feb 1975, 109–113,
DOI [10.1175/1520-0450(1975)014<0109:TDOBSA>2.0.CO;2](https://doi.org/10.1175/1520-0450(1975)014%3C0109:TDOBSA%3E2.0.CO;2).
Four field experiments (May, July, September, December 1973), Avondale loam, Phoenix AZ, 72 × 90 m plot
irrigated to ~10 cm and allowed to dry; albedo from paired Eppley pyranometers every 20 min, water
content sampled gravimetrically in 0–0.2, 0–0.5, 0–1, 1–2, 2–4, 4–6, 6–8, 8–10 cm increments.

| Result | Value |
|---|---|
| zenith-normalised albedo, **wet** | **0.14** |
| zenith-normalised albedo, **dry** | *„approaches an upper limit somewhere just above"* **0.30** (Fig. 2 dry curve runs 0.30→0.35 over 0–70° zenith) |
| **wet / dry ratio** | **0.14 / 0.31 ≈ 0.45** |
| linear range | albedo is linear in the volumetric water content of the **0–0.2 cm** layer over **0.00 … 0.18**, then saturates |
| roughness effect | raked plot (~1 cm relief) vs. smooth, same soil: **−0.02** when very wet, **−0.04** once drying, attributed to self-shading |
| season | *„For soil depths on the order of 2 cm, the albedo-soil water content relationship appears to be independent of season"* |
| Munsell | 10YR 5/3 dry, 10YR 3/3 wet |

**Three things this settles for the material:**

1. **„Wet is about half of dry" is correct** for this soil — 0.45× — and the owner's estimate stands.
   The authors caution it is soil-specific: *„Differences in specifics such as initial and final absolute
   magnitudes, however, will need to be determined for each individual soil type and surface condition."*
   So the *ratio* is a defensible default and the *endpoints* are per-class data.
2. **The moisture that matters is a surface film, not a reservoir.** Linearity holds against the top
   2 mm. Their Fig. 6 shows the curve turning step-like as the averaging depth grows — deep moisture is
   the wrong input. A wetness dial with a seconds-to-minutes time constant is physically right.
3. **Surface relief is an independent optical parameter.** 1 cm of roughness moves albedo by 0.02–0.04
   at constant colour and constant moisture. The „grain size + height amplitude" field is therefore
   not decoration, and it is the same magnitude as the material contrasts that are missing today.

### The colour of the ground — two paths to one chromaticity

The material table splits an albedo triple into a **brightness** and a **chromaticity** and sources them
separately. The brightness is `albedoBroadband`; the chromaticity is the RGB *ratio*, renormalised so
the Rec.709 luminance equals that broadband figure exactly. A chromaticity source therefore only has to
get the *colour* right, never the level — which is what makes a laboratory spectrum of a prepared sample
admissible here when it would not be admissible for the albedo itself.

**Path A — Munsell → linear sRGB.** Soil science publishes colour in Munsell, so the table takes it that
way where a notation exists:

| Step | Data | Note |
|---|---|---|
| Munsell (H V C) → *x,y* | `real.dat`, Munsell renotation [25] | RIT MCSL: *„calculated using illuminant C and the CIE 1931 2 degree observer"*, and `real.dat` is *„those colors listed the original 1943 renotation article (Newhall, Judd, and Nickerson, JOSA, 1943)"* |
| odd chroma | linear interpolation between tabulated even chromas at the same hue page and value | checked against radial interpolation about the illuminant C white point (0.31006, 0.31616): the two agree to **0.0004 in xy**, under the table's own four-decimal precision |
| *x,y* → XYZ at Y = 1 | — | **the renotation's Y column is discarded**, which is exactly why RIT's *„multiply the Y values … by 0.975, which is Ymgo"* caveat does not touch this file |
| C → D65 | Bradford, C (0.98074, 1, 1.18232) → D65 (0.95047, 1, 1.08883) | D65 because the render primaries are sRGB/Rec.709 [29] and sRGB's white *is* D65 |
| XYZ → linear RGB | IEC 61966-2-1 matrix [29] | then scaled so `0.2126R + 0.7152G + 0.0722B = albedoBroadband` |

**Path B — measured spectrum → linear sRGB.** Where no notation exists, a directional-hemispherical
reflectance curve from the ECOSTRESS Spectral Library [26] is integrated against the CIE D65 SPD and the
1931 2° colour-matching functions at 1 nm [28]:
`XYZ = k ∫ R(λ)·S_D65(λ)·[x̄,ȳ,z̄](λ) dλ`, then the same sRGB matrix and the same luminance lock.

**The two paths agree, and that is the only end-to-end check this chain has.** ECOSTRESS soil 88P2535,
whose JHU record names it *„Dark yellowish brown micaceous loam"*, converts by path B to
**1.000 : 0.473 : 0.059**. The Munsell chip **10YR 4/6**, whose USDA soil colour *name* is exactly „dark
yellowish brown", converts by path A to **1.000 : 0.466 : 0.070**. Two independent data sets, two
independent conversions, **agreement within 0.011 per channel**.

Two pipeline checks, both exact: path B returns the D65 chromaticity **(0.31273, 0.32902)** for a flat
unit reflector, and path A reproduces RIT's own published sRGB table for the renotation to a maximum of
**0.0038** over the 1 446 in-gamut chips — that file's own rounding.

**Hue is quoted twice, and it has to be.** „Hue" below is the HSV hue of the triple; *linear* is the
triple as stored, *sRGB* is after IEC 61966-2-1 encoding, which is what a measurement taken off a
screenshot sees. The encoded hue runs **0.6–6.2° above** the linear one across the fourteen warm classes
here — least on near-neutral asphalt (+0.6°), most on `waldboden` (+6.2°). Quoting one figure without
saying which space it is in is how a hue argument goes wrong.

### What the sourced colours actually say — the ground is not 40–50°

| class | chromaticity source | hue, linear | hue, sRGB | sat, sRGB |
|---|---|---|---|---|
| `sand` | measured Quartzipsamment 87P706 | 31.9° | **37.7°** | 0.50 |
| `erde_trocken` | **Munsell 10YR 5/3** (Idso [18]) | 28.4° | **32.5°** | 0.37 |
| `erde_feucht` · `schlamm` | **Munsell 10YR 3/3** (Idso [18]) | 25.9° | **31.7°** | 0.48 |
| `waldboden` | measured Haplumbrept ×2 | 29.6° | **35.8°** | 0.52 |
| `laubstreu` | measured *Quercus* litter ×3 | 30.5° | **34.1°** | 0.33 |
| `nadelstreu` | measured pine + fir needles ×6 | 23.7° | **28.2°** | 0.40 |
| `kies` | measured quartzite + limestone + sandstone | 22.7° | 25.4° | 0.26 |
| `fels` | measured limestone + red sandstone | 22.1° | 25.4° | 0.30 |
| `beton` | measured construction concrete | 31.4° | 33.7° | 0.22 |
| `asphalt` | measured paving asphalt | 35.1° | 35.7° | 0.06 |
| — *for comparison* | measured green grass | 78.9° | 75.7° | 0.36 |

**Not one sourced bare-ground chromaticity reaches 40°.** Every soil, litter and rock class lands
between **22° and 38°** encoded, and the Munsell 10YR page — the hue page both independent soil sources
agree on — sits at 32° encoded at value 5, chroma 3. The critic's reference band of **37.5° / 48.7° /
43.6°** was measured on *photographs of the landscape*, and the same critic's own third row says what is
in those photographs: **42.9 % of pixels have green as the strongest channel**. That is vegetation over
soil, and green grass measures **78.9°** here. **The target mark of 40–50° is not a statement about the
ground material and no sourced Munsell reaches it.** Bending one to reach it was refused.

What the sourced values *do* fix is the second row of that table. Saturation was 0.21–0.35 rendered
against 0.47–0.71 in the photographs; the sourced chromaticities put `waldboden` at 0.52,
`sand` at 0.50, `erde_feucht` at 0.48 and `nadelstreu` at 0.40 — inside or at the edge of the reference
band, where the retired `[SET]` values were not. The remaining hue gap is the **aggregate term**, not a
wrong colour: the stand is what is green.

### The visible band — why the correction is per class and not a factor

`albedoBroadband` is a shortwave (0.3–2.5 µm) quantity and the renderer works in the visible band.
The table therefore carries `visibleBroadbandRatio` per class:

```
ratio = [ ∫₄₀₀^₇₀₀ R(λ)G(λ)dλ / ∫₄₀₀^₇₀₀ G(λ)dλ ]  ÷  [ ∫₃₀₀^₂₅₀₀ R(λ)G(λ)dλ / ∫₃₀₀^₂₅₀₀ G(λ)dλ ]
```

`G(λ)` is ASTM G173-03 global tilt (AM1.5G) [27]; `R(λ)` is the **same** ECOSTRESS spectrum that gave
the class its chromaticity, so colour and band ratio can never disagree about which material was
measured. Below the first measured wavelength (400 nm for the JHU samples, 350 nm for the UCSB ones)
`R` is held at its first value; 300–400 nm carries **4.64 %** of the AM1.5G energy over 300–2500 nm
(computed from the same file; the visible 400–700 nm band carries **43.30 %**), and that extrapolation
is the dominant approximation in these numbers.

| class | visible | shortwave | **ratio** |
|---|---|---|---|
| `asphalt` | 0.075 | 0.079 | **0.953** |
| `schotter` (basalt + granite) | 0.156 | 0.158 | **0.986** |
| `pflaster` (concrete + granite) | 0.229 | 0.254 | **0.901** |
| `beton` | 0.247 | 0.288 | **0.856** |
| `kies` | 0.268 | 0.360 | **0.744** |
| `fels` | 0.230 | 0.311 | **0.741** |
| `laubstreu` | 0.253 | 0.387 | **0.654** |
| `sand` | 0.133 | 0.224 | **0.593** |
| `erde_trocken` | 0.159 | 0.281 | **0.566** |
| `nadelstreu` | 0.166 | 0.324 | **0.514** |
| `waldboden` | 0.089 | 0.178 | **0.502** |
| *green grass, for the spread* | 0.063 | 0.246 | **0.257** |

**A global gain cannot be right.** The sourced classes span **0.502 to 0.986**, a factor of 2.0, and
against green vegetation the spread is **3.7×**. Its BLADE half survived the ground half by two rounds
and is retired too: `vegetation.json` now declares linear blade reflectances measured the same way
([`../vegetation.md`](../vegetation.md)). A single `reflectanceGain 0.50` — retired — sat below
every mineral class in this table and above green vegetation, which is why it darkened field and forest
at once. Dark igneous rock is nearly *flat* across the shortwave (0.986) and needs no correction at all;
the humus of a forest floor needs the most of any mineral surface (0.502).

Note what the ratio does **not** cover: it corrects the *band*. It does not correct a laboratory
directional-hemispherical measurement to a field albedo, and it carries no sun-angle dependence.

### Sources

| # | Source |
|---|---|
| 18 | Idso, Jackson, Reginato, Kimball & Nakayama, *The Dependence of Bare Soil Albedo on Soil Water Content*, J. Appl. Meteorol. 14(1), 1975, 109–113 — Fig. 1 caption: *„Munsell color notation supplied for this soil by the Soil Conservation Service is 10YR 5/3 when dry and 10YR 3/3 when wet"* |
| 25 | Newhall, Judd & Nickerson, *Final Report of the O.S.A. Subcommittee on the Spacing of the Munsell Colors*, JOSA 33(7), 1943 — as tabulated by RIT Munsell Color Science Lab, `real.dat` — https://www.rit-mcsl.org/MunsellRenotation/real.dat (illuminant C, CIE 1931 2°) |
| 26 | Meerdink, Hook, Roberts & Abbott, *The ECOSTRESS Spectral Library version 1.0*, Remote Sens. Environ. 230, 2019, 111196; Baldridge, Hook, Grove & Rivera, *The ASTER Spectral Library Version 2.0*, Remote Sens. Environ. 113, 2009, 711–715 — https://speclib.jpl.nasa.gov |
| 27 | ASTM G173-03, *Reference Solar Spectral Irradiance: Air Mass 1.5*, derived from SMARTS 2.9.2 |
| 28 | RIT Munsell Color Science Lab, *all 1 nm data* (CIE D65 SPD, CIE 1931 2° colour-matching functions) — https://www.rit-mcsl.org/UsefulData/all_1nm_data.xls |
| 29 | IEC 61966-2-1, *Default RGB colour space — sRGB* |

The numbering is [`../visual-target.md`](../visual-target.md)'s; a number means the same paper everywhere
in `doc/render/`.
