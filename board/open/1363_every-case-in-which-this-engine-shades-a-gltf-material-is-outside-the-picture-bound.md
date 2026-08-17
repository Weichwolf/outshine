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
