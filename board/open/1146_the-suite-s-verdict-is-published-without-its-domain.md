Type: bug
Area: render
Tags: oracle, instrument, khronos

**The suite's verdict is published without its domain, and its domain is four per cent of the pixels**

`criteria met` and `cases within the picture bound` are published side by side and read as what the render
suite proves. **Twenty-seven of the thirty-five cases contain no shading at all**, so the second number is
overwhelmingly a **texture and coverage** verdict and only marginally a shading one — and nothing beside it
says so. This tree's own rule is that an instrument's domain is part of its claim, and that a number
stated without its domain decides nothing; this is that rule broken by the suite's headline figure.

## The enumeration, exhaustive, by two instruments that agree

**Instrument A — the declaration.** Every `manifest.json` under `test/render/*/*/` read for
`scene.light.kind` and `scene.material.kind`. **Instrument B — the measurement.** `outshine.normal.raw`
in every case directory, counting fragments with a non-zero shading normal; the emitted arms write a
declared zero vector, so the count is the number of fragments that entered `shadeRow`. **35 of 35 case
directories carry the file** — nothing is missing and nothing is inferred from absence.

| | cases | what both sides compute |
|---|---|---|
| **oracle lowered to an emitter** (`emission`, `emission-per-material`) | **17** | `declaredRadiance x baseColour(u, v)`, on both sides, and nothing else |
| **oracle is a Diffuse BSDF under the factory world**, ours is the emitted arm | **10** | a cosine-weighted environment integral against a declared constant — equal only where nothing occludes the hemisphere |
| **oracle is metal-rough under a declared light** | **8** | the BRDF, on both sides |

**The two instruments name the same eight**: `lighting/directional-light` 131 908 shaded px ·
`lighting/point-light-intensity` 381 024 · `materials/normal-tangent-mirror` 374 568 ·
`materials/normal-tangent` 294 876 · `materials/water-bottle` 47 275 · `materials/boom-box` 44 949 ·
`materials/corset` 35 737 · `materials/lantern` 17 665. Every other case reports **0**.

**1 328 002 shaded fragments of 31 852 800 rendered pixels across the suite — 4.17 %.**

## What that makes the picture bound

**True, and it is corroborated rather than merely argued.** Every investigation this suite has driven
recently landed on the domain it admits: `board:1130` on mip chains and uv discontinuities, `board:1133`
on the sampler's coordinate, `board:1135` on the oracle recipe's point sampling, `board:1136` on four
pixels that turned out to be texel and surface identity. **The failure modes the suite finds are the
failure modes its domain contains**, which is evidence the split above is right — and simultaneously the
warning: a defect in the BRDF, the light loop, the attenuation or the visibility ray can only be seen by
eight cases, and a defect in the emitted arm's own inputs cannot be distinguished from a texture defect
at all.

**What it would take to move the number, in order of cost.**

- **`materials/scifi-helmet`: one manifest field, one case re-render, and the population becomes nine.**
  See the finding below — the blocker is that the case declares no light, not that the asset resists.
- **`materials/a-beautiful-game`: not recommended and the reason is the asset.** Two of its fifteen
  materials use `KHR_materials_transmission` and `KHR_materials_volume`, for which this engine has no
  term; its declared role is the draw list at scale — 49 parts, 1 500 224 triangles — and it is
  `general-position` with the picture judged by eye.
- **Beyond those two it is `board:0078`'s ladder** — new subjects with declared lights — and
  `board:1135`'s open decision about which cases render an integrating oracle, since anything needing
  more than a delta light needs more than one sample.

## The finding: the lowering is deliberate, its reason is recorded, and the reason is the LIGHT

**Checked before answering, and a reason exists in three places.** Each of the two manifests carries it in
`scene.material.note` citing `board:0087` — a closed body that sees itself makes a Diffuse BSDF at one
sample per pixel *a Bernoulli draw on the visible sky fraction, which is an ambient-occlusion estimator
and not a material*. `board:0088` names both assets among those whose oracle cannot be reduced. **So it is
a chosen reduction and not a default nobody picked, and the coordinator's suspicion is answered: no.**

**But the recorded reason attributes to the asset what belongs to the scene**, and the tree already states
the correct attribution in a different file. `materials/water-bottle`'s own light note:

> *A DELTA LIGHT AND NOT AN ENVIRONMENT, and on a metal-rough asset that is what makes the row decidable
> at all: a uniform environment delivers the same radiance from every direction, so roughness, metalness
> and the normal map all stop changing the picture.*

Both of these cases declare `light: none`, so the only illumination is the factory world — **a uniform
environment, under which roughness, metalness and the normal map cannot change the picture by that same
argument.** A metal-rough oracle there would be undecidable, which is why lowering it to an emitter is
correct *given the scene*. The chain is: no light → uniform environment → nothing metal-rough is decidable
and a diffuse closure is a variance estimator → lower to emission. **Every link is about the light, and
none of them is about the material stack.**

**And the "whole material stack" wording does not hold of `SciFiHelmet`.** Read from the pinned file: it
declares **no extensions at all** and **one** material — base colour, metallic-roughness, normal,
occlusion. That is the same row `water-bottle` binds (*base colour, metallic-roughness, normal and
emissive in four images and this arm binds all four*), and `water-bottle` is a **live metal-rough case**
with a delta sun. So the corpus holds two assets of one kind under two different oracles, and the
difference between them is one field. `board:0088`'s entry is right about `ABeautifulGame` and about
`Barcelona Pavilion`; it is the `SciFiHelmet` half that the file contradicts.

**RECOMMENDATION, WITH ITS EVIDENCE, AND IT IS NOT A RULING.** Give `materials/scifi-helmet` the delta sun
`materials/water-bottle` already declares — `irradianceWPerM2 = pi`, `angleRad = 0`, the beam off the view
axis — and `material.kind: metal-rough`. It costs one manifest edit and one case's re-render, it needs no
engine change and no new instrument, and it turns the corpus's richest single metal-rough material from a
texture case into a shading case. **It is the owner's call because it changes what a case asserts**, and
it is filed here rather than as an `issue` because it blocks nothing: the domain split below is owed
whether or not the corpus moves. What would overturn it: a measurement showing the asset's own
self-shadowing makes a delta-lit render undecidable at one sample — which `water-bottle`'s note says is
already accepted there as *judged by eye*, so the bar is met by precedent.

**TAKEN, by the orchestrator, on authority the owner delegated**, with the asset verified independently:
one material, `extensionsUsed` absent, `baseColorTexture · metallicRoughnessTexture · normalTexture ·
occlusionTexture`. **The declaration is `board:1152`**, a task under `board:0078`, whose rung 18 already
states this subject's criterion as *coverage · direct radiance* — so the edit delivers the half of a
declared rung that the corpus has never asked for, rather than adding scope. **The shading population goes
8 → 9**, and the split this item is *done when* must therefore be derived on the run rather than written
down, because it changes on the round that lands.

## Done when

The two published counts each carry the population they are over — **cases within the picture bound, split
into the eight that shade and the twenty-seven that do not** — so that a suite passing 24 of 35 can never
be read as a claim about shading that only eight cases can support. **The split is derived from the tree
and never stored**: the shading population is the set of cases whose `outshine.normal.raw` carries a
non-zero fragment, which is measured on the run that produces it rather than listed anywhere.
