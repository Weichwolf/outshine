Type: feature
Area: generators
Tags: oracle, khronos, perf

**I.26.7 The forest rung — a scenario test, not a render test, and that is what it means to measure cost**

*Owner's ruling, 2026-08-12: **our renderer must be good at drawing vegetation**, and a forest scene in
glTF is how that is built and proven. It reverses a foliage exclusion I had applied one round earlier,
and the correction is worth stating plainly: **excluding forests protected the parity ladder from a
subject that was never on it, at the cost of the hardest workload the renderer will ever meet.***

**This is the rung where the ladder changes what it is measuring, and the change is large enough that it
changes which suite the rung lives in.** Rungs 1–21 ask *is the picture right* against an oracle. The
forest rung asks *does it hold 60 Hz and does it look right while moving* — so by § I.26.9's placement
rule it is a **scenario test**, declared as camera × clock over a studio stage (§ I.25), and it is the
first **performance** acceptance anywhere in this section. No parity number, no IoU, no radiance. *It was
first filed under the render suite, which was wrong in a way worth keeping visible: a case with no score
inside a suite whose contract is a score is precisely the hollow case the empty-image guard exists to
catch, and this one would have been the first.*

**A forest is the only subject that exercises six things nothing else on the ladder touches.**

| What only a forest exercises | Why nothing else covers it |
|---|---|
| **instancing at scale** — thousands of copies of a handful of meshes | every Khronos feature test is one object; `SimpleInstancing` is a proof of syntax, not of scale |
| **alpha test and alpha-to-coverage on leaf cards** | `AlphaBlendModeTest` is flat quads at one depth |
| **overdraw** — the fill-rate killer on 5 GPU cores | a closed opaque mesh generates none; depth complexity in a canopy is tens |
| **two-sided thin-surface shading, leaves lit from behind** | needs `doubleSided` *and* diffuse transmission on the same primitive |
| **LOD transition and popping** | a still rung structurally cannot see it |
| **density culling at distance** | there is nothing to cull in a single-subject scene |

- [ ] **The instrument, and it is not the parity ladder's**: **frame time p50/p95/p99 over a moving camera at 720p60**, never a mean — `CLAUDE.md`'s existing rule, and this is the first rung that can apply it to a real workload · **by eye and in motion** for popping, ghosting and LOD transition · **Blender's render of the same scene as a reference for the eye only** — canopy translucency and occlusion, what the light does under a crown — and **never as a coverage or radiance number**
- [ ] **No IoU, no boundary p95, no radiance parity on this rung.** Our alpha-to-coverage against Cycles' stochastic transparency compares two sampling policies, and a per-pixel mask difference there measures the choice rather than the implementation
- [ ] **Blender's render of the same scene is a reference for the eye and never a number** — canopy translucency, what the light does under a crown, whether the crown reads as a crown. It is opened beside ours by a person; nothing reads it
- [ ] **It decouples the renderer from the generator, and that is a scheduling argument with teeth.** The vegetation **rendering path** — instanced leaf cards, alpha test, two-sided transmission, LOD, density culling — can be built and proven against a downloaded forest **before the tree generator exists**. Afterwards it is a **known-good target**: when the generator's first canopy looks wrong, the renderer is already exonerated and the defect is attributable in one step instead of two
- [ ] **It is a class claim and therefore an acceptance gate, not an optional extra at the end.** Kingdom Come: Deliverance's identity *is* its forest. A renderer that cannot hold 720p60 in one has failed the CryEngine-class claim whatever its parity numbers say — so **the renderer stage is not done until the forest rung is green**, and it is a gate on that stage rather than a rung somebody gets to later
- [ ] **Its dependency is rung 12 and nothing further**: it needs instancing and alpha test to exist and to have passed as isolated render cases, and it needs nothing else from the parity ladder — not radiance, not skinning, not the film. *Being in another suite does not free it from the dependency order; a join still waits for its inputs (§ I.26.5)*
- [ ] **The subject is Poly Haven's `pine_forest` collection, and it is the cleanest licence in the corpus: CC0.** *"All assets on this site … are licensed as CC0"*, *"You can use our assets for any purpose, including commercial work"*, *"You do not need to give credit"*, *"You can redistribute them"* (`polyhaven.com/license`). Sixteen models tagged `collection: pine_forest`, **published natively as glTF at 1k/2k/4k** — so no `.blend` export step exists on this rung at all and § I.26.2's lossy-export risk does not apply
- [ ] **Poly Haven's trees are scan-derived and enormous, and the selection follows from that rather than from taste**: measured through the API at glTF/1k, `pine_tree_01` is **958 MB** and `fir_tree_01` **487 MB** — the whole collection is **1 874 MB**. So the rung takes the **saplings, ferns, grass, moss, roots, stumps and rocks** (each under 26 MB, ≈ 82 MB together) and builds density by **instancing**, which is what the rung is measuring anyway. One hero tree is fetched separately and only if a large-mesh arm is wanted
- [ ] **Poly Haven is a third source kind, `api`** — an asset id resolved through `api.polyhaven.com/files/<id>` to a URL and a declared byte size, then pinned by **URL plus SHA-256** like any `file`. The API is how the URL is *discovered*, never what is trusted: a manifest entry records the resolved URL and hash, so a rebuilt corpus does not depend on the API answering
- [ ] `demo/eevee/ember_forest/forest.blend` (74.3 MB, CC-BY, Mike Pan) is the **second forest and the export arm** — a real authored forest scene that must survive § I.26.2's conversion, where the Poly Haven collection needs no conversion. Two forests from two pipelines is what separates *our vegetation path is slow* from *the exporter dropped the leaf cards*
- [ ] The forest rung's **motion arm** is `sprite_fright_030_0020_A` (CC-BY 4.0, confirmed in its own `readme.txt`), because popping and LOD transition need a moving camera through a dressed set, and it needs no per-frame parity number to produce that judgement
- [ ] **A declared density sweep, because one forest is one data point**: instance count per hectare swept over a declared range with the frame-time distribution at each step, so the product is *where 720p60 breaks* rather than *it was 47 ms once*. That curve is the rung's real output and it is what a later generator is tuned against
