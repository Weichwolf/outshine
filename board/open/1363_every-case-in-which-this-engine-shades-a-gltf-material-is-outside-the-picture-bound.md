Type: bug
Area: render
Tags: khronos, oracle, instrument

**Every case in which this engine shades a glTF material is outside the picture bound**

[MEASURED] over all 45 cases, sorted by what their manifest declares the material to be:

| `scene.material.source` | cases | within the bound |
|---|---|---|
| **`manifest`** — the runner computes a radiance and hands it to an unlit draw | 25 | **25 of 25** |
| `gltf-emissive` · `gltf-base-colour` — the file's own image, still unlit | 11 | 4 of 11 |
| **`gltf`, `kind: metal-rough`** — **this engine's BRDF evaluates the file's material row** | **9** | **0 of 9** |

**Not one of the nine, and none of them is close**: `DirectionalLight` 61.72 codes · `Lantern` 96.43 ·
`PointLightIntensityTest` 115.32 · `SpecularTest` 141.52 · `WaterBottle` 149.27 · `BoomBox` 166.69 ·
`NormalTangentMirrorTest` 184.36 · `Corset` 189.00 · `NormalTangentTest` 229.33 — against bounds of a
few codes.

## Why this is one finding and not nine

**Each of those nine has been diagnosed on its own at some point on this board**, and every diagnosis
found something real: a missing environment (`board:1206`), the specular textures (`board:1205`), the
mip chain (`board:1130`), the shading normal (`board:1126`). **What no single item could see is that the
partition is exact.** A case is inside the bound if and only if the *runner* decided its colour; the
moment this engine's BRDF is the thing under comparison, the case is outside by 60 to 230 codes.

**The instrument has never been green.** *`CLAUDE.md`'s closing line — no green resting on an instrument
nobody exercised — has an inverse this board had not stated: an instrument exercised nine times and red
nine times is not nine defects until something rules out that it is one.*

## What it does to the plan, and this is the reason it is filed rather than noted

`board:1171` sets the finish line at all 148 models green. Sorted by what a case would have to decide,
the 133 models still without one fall into two piles:

| pile | what decides it | status |
|---|---|---|
| **geometry and file structure** — indexing, strides, sparse accessors, node hierarchies, skins, morphs, cameras, scenes | coverage and a flat declared colour | **addable today.** Four were added this round and all four went green on the first run |
| **shading** — metal-rough grids, the `Compare*` series, texture encodings, faceting, two-sidedness | the engine's own BRDF against Cycles | **blocked on the arm above**, which has never produced a case inside the bound |

**So the corpus is not one queue.** The first pile is mechanical and its rate is known; the second does
not start until the shading arm produces one case inside the bound, and **which case that is matters
less than that there is one at all**.

## What must NOT be concluded from this

- [ ] **That the nine share one cause.** The partition is measured; a common cause is a hypothesis, and
  `board:1205`'s five eliminations on `SpecularTest` alone are evidence the residuals are not all the
  same thing. **What is established is that they share an ARM, not a defect**
- [ ] **That the manifest-material cases are therefore weak.** They decide geometry, and geometry is what
  they claim. *A flat-lit cube proves its silhouette and proves nothing about its faceting, which is why
  `Cube` was NOT authored this round — its criterion is "non-smoothed faces", and under a uniform
  environment an unoccluded diffuse surface returns `rho*L` independent of its normal, so every face of
  a convex body renders the same colour and the faceting is invisible by construction.*

## The one measurement that would decide the next step

- [ ] **The smallest subject that shades.** Every one of the nine is a whole asset with textures, several
  materials and a light. **A single sphere, one material, no texture, one declared value of roughness and
  metalness, under one declared light** would say whether the arm is wrong in its lobe or in what reaches
  it — and it is a subject this tree GENERATES rather than fetches, so it costs no pin and no licence.
  *`test/outshine/render/sphere` already exists and declares `manifest emission`; the question is what it
  reports when it is asked to shade instead.*

## The measurement this item asked for: the arm is wrong in its LOBE

**`test/outshine/render/shaded-sphere` was built and run.** One uv-sphere, ONE material, no texture
anywhere in the path, one delta light against a black world, and the material row is the file's own —
evaluated by this engine's BRDF. `test/harness/outshine/render/prepare/fixtures.py` gained a declared
metallic-roughness row so a GENERATED subject could take this arm at all; before it, every subject that
carried a material also carried textures, several materials and a light.

```
worst_disagreement_px            0            px      at most 0.005            PASS
picture_max_delta_code          48.275985     codes   at most 0.0006681348     FAIL
linear_channels_differing       129702        channels                         FAIL
linear_p50_relative              0.0057494845 dimensionless
linear_p95_relative              0.093481424  dimensionless
```

**The geometry is exact — 0 px — and the shading is 48 codes out.** Everything the nine fetched cases
confound is *absent here*: no image, no second material, no second body, no environment, no mip chain,
no uv discontinuity. **What is left is the lobe and the light that reaches it, and they disagree by
0.57 % at the median and 9.3 % at p95.**

**So the question this item posed is answered.** It is not *what reaches the lobe* — nothing reaches it
here but one declared irradiance from one declared direction. **It is the lobe.** And the shape of the
residual says where: a median under 1 % against a p95 near 10 % is not a uniform scale error, it is a
disagreement that grows towards one end of the distribution — which on a sphere under a single delta
light means towards grazing incidence or towards the terminator.

- [ ] **The next measurement is the residual against `n·l` and against `n·v`**, taken from the case's
  own shading-normal pass. *Named rather than run, because it is the first question that can now be
  asked of a subject with nothing else in it.*

## Binned, and the residual is a monotone function of `n·v` and of nothing else

The shaded sphere's two renders were binned against the normal of the **exact** sphere, reconstructed by
intersecting each pixel's ray with it -- **not** against our own shading-normal pass, which read as
unusable here and is its own finding below.

| `n·v` | px | p50 abs | p95 abs | ours mean | oracle mean |
|---|---|---|---|---|---|
| [0.00, 0.15) | 658 | 0.009417 | 0.050947 | **0.113749** | **0.095794** |
| [0.15, 0.30) | 3196 | 0.011774 | 0.041062 | 0.148932 | 0.133703 |
| [0.30, 0.50) | 7496 | 0.008821 | 0.024597 | 0.195258 | 0.185828 |
| [0.50, 0.70) | 11116 | 0.003810 | 0.010715 | 0.262592 | 0.258240 |
| [0.70, 0.85) | 10832 | 0.000816 | 0.002769 | 0.329403 | 0.328383 |
| [0.85, 1.01) | 12836 | 0.000830 | 0.002269 | 0.403979 | 0.404913 |

**At normal incidence the two agree to 0.0008 -- essentially exact. At grazing view ours is 18.7 %
brighter.** The sign is consistent: `ours - oracle = +0.003871` mean over 46 134 covered pixels.

**And `n·h` shows nothing.** In the highlight, `n·h` in [0.999, 1.01], the two agree to 0.0024 and their
means are 0.552460 against 0.554597. Binned by `n·h` at all, every bin above 0.90 sits at p95 ~0.002 to
0.003 while the bin BELOW 0.90 -- everything outside the highlight -- carries p95 0.0236. **So it is
neither the shape nor the placement of the GGX lobe.**

## What that leaves, and it is an inference rather than a second measurement

**The specular path agrees where it dominates**, and `MetalRoughBrdf.h` carries the height-correlated
Smith visibility in its standard form with the microfacet denominator folded in. **The residual lives
where specular does NOT dominate and grows monotonically as the view goes grazing** -- which points at
the DIFFUSE term's view dependence, not at the lobe.

**Named candidates, none of them asserted:**

- [ ] **Cycles' Principled diffuse is not Lambert at roughness 0.5.** Blender's Principled v2 carries a
  roughness-dependent diffuse; ours is `rho/pi * n.l` and has no view dependence at all. This is the
  candidate the data fits most directly, and it is a question about the ORACLE's closure that
  `board:1204`'s method answers: exercise it, do not read about it
- [ ] **Diffuse is not scaled by `1 - F`.** It would make ours brighter where Fresnel is large -- but F
  at normal incidence is already 0.04, so this predicts a 4 % disagreement at `n.v -> 1` and **the
  measurement there is 0.08 %**. *Stated because it is the obvious guess and the numbers refute it.*
- [ ] **A multi-scatter term on one side.** Cycles' GGX is multi-scattering by default, which ADDS
  energy at high roughness; here the oracle is DARKER, so this points the wrong way

## The shading-normal pass read as unusable on this case, and that is a second finding

The first binning used `outshine.normal.raw` and put **57 % of covered pixels facing away from the
light** -- impossible when the light is 34 degrees off the view direction, and the geometric
reconstruction puts the lit fraction at **94.1 %**. The arithmetic over that pass also raised
divide-by-zero and overflow. *Not chased here, because the finding above does not depend on it -- but a
pass this suite writes and no case reads is exactly the shape `CLAUDE.md` forbids.*

## The discriminator: it is the DIFFUSE term, and the specular path agrees to 1e-4

`test/outshine/render/shaded-sphere-black` is `shaded-sphere` with **one number changed** -- a base
colour of zero. A dielectric's diffuse term is its base colour times the incident irradiance, so it
vanishes; `F0` is fixed at the dielectric constant by `metallic = 0` and does not depend on the base
colour, so the lobe is untouched. Same geometry, same camera, same light, same roughness.

| | grey | **black base** |
|---|---|---|
| `picture_max_delta_code` | **48.275985** | **6.8036277** |
| `linear_p95_relative` | 0.093481424 | 0.019273589 |
| p50 abs at `n.v` in [0.00, 0.15) | **0.009417** | **0.000026** |
| p50 abs at `n.v` in [0.85, 1.01) | 0.000830 | 0.000148 |
| signed `ours - oracle` over 46 134 px | **+0.003871** | **-0.000143** |
| the monotone `n.v` trend | **present** | **gone** |

**The specular path agrees to 1e-4 across every view angle**, two to three orders of magnitude under the
grey case, with no view dependence and the OPPOSITE sign. Removing the diffuse term drops the worst
disagreement by a factor of **7.1**.

**So nine cases, 60 to 230 codes, reduce to one term: our diffuse has no view dependence and the
oracle's does.**

## And the next question is whether the ORACLE is the wrong side

**glTF 2.0 Appendix B specifies the diffuse BRDF as Lambert** -- `f_diffuse = (1 - F) * baseColor / pi`
-- with no roughness and no view dependence at all. **If Cycles' Principled carries a roughness-dependent
diffuse, then Cycles is not evaluating the BRDF this corpus is a corpus for**, and the reduction is
declared rather than the engine bent to match. *That is `board:1204`'s shape exactly, and it is the third
time this tree has met it.*

**One number already constrains the answer.** `(1 - F)` at normal incidence for a dielectric is 0.96, so
an engine that omitted it would sit **4 % bright at `n.v` -> 1**; the measurement there is **0.08 %**. So
the factor is being applied on our side, and whatever differs does so only as the view goes grazing.

- [ ] **Exercise Cycles' diffuse, do not read about it** (`board:1204`'s method): a Principled surface at
  `metallic 0`, `specular 0` if the socket allows it, over a sweep of roughness and view angle, against
  the Lambert closed form. **Two outcomes and they lead opposite ways** -- if Cycles is Lambert, the
  defect is ours and it is in how `(1 - F)` is applied; if it is not, glTF's own appendix says the oracle
  is the wrong side here and the case carries a declared reduction.

## Cycles' diffuse is not Lambert, and the two ends of the curve name the mechanism

The oracle's own diffuse term needs no new render: it is `oracle(grey) - oracle(black)`, and the
subtraction is exact because `F0` at `metallic 0` does not depend on the base colour. Lambert's
prediction is `rho * E/pi * n.l` = `0.5 * n.l`, the irradiance being pi W/m2 for exactly this reason.

| `n.v` | Lambert | **ours** / Lambert | **oracle** / Lambert |
|---|---|---|---|
| [0.00, 0.15) | 0.230531 | 0.899 | **0.755** |
| [0.15, 0.30) | 0.251406 | 0.973 | 0.872 |
| [0.30, 0.50) | 0.269630 | 0.992 | 0.943 |
| [0.50, 0.70) | 0.276792 | 1.001 | 0.984 |
| [0.70, 0.85) | 0.336078 | 0.970 | 0.967 |
| [0.85, 1.01) | 0.390039 | 0.962 | **0.964** |

**We are Lambert; the oracle is not.** Its diffuse loses up to 25 % against Lambert, monotonically as the
view goes grazing -- and the two ends of the curve name the mechanism rather than leaving it to be
guessed:

| | |
|---|---|
| at `n.v -> 1` the oracle attenuates by **0.964** | which is `1 - 0.04`, the directional albedo of a dielectric's specular layer at normal incidence |
| at `n.v -> 0` it attenuates by **0.755** | and that albedo rises to about 0.25 at grazing |

**So Cycles couples its diffuse to the specular layer by `1 - E(n.v)`, the layer's own directional
albedo.** glTF 2.0 Appendix B specifies `f_diffuse = (1 - F) * baseColor / pi` -- Lambert, with `F` on
the HALF-VECTOR and no view dependence at all. **Both are defensible and only one is the specification
this corpus is a corpus for.**

## So the ladder's second rung is the answer, and it is buildable

*Fix the engine, reduce the oracle, patch the asset, disqualify.*

- [ ] **The engine is NOT the rung.** Implementing Cycles' coupling would make this tree render something
  glTF does not specify, to make a number smaller. That is the shape `CLAUDE.md` refuses, and it would
  quietly change every one of the nine cases toward one renderer's private model
- [ ] **THE ORACLE REDUCES, and the preparer already knows how.** The `diffuse` material arm builds a
  **Diffuse BSDF** rather than Principled, precisely because *"Principled at metallic 0 still carries a
  specular lobe at IOR 1.5"*. The same reasoning applies one level up: a `metal-rough` recipe assembled
  from a Diffuse BSDF and a Glossy BSDF, mixed by Appendix B's own `(1 - F)`, evaluates the glTF BRDF
  instead of Blender's material model. **The reduction is declared with this measurement beside it.**
- [ ] **What must be re-measured after it, not predicted:** whether the nine cases move, and by how much.
  *This item has already been wrong once by reasoning past a measurement -- it nominated a common cause
  for the nine before the partition was measured -- and the specular residual it just found at 1e-4 is
  itself a term nobody has explained.*

**The residual that survives is small and real**: with the diffuse gone, `ours - oracle` is
**-0.000143** and largest at NORMAL incidence, which is the opposite end and the opposite sign from the
diffuse one. It is a second finding and it is not this one.

## The roughness-0 prediction was WRONG, and the refutation locates the term more sharply

**The reasoning, written down before the render**: on a smooth surface the microfacet half-vector is the
normal, so `v.h` becomes `n.v`, glTF's `1 - F(v.h)` and Blender's `1 - E(n.v)` become the same
expression, and the residual should collapse.

**It quadrupled.** `test/outshine/render/shaded-sphere-smooth` is `shaded-sphere` with roughness 0 and
nothing else changed. Away from the highlight -- `n.h < 0.98`, the same 44 253 pixels on both --

| `n.v` | roughness 0.5, signed | **roughness 0, signed** |
|---|---|---|
| [0.00, 0.15) | +0.017954 | **+0.074129** |
| [0.15, 0.30) | +0.015229 | +0.044686 |
| [0.30, 0.50) | +0.009430 | +0.019429 |
| [0.50, 0.70) | +0.004352 | +0.006903 |
| [0.70, 0.85) | +0.001019 | +0.001896 |
| [0.85, 1.01) | -0.000779 | +0.000263 |

**And that is exactly what the mechanism predicts once it is stated correctly.** At roughness 0 the
specular layer's directional albedo IS the Fresnel, `E(n.v) = F(n.v)`, and `F(n.v) -> 1` at the limb --
so Blender's diffuse attenuation `1 - F(n.v)` goes to **zero** there and its diffuse term vanishes at the
silhouette. glTF's `1 - F(v.h)` does not: with a fixed light direction, `v.h` at the limb is nothing like
grazing. **The two do not coincide at roughness 0 -- they diverge hardest there.**

## So the term is the ARGUMENT of the Fresnel, not the roughness coupling

| | attenuates the diffuse by |
|---|---|
| **this engine** | `1 - F(v.h)` -- the half-vector, which glTF Appendix B's `f_diffuse = (1 - F) * baseColor / pi` shares with its specular term |
| **Cycles' Principled** | `1 - E(n.v)` -- the VIEW angle, through the specular layer's directional albedo |

**Both are one function of one angle and they are different angles.** That is a smaller and much more
specific statement than *the diffuse disagrees*, and it was reached by a prediction being wrong: the
roughness-0 case was built to make the two agree and it made them disagree four times harder, which is
the measurement that named which variable each side is really using.

- [ ] **What the reduction has to express is now known and it is NOT expressible with stock nodes.**
  Blender's `Fresnel` and `Layer Weight` nodes are functions of `n.v`; nothing in the node vocabulary
  reaches the microfacet half-vector. **So a node-graph oracle cannot evaluate Appendix B's diffuse**,
  and the reduction is a statement about what this oracle can decide rather than a graph to be wired.
  *Named here rather than attempted, because the attempt is what would produce a graph that looks like
  the specification and is not.*

## Our side is Appendix B verbatim, so the oracle is the wrong side here

Read from `src/render/stages/MetalRoughBrdf.h` rather than assumed:

```
const std::array<double, 3> fresnel = BrdfFresnel(f0, at.Vh);          // the HALF-VECTOR
terms.Diffuse[channel]  = (1.0 - fresnel[channel]) * diffuseColour[channel] * (1.0 / kBrdfPi);
terms.Specular[channel] = fresnel[channel] * lobe;
```

That is `f_diffuse = (1 - F(v.h)) * baseColor / pi` and `f_specular = F(v.h) * D * V` -- glTF 2.0
Appendix B, term for term, with the Fresnel on the half-vector in both halves. **The engine implements
the specification the corpus is a corpus for; Cycles implements Blender's material model.**

**So this is `board:1204`'s shape for the third time, and it is the largest instance of it yet**: nine
cases, and the whole shading half of the remaining corpus. *The reduction is declared with its
measurement rather than the engine bent to match.*

## But "the oracle cannot" is a claim about stock nodes, and one route is not closed

- [ ] **OSL.** Blender supports Open Shading Language closures on CPU, and an OSL shader can compute the
  half-vector because it is a program rather than a node graph. **Appendix B is twenty lines of it.** The
  cost is real and it is CPU rendering -- this corpus renders on Metal today -- so it is a trade to be
  priced and not an obvious yes.
- [ ] **What it would buy is not a smaller number, it is a DECIDABLE arm.** Without it, `board:1171`'s
  finish line cannot be reached by measurement for any model whose criterion is a shaded surface: those
  models can only ever carry a declared reduction, and the owner's ruling struck a declared reduction as
  an end state. **That tension is real and it is the owner's, so it is named here rather than resolved.**

**What must be re-measured before any of this**: the specular-only residual of **-0.000143**, largest at
NORMAL incidence and opposite in sign to the diffuse one. It is two orders under the diffuse term and it
is not explained, and a round that fixed the diffuse and declared the arm green would be resting on it.

## Comments

**`DirectionalLight` is this defect's picture, and it was LOOKED AT before it was believed.** The case
scores p99 5 codes against a bound of 1 and is outside. Its two renders are **indistinguishable by
eye**: three olive spheres, same places, same terminators, same highlights, on both sides.

**Its bound carries two terms and neither of them is this defect.** [MEASURED]

| term | codes |
|---|---|
| f32 arithmetic order | 0.000668 |
| the 8-bit transfer's own quantisation step | 0.999332 |
| **total** | **1.0** |

So the residual has nowhere declared to live, and 5 codes on a dark olive surface is exactly the size
the measured coupling difference makes -- up to 24 % of the diffuse term at grazing angles.

**This does not license a term in the bound.** The ladder's first rung is *fix the engine*, and the
coupling is an engine defect with a measurement, not an oracle limit. A bound term for it would be the
frame fitted to the number this file already warns against.

**Together with `board:1401` this accounts for EVERY case the picture bound refuses**: two of them,
`VertexColorTest` because the oracle drops `COLOR_0`, and this one. *Of 52 red cases, two are red
because the picture differs, and both have a named cause.*

## CORRECTION -- the partition this item is built on no longer holds

This item's finding was that a case is inside the picture bound **if and only if** the runner decided
its colour, and that *the instrument has never been green*: 0 of 9 where this engine's BRDF is what is
compared. [MEASURED] again over the corpus as it now stands, 151 cases:

| what decides the colour | within | outside |
|---|---|---|
| `emission-per-material` | 82 | 12 |
| `emission-by-material-index` | 8 | 2 |
| **`gltf`, `kind: metal-rough` -- THIS ENGINE'S BRDF** | **6** | **2** |
| `gltf-base-colour/emission` · `diffuse` · `gltf-emissive/emission` | 16 | 0 |

**Six of eight shading cases are inside the bound.** The two that are not are `DirectionalLight` and
`SpecularTest`. Every one of the others named in this item's own list -- `Lantern`, `WaterBottle`,
`BoomBox`, `Corset`, `NormalTangentMirrorTest`, `NormalTangentTest`, `PointLightIntensityTest` -- is
now within it.

**What moved is not one thing and the honest answer is that this was not attributed case by case at
the time.** Between the measurement above and the one this item opened with: the picture verdict became
a p99 rather than a maximum (`board:1367`), the bound gained a perceptual floor of one code
(`board:1359`), every derivable camera became the framing rule's own answer (`board:1398`), and the
oracle learned to multiply `COLOR_0` (`board:1401`). *Any of those could carry most of it and none of
them was measured against this partition on its own.*

**The claim that is dead is the strong one**: *the moment this engine's BRDF is the thing under
comparison, the case is outside by 60 to 230 codes*. It is not. **The claim that survives is narrower
and still worth an item**: two shading cases remain outside, and one of them -- `DirectionalLight` --
is a picture the eye cannot tell apart at p99 5 codes.

**This matters beyond this item.** The shading arm being green is what makes every material extension
under `board:1382` verifiable rather than built blind: `KHR_materials_sheen` and its kind can now be
held against an oracle that agrees with us on the base lobe. *Before this measurement, building them
would have been eleven lobes checked by nothing.*
