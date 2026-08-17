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
