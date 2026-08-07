# Continuous LOD — one rule for every pass

**Origin:** §4.2 of [`visual-target.md`](visual-target.md), split out because it binds **every** pass
rather than one. Neighbours: [`renderer.md`](renderer.md) (the pass topology the ladder runs inside),
[`vegetation.md`](vegetation.md) (the one layer that keeps a ladder of its own),
[`classification.md`](classification.md) (the class is decided once, the ladder only changes how it is
drawn), and the per-pass files under [`stages/`](stages/).

## Spec

### Continuous LOD: 1 m to >1000 m, and no seam anywhere in it

> Owner: *„und continous LOD fähig. nahtloses herauszoomen von 1m bis >1000m."*

**Three decades of scale, roughly ten octaves, with no visible step in any of them.** This binds every
stage of [`classification.md`](classification.md)'s chain, not just vegetation — a ground material that
tiles at 5 m and a tree that pops at 80 m fail the same requirement.

**There are no tiers, and no distance is ever declared. SCREEN-SPACE ERROR decides, and nothing else.**
A detail exists while it still covers pixels:

```
sse_px = size_m · f_px / d      →  drawn while  sse_px > τ
```

`f_px` is the focal length in pixels (`0.5·height / tan(0.5·vFov)`), `τ` a single threshold **in pixels**.
**Distance is a consequence, never an input** — the fade radius for a 1 cm pebble and a 30 m beech fall
out of the same line, three decades apart, because their `size_m` differs by three decades. Nobody writes
either number down.

**The formulation is not ours and the pixel unit is the literature's.** Ponchio states it in exactly this
shape: *„We use the screen projection error E which is simply the projection of the model space error λ
on screen. In order to have a conservative estimate of the error, the closest point in the bounding
volume of the patch should be used. Bounding spheres are particularly suited for this error metric …
A user specified error threshold τ can then be simply expressed in pixels."* (Ponchio, *Multiresolution
structures for interactive visualization of very large 3D datasets*, PhD thesis, 2008, §3.6.1,
[PDF](https://vcg.isti.cnr.it/~ponchio/download/ponchio_phd.pdf).)

**τ is one pixel, and that number is measured rather than felt.** The values actually shipped, with what
each ran on — the full derivation and the disagreement between them is in `## Knowledge`:

| Source | τ | Error notion |
|---|---|---|
| Hoppe, *View-Dependent Refinement of Progressive Meshes*, SIGGRAPH 1997 | 0.075 %…0.33 % of window size = **0.45…2 px at 600²**; a regulated flythrough is *„never allowed below 0.5 pixels"* | Hausdorff distance, generalised to an anisotropic deviation space — **explicitly silhouette-sensitive** |
| Hoppe, *Smooth View-Dependent LOD Control … Terrain Rendering*, Visualization 1998 | *„Popping is avoided if the screen-space error tolerance is kept near a value of **1 pixel**"* | as above, plus geomorphs |
| Ulrich, *Rendering Massive Terrains using Chunked Level of Detail Control*, SIGGRAPH 2002 Course | *„around **2-pixel** screen-space error tolerance, at a resolution of 1024×768"* | object-space max vertical error per chunk |
| Ponchio 2008 (BDAM / P-BDAM results) | **1 px** for Puget Sound at 800×600; **3 px** for Mars at 640×480 | projected model-space error λ, conservative over a bounding sphere |
| Karis et al., Nanite, SIGGRAPH 2021 | ***„< 1 pixel error"*** | Quadric Error Metric (Garland & Heckbert 1997), position **and** attribute error mixed with weights |

So *„on the order of one pixel"* was right, and it is now a citation rather than a memory. **What was
missing is which error**, and the answer is not one thing: the terrain lineage projects a
**scalar/vertical deviation**, Hoppe 1997 projects an **anisotropic deviation space that refines harder
at silhouettes**, and Nanite projects a **QEM residual with attribute error folded in**. Karis is candid
that the last of those has no theory behind it: *„It's a dumb heuristic to mix them as if they are the
same thing. They aren't but I still don't have anything better."*

**Does τ depend on AA? Yes, and in both directions — this is the part memory got wrong.**

* **Downward, by resolution convention.** Hoppe 1997 expresses τ as a *fraction of viewport size*, so his
  τ grows with resolution. This file holds τ in **absolute pixels**, which is the modern convention and
  the one that makes the „at 1440p every element survives twice as far" bullet true. Both are defensible;
  ours is a choice and is named as one. `[SET]`
* **Upward, by temporal accumulation.** Hoppe 1998 already used it without TAA: geomorphs let him *„increase
  the pixel error tolerance to improve frame rate"* while *„the model may at times have a projected
  geometric error of a few pixels"* and geomorphs *„make this error nearly imperceptible."* Nanite makes
  the same trade against TAA rather than geomorphs. **The transition mechanism and τ are one budget, not
  two independent knobs.**
* **And the reverse holds.** Ponchio reports that when the frame rate allows *„very small pixel
  thresholds"*, popping is *„virtually eliminat[ed] … without the need to resort to costly geomorphing
  features."* A sufficiently small τ makes the whole transition mechanism unnecessary. That is the same
  statement as Nanite's, from eighteen years earlier and a different subsystem.

Four properties follow, and they are the reason this is the only acceptable formulation:

- **Resolution-independent.** At 1440p every element survives twice as far, automatically. A hard-coded
  radius would have to be re-tuned per resolution — and [`visual-target.md`](visual-target.md) §1.1 lets
  the engine, not the mod, pick it.
- **FOV-independent, which is what „zooming out" actually means.** `f_px` carries the zoom, so a narrow
  FOV keeps detail at range without a second table.
- **Size-adaptive within a layer.** A mature oak outlives a sapling of the same species with no rule of
  its own; a cobble outlives a sand grain.
- **One rule for every layer.** Ground, shrub, tree and building use the same inequality with a
  different `size_m` — there is no per-layer LOD policy to keep in sync.

**What each layer plugs in as `size_m`** — the smallest feature that still has to read, not a distance:

| Layer | the feature whose pixel size decides | what remains once it drops below τ |
|---|---|---|
| ground material | grain / height-amplitude wavelength, per octave | the next coarser octave; finally the class reflectance alone |
| grass · clutter | **no row here.** Nothing below the size of a tree is geometry ([`../goal.md`](../goal.md)), so the stand is a fragment term at every distance and has nothing to select. What a footprint still retires inside it — the tussock rung of the canopy-top field — retires to its own MEAN, not to a coarser representation ([`stages/terrain.md`](stages/terrain.md)) | |
| shrubs · perennials | branch thickness, then leaf-card width | the plant's own card, then canopy mass |
| trees | leaf-card width, branch thickness, trunk width | octahedral impostor, then aggregate canopy |
| buildings | facade band height, then storey height, then footprint width | the massing, then the town's silhouette |

Each row is a **continuum of octaves**, not the two states the table's two columns might suggest: an
octave fades as its own `sse_px` crosses τ, so several are always cross-fading at once. That is what
makes the transition invisible — never a switch between representations.

**The mechanism is decided and must not be re-invented per layer**, and it is *not* „dither and hope".
The order of preference is what [`visual-target.md`](visual-target.md) §2's sources establish:

1. **Keep the error below τ.** Then no transition mechanism is needed at all — Ponchio 2008 and Karis
   2021 agree on this independently. This is the default.
2. **Where 1 cannot be afforded, dither — but hashed, and anchored in object space.** A per-frame random
   threshold produces *„continuous twinkle"* that TAA does not fix (Wyman & McGuire, I3D 2017). Their
   conditions are exact and are transcribed in `## Knowledge`.
3. **Geomorph** where the topology allows it — the terrain answer (Hoppe 1998), lifetime one second.

`~/Git/wasm-tree` already implements the vegetation half of the ladder — continuous LOD, octahedral
impostors, alpha-to-coverage, cross-tree instancing of all LOD levels — and that is the part that
transfers ([`vegetation.md`](vegetation.md)).

### Nanite as ONE ladder for all five layers — judged, not assumed

> Owner, 2026-08-06: *„Nanite ist vielleicht sogar die einfachste Lösung."*

The argument for it is not triangle throughput, it is **policy count**: without one mechanism this spec
needs five LOD systems — foliage cards, tree impostors, building simplification, terrain chunks, ground
detail octaves — each separately correct and each separately able to pop. Nanite's selection criterion
*is* screen-space error, which this file already declares the only admissible one.

**The decomposition holds.** Karis's notes separate cleanly into two halves that are not co-dependent:

| Half | What it does | Needs 64-bit atomics? |
|---|---|---|
| **1 — cluster DAG + SSE selection** | meshlets in a hierarchy; parents' stored error forced ≥ children's so the error function is **monotone along every root-to-leaf path**, which is what guarantees a unique cut and therefore crack-free neighbours at different levels | **no** |
| **2 — compute software rasteriser** | sub-pixel triangles beat the hardware rasteriser; clusters whose triangle edges are **< 32 px** go to software, and a scanline variant is chosen when the inner X loop exceeds **4 px** | **yes** — *„Use 64 bit atomics! InterlockedMax"* into the visibility buffer |

Half 1 is where seamlessness comes from. Half 2 is a throughput optimisation. **The decomposition is
confirmed, and it survives the WebGPU gap** ([`visual-target.md`](visual-target.md) §2.1 states the gap
itself and `## Knowledge` there carries the specification text).

**The 64-bit hole is real and it costs exactly half 2 — and this has been demonstrated, not reasoned.**
`Scthe/nanite-webgpu` is a working Nanite in the browser: meshlet LOD hierarchy (meshoptimizer + METIS
compiled to WASM), software rasteriser, octahedral billboard impostors, per-instance and per-meshlet
frustum and occlusion culling. Its author's own account of the wall he hit:

> *„WebGPU does not support `atomic<u64>`, so I had to compress the data to fit into 32 bits (u16 for
> depth, 2\*u8 for octahedron-encoded normals). It's a painful limitation … 16-bit depth is.. not a great
> idea. It produces **tons** of artifacts like z-fighting or leaks."* · *„No visibility buffer. It's not
> possible with the `atomic<u64>` limitation that I have."*

He names the 32-bit workarounds and rejects them on cost, not on feasibility: per-pixel linked lists,
tiled binning, or **double rasterisation — first pass writes depth, second resolves with
`compareExchange()`** — *„There are other algorithms to achieve this, but they are much slower."*
The Z-prepass-plus-`atomicMin` route is therefore **available and unmeasured**, not blocked.

| | Verdict | Evidence |
|---|---|---|
| **N1 — alpha-cutout vegetation** | **carries, with a named cost** | UE5 docs today: *„Nanite supports materials that have their Blend Mode set to Opaque and **Masked**."* The 2021 notes said the opposite (*„Does not yet support: Translucent or masked materials"*), so this changed after publication. What has **not** changed is the aggregate problem, and Karis states it as a scaling failure rather than an artefact: *„The property of scaling cost with screen resolution doesn't hold with aggregates such as grass and leaves."* Epic's answer is a separate system — „Nanite Foliage … a combination of instancing, skinned meshes, **voxelization**, animation, and material characteristics" — i.e. **the densest layer got its own machine anyway** |
| **N2 — wind-animated geometry** | **carries partially; the hard part is unsourced** | UE5: *„deformation with World Position Offset (WPO) in a material is supported, [but] limited. Nanite meshes using WPO displacement are **split into smaller clusters** whereby each of those clusters have their own individual bounds and are **culled individually**. You must **clamp the amount of displacement** in order to manage how many clusters … are culled."* So the suspicion is right: **WPO costs culling efficiency**, priced in cluster count and inflated bounds. Epic adds one observation that helps us specifically: *„Foliage using WPO is less problematic because the foliage is filled with holes and cannot really occlude itself"* — occlusion culling was never earning much on foliage |
| **N3 — memory** | **the numbers are known and they are not the blocker** | Karis 2021, measured on their demo: **5.6 bytes per Nanite triangle** on compressed disk, **11.4 bytes per input triangle**, **~10.9 MB per 1 M triangles**; a 25.90 GB raw scene becomes 7.67 GB in memory and 4.61 GB on disk; **40.5 KB per mesh always resident**; clusters ~2 KB in 128 KB pages. Against [`visual-target.md`](visual-target.md) §1's 4 GB: 40.5 KB × a few hundred distinct meshes is tens of MB, which is affordable. **But the demo's budget is a console's, not a phone's** — and Nanite requires **DX12 Shader Model 6**, which is a desktop/console floor, not a mobile one |
| **N4 — has anyone built it in WebGPU** | **yes, and the limit they hit is precisely the one above** | `Scthe/nanite-webgpu` (~1.1 k stars). Also his verdict on where the difficulty actually sits: *„Meshlet LOD hierarchy is quite easy to get working. Praise meshoptimizer and METIS! But if you want to do it efficiently, it will be a pain."* And: *„unless you tackle simplification and error metric problems, you will end up with code similar to mine."* His Jinx model only simplified 44 k → 3 k triangles, against Karis's claim (slide 95) that **all** UE5 LOD graphs terminate in a **single 128-triangle root cluster** |
| **N5 — the fallback if it tips** | **named, and it is the same half 1** | **meshoptimizer** (Kapoulkine) supplies `meshopt_simplify` + `meshopt_buildMeshlets`, and since [PR #704](https://github.com/zeux/meshoptimizer/pull/704) `meshopt_simplifySparse` *„specifically for Nanite clones"*; the open design thread is [discussion #750](https://github.com/zeux/meshoptimizer/discussions/750), „Nanite-style DAG clusterizing and simplification". Partitioning is METIS. The academic ancestor Karis himself credits is **Cignoni et al., *Batched Multi-Triangulation*, IEEE Visualization 2005** and Ponchio's 2008 thesis — both of which run on the *hardware* rasteriser |

**JUDGEMENT: it carries, in the half that matters, and the simplification win is real but smaller than
five-to-one.** Half 1 — cluster DAG with monotone SSE selection — is buildable on core WebGPU today, has
a working browser precedent, and has an off-the-shelf toolchain. It replaces the LOD policy for
**buildings, terrain and rock**, and it is the same rule this file already mandates. Half 2 is deferred
until `atomic_vec2u_min_max` ships or a 32-bit route is measured, and its absence costs throughput, not
seams.

**What it does NOT collapse:**

* **Vegetation keeps its own ladder.** Not because masked materials are unsupported — they are supported
  — but because Karis names aggregates as the case where the cost model itself breaks, and Epic's own
  answer to foliage is a *separate* system with voxelisation in it. `~/Git/wasm-tree`'s impostor ladder
  is not made redundant ([`vegetation.md`](vegetation.md)).
* **Ground-detail octaves are not geometry.** They are a shader-side frequency ladder
  ([`stages/terrain.md`](stages/terrain.md)). A mesh DAG has nothing to say about them.

So the honest count is **five policies → three**: one DAG for buildings/terrain/rock, one vegetation
ladder, one shader octave ladder. That is a real reduction and a poor slogan.

### Motion vectors when the cluster cut changes — and whether wind rides the DAG

> Owner, 2026-08-06: *„Nanite könnte ja auf jedem Mip-Level Bewegungsvektoren tragen."*

**First, the general question, because its answer decides the wind case: how does Nanite produce a motion
vector for a vertex that did not exist in the previous frame?**

**It does not, and the primary source does not raise the problem.** Searched through the whole of Karis
et al. 2021 for motion vectors, velocity and reprojection: every hit is about **occlusion culling**
(previous frame's HZB, two-pass culling, reprojecting geometry rather than the z-buffer). On temporal
correspondence across an LOD switch the notes say exactly one thing — *„Temporal AA sees any difference
as aliasing"* — and that is not a solution, it is the **reason a solution is not needed**: because the
cut is only allowed to change while the resulting error is under 1 px, the missing correspondence *is* a
subpixel discrepancy, which is the class of error TAA is built to absorb. **Sub-τ selection is what pays
for the absent motion vector.** The two halves of [`visual-target.md`](visual-target.md) §2's argument are
the same argument.

**Consequence for this spec, stated so it is not re-derived per layer:** a motion vector is owed for
*object and camera* motion, which is a per-instance transform and is available at every level. It is
**not** owed for the LOD change itself, provided τ holds. Where τ does not hold — impostor swaps, card
fades, anything above a pixel — the transition needs its own mechanism and TAA will not cover it.

**Second, the wind proposal. Assumption A is correct and is verified against the source.**

`~/Git/wasm-tree`'s wind (`src/render/render.c`, `CARD_INST_VS`) is a **closed-form function of
`u_time`, the world-space anchor point and a per-instance phase.** There is no integrator, no previous
state, no feedback:

```
t      = u_time · wind_freq
gust   = 0.55 + 0.45·sin(t·0.33 + wbase.x·0.15)      // travelling gust
gsway  = sin(t·0.80 + sigp)                           // whole tree
branch = sin(t·1.00 + i_phase·0.5 + sigp·0.3)         // branch level
ripple = sin(t·2.70 + i_phase·3.0)                    // leaf flutter
sway   = wind_amp · i_size.y · tip ·
         ((0.6·branch + 0.25·gsway)·(0.5 + 0.9·gust) + 0.18·ripple)
```

Substituting `u_time − Δt` yields `Position(t−Δt)` for **any** vertex at **any** level, including one
that had no counterpart last frame. **The motion vector therefore never needs to be stored, on any
level.** The owner's instinct that it could be carried per level is right; the stronger result is that it
need not be carried at all. That is the cheapest possible answer and it is a property of choosing wind
that is analytic — a constraint worth defending, since a wind that ever becomes stateful (collision,
a simulated gust field) forfeits it.

**Third, assumption B — „amplitude is what must be simplified per level" — is the right shape, and the
premise under it is wrong in a way that matters.** The premise was that `wasm-tree` already carries a
hierarchy of wind levels attached to a hierarchy of geometry. It does not; what it carries is measured in
[`vegetation.md`](vegetation.md) `## State`. So the mapping *level → wind band* is **not an existing
correspondence to exploit; it is work to be done** — and the geometry it would attach to (a
wind-displaced trunk and branch mesh) does not exist either. The idea survives; the claim that it is
nearly free does not.

The amplitude argument itself is sound and is already half-implemented in the small: `sway` scales with
`i_size.y` (card size) and with `tip`, so a bigger card already swings further. What is missing is the
*frequency* half — and that is the part that would actually alias. A cluster that has merged a thousand
leaves into ten triangles must stop carrying the 2.7·t ripple, because its silhouette can no longer
resolve that frequency; what it must keep is the 0.8·t sway. **Dropping the high band at coarse levels is
band-limiting, exactly like a mip level**, and it is the same argument this file makes for ground octaves.

**No source was found for LOD-dependent damping of vertex animation.** What the canonical vegetation-wind
reference does establish is the *band structure*: Sousa, *Vegetation Procedural Animation and Shading in
Crysis*, GPU Gems 3 ch. 16, 2007 — **main bending** (whole plant, along the wind vector) plus **detail
bending** (per-leaf, with per-leaf phase and stiffness from vertex colour), frequencies given as
*„1.975, 0.793, 0.375, 0.193 are good frequencies"*. Two bands, cleanly separated, and **nothing about
attenuating either with distance or LOD**. Searched Karis 2021 and the UE5 Nanite documentation for the
same: nothing. This is an **unsourced design, not a known technique** (`## Gaps`).

**The strongest counter-argument found, stated because it is not fatal but is expensive.** It is not the
motion vector and not the amplitude — it is UE5's own note that WPO forces Nanite to **split meshes into
smaller clusters with individually culled bounds, and to clamp the displacement** so the inflated bounds
do not swamp culling. Wind on a cluster DAG therefore taxes exactly the mechanism that pays for the DAG.
Against that, Epic's second observation applies to our case specifically — foliage is porous and never
occluded much anyway — so the tax is levied on a revenue we were not collecting. **Bounds inflation is
the number to measure before committing**, and it has an analytic ceiling here: `sway ≤ wind_amp ·
i_size.y · 1.0 · (1·1.4 + 0.18)`, so the bound grows by a fixed fraction of card length rather than by
an unbounded material expression.

### Acceptance

**„Seamless" must be measurable, because it is otherwise taste.** It has two halves, and only the second
one is ours.

**Half one — the popping metric is FLIP, and it is not an invention.** The question „is this LOD
transition visible?" is literally the question FLIP was built for: *„a difference evaluator … [whose]
algorithm produces a map that approximates the difference perceived by humans **when alternating between
two images**"* (Andersson, Nilsson, Akenine-Möller, Salvi, Munkberg, Fairchild, *FLIP: A Difference
Evaluator for Alternating Images*, Proc. ACM Comput. Graph. Interact. Tech. 3(2), 2020,
[NVIDIA](https://research.nvidia.com/publication/2020-07_FLIP)). Alternating between two images is
exactly what a pop *is*. It outputs a per-pixel map, poolable to a weighted histogram or a single value,
and it is parameterised by **pixels per degree** — so the verdict follows the actual viewing condition
rather than the render resolution. FLIP's default is 67 ppd (a 0.69 × 0.39 m 4K panel at 0.70 m).

**Applying it here:** render frame *n* and frame *n+1* across a scripted fade, FLIP them, and pool. A
transition that is invisible produces a FLIP response indistinguishable from the same pair with the
transition frozen. **No threshold value is set here** — the LOD literature does not publish one, and
inventing one would repeat the defect this section is fixing. Deriving it is a measurement task
(`## Gaps`).

**Half two — the ground-profile probe, which is `[SET]` and is not a perceptual metric.** Take a frame at
`pitch=0`, where ground distance follows `d = eyeHeight · f / (y − horizonY)`. Compute high-frequency
energy — the mean absolute difference of adjacent columns — per image row, and plot it against `d`. **The
curve must be monotone and smooth: no ratio above ~2 between adjacent samples, and no step that coincides
with a fade distance.** Do the same for saturation and for mean luminance.

> **`[SET]`, explicitly.** The factor **2** is a setting, not a measurement and not a published
> threshold. **No established measure of LOD-transition visibility was found in the LOD literature** —
> Hoppe, Ulrich, Ponchio and Karis all report τ in pixels and then judge popping by eye or by video.
> The ratio survives because it does a job FLIP cannot: FLIP compares two images and says *whether* they
> differ; this probe reads one image and says *where along the depth axis* the ladder is discontinuous.
> It is a **diagnostic, not an acceptance gate.** FLIP is the gate.

## State

**Nanite half 1 is built and measured: `render/ClusterDag.h`, on terrain AND on buildings.** Half 2 —
the compute software rasteriser — is not built and cannot be (WGSL §6.2.8, no 64-bit atomic).

What exists: a cluster DAG builder (Morton partition into 128-triangle clusters, groups of 4, QEM
half-edge collapse to 50 % per group with the group's outer edges LOCKED, re-split, repeat) plus the
per-cluster cut `sse_px = err_m · f_px / (d − r) ≤ τ < parent's`. Every cluster stores TWO spheres and
TWO errors — its source group's and its destination group's — so siblings switch on the same frame.
The cut runs on the CPU per cluster per frame and merges neighbouring selected clusters into one draw;
the terrain bundle re-records only when the cut's structure changes.

**τ = 1.0 px, in ABSOLUTE pixels.** `[SET]`, `FB_TAU` overrides it for measurement. No dither, no
crossfade, no geomorph anywhere — sub-τ selection is what pays for them.

**The lock is on the VERTEX, not only on the edge, and the edge rule alone was a wild write.** Two
closed OSM prisms that meet at a single welded corner make a bowtie: inside each group every edge at
that corner is still shared by two triangles, so the edge rule left it free, group A collapsed it and
group B kept it as a collapse TARGET. `dag::Absorb` is one structure over the whole mesh, so B then
wrote through a dead entry's tail of −1. Measured on the demo scene's building field: **6 such merges
at level 0, 8 by level 3**, on the 48-group mesh only; the terrain tiles (4 groups) had none. In WASM
that is `memory access out of bounds` during `ClusterDagBuild` and the client stops — native never
noticed. Locking every position another group also uses is what the crack argument needed anyway and,
on a manifold surface, locks exactly what the edge rule already locked: measured, the demo scene's
triangle counts are **unchanged** (terrain 51 054, buildings 17 024, shadow 8 550).

**`err_m` is a BOUND, and that took a second builder.** The stored error is the **maximum deviation of
the level-0 surface from this level's**, measured — Ulrich's vertical deviation where the surface has a
vertical (terrain, `ClusterDagOpts::Up`), the nearest point on the simplified surface where it does not
(buildings, whose walls no vertical ray meets). A half-edge collapse never moves a vertex, so every
position in every level is an original position and the ones that vanished are exactly what has to be
measured; an intrusive list per representative (`dag::Absorb`) keeps that set, and a bucket grid over
the two widest axes (`dag::DevMesh`) makes the query O(1). It is measured against the **whole level**,
not the group: a group's locked boundary vertex stands for positions its own triangles stopped covering
when the grouping shifted, and measuring those against the group alone reports the distance to the
nearest edge instead of the deviation.

The QEM residual is still what ORDERS the collapses, which is the only thing Garland-Heckbert derived
it for. It is no longer what is stored, because **it is not a bound** — measured on a synthetic height
field, the true vertical deviation ran up to **2.8× the reported error** and the ratio did not settle
with amplitude (32² field, amplitudes 0.01/1/40 m: L1 1.91×, L2 2.13×, L3 2.57×, L4 1.23× at every
amplitude). That is the size Karis's „< 1 pixel" claim and this file's TAA argument both rest on, and
it was not the size being limited. Now it is:

| level | reported | true max vertical | ratio | worst single cluster |
|---|---|---|---|---|
| L1 | 0.7380 m | 0.7380 m | **1.000** | **1.000** |
| L2 | 2.1313 | 2.1313 | **1.000** | **1.000** |
| L3 | 3.4529 | 3.4529 | **1.000** | **1.000** |
| L4 | 13.0719 | 13.0719 | **1.000** | **1.000** |

32² height field, amplitude 40 m; identical at 0.01 m and 1 m and on a 64² field to six levels. The
bound is not merely conservative, it is **tight**, and the per-cluster column is the stronger of the
two — no cluster's own region deviates more than the error that cluster carries.

**Correctness is proved by coverage, not by eye.** A probe rasterises the selected cut into the XY
plane and counts covers per sample. Over a 32² and a 64² height-field tile, at ten distances from 50 m
to 25.6 km: **0 holes and 0 overlaps in 200 704 samples per case**, and 0 monotone violations over
every cluster. Three defects were found that way and all three are fixed in the file:

* The **error was area-weighted**, so its square root scaled with triangle size — a 6 m height field
  reported 40 m and the ladder never left level 0. `QDist` divides by the accumulated plane weight,
  and it is now the collapse order only (above).
* **Monotone error is not enough.** `sse` carries the radius too, so a parent with a tighter sphere can
  project a smaller error than its child and the cut crosses twice: 175 doubly-covered samples out of
  200 704, until each group's sphere was forced to contain its children's.
* **The fold-over test needed a quality floor.** A sliver's normal is near-horizontal on a height
  field, so a later collapse flipped it with `dot = +1026` (|n| 27.6 → 551.4) and 550 m² of one tile
  ended up covered twice. Rejecting any new triangle under `4·area²/longestEdge⁴ = 0.10` `[SET]` takes
  the fold to zero — and costs depth: the 64² tile now floors at 1138 triangles instead of 254.

**Nothing outside the picture is drawn.** The cut is now preceded by a frustum test — five planes read
out of the frame's own MVP (`render/Frustum.h`, Gribb/Hartmann; the near plane is `w − z ≥ 0` under
reversed Z and there is no far plane, the projection being infinite), per tile against its vertex bound
and then per cluster against the sphere the DAG already stores. `FB_CULL=0` disarms it on the same
binary. **The picture is byte-identical with it on and off** — 0 of 921 600 pixels, at four standpoints
(1.7 m pitch 0, 1.7 m pitch +20, 200 m pitch −30, 2000 m pitch −60).

It found a defect it did not cause: the **no-DAG fallback root cluster carried no bounding sphere**
(centre 0, radius 0), so with `FB_DAG=0` the per-cluster test read a point at the tile origin and
deleted the tile under the camera. Every cluster now carries a sphere, including the degenerate roots
(`Render::BoundingSphere`).

**Measured on the reference scene (`mods/demo/scene.json`), 1280×720, Apple A18 Pro / Mac17,5, 5 GPU
cores, macOS 26.4.1, Dawn/Metal, min-of-6 over 60 frames, THE REAL FRAME** — measured while the frame
still carried the blade geometry, with CSM, AO and the metered exposure all on, because a saving that
only exists under `FB_GEOM` is not a saving. One binary, two switches; `FB_DAG=0 FB_CULL=0` was
byte-identical to the build before any of this existed:

| `FB_DAG` `FB_CULL` | terrain tris | terrain draws | building tris | shadow tris | ms/frame |
|---|---|---|---|---|---|
| 0 · 0 — before | 299 520 | 130 | 24 166 | 96 664 | **5.373** |
| 1 · 0 — ladder only | 126 496 | 287 | 24 166 | 96 664 | **4.969** |
| 1 · 1 — ladder + cull | **51 054** | **120** | **17 024** | **8 550** | **4.763** |

Terrain **5.9× fewer triangles** than the flat mesh and **−11.4 % frame time**; the cull alone is
−0.206 ms of that, and −0.153 ms under `FB_GEOM=1` (1.440 → 1.287 ms), where the frame is nothing but
geometry. Of 130 resident tiles **53 are in the picture**, which is what a 91.5° horizontal field over a
tile ring predicts.

**The ladder is 2.4×, not 3.0×, and that is the honest number.** The bound above costs triangles: at
eye level the cut is 126 496 where the unbounded residual gave 99 968, **+26.5 %**. The picture moves
in the direction of correct — 0.43 % of pixels differ, mean 3.5/255 over those, and they are the far
ridges gaining detail.

**The quality/cost knob behaves** (eye 5 km, pitch −25, each against the flat frame):

| τ px | triangles | ms | mean abs Δ vs flat |
|---|---|---|---|
| — (flat) | 252 262 | 1.791 | 0 |
| 0.25 | 173 414 | 1.518 | 0.250/255 |
| 0.5 | 126 216 | 1.354 | 0.554/255 |
| **1.0** | **89 434** | **1.256** | **0.888/255** |
| 2.0 | 67 924 | 1.214 | 1.043/255 |
| 4.0 | 58 574 | 1.205 | 1.251/255 |

Frame time saturates past τ = 1: beyond it the terrain is no longer what the frame pays for.

**The ground-profile probe finds NO new discontinuity.** Same frame, same probe, `FB_DAG` 0 vs 1 at
eye 1.7 m and pitch 0: the two high-frequency-energy curves agree to the fourth decimal at every depth
sample, and the worst adjacent ratio is **2.40 at d = 101 m in BOTH**. That 2.40 is over the `[SET]`
gate of ~2 and it is **not this ladder's** — it is identical with the DAG disarmed, and it belongs to
the ground albedo, one step later in [`../goal.md`](../goal.md) §2's order.

**And that attribution held.** Re-measured after the ground-shader round on the same reference frame
(probe restated: mean absolute 3-tap horizontal high-pass per row, normalised by the row mean, 4-row
bands, columns 200–1080, d = 3.0…133 m) the worst adjacent ratio falls from **4.65 at d = 88 m** to
**1.72 at d = 33 m** — under the gate, with the LOD ladder untouched
([`stages/terrain.md`](stages/terrain.md) `## State`).

**Buildings carry the same mechanism and it does not pay yet, and the reason is structural.** The DAG
builds (24 166 → 12 498 → 8 592 → 7 946 triangles, 2.2× the vertex memory) but its first coarse level
carries **7.91…10.98 m of error**, which at τ = 1 px is admissible only beyond **4.9…6.8 km** — and
the field is about 3 km across, so from inside it every building draws at level 0. From the air it does
fire: at 12 km the building mesh falls 72 498 → 26 022 vertices (−64 %) with no visible silhouette
change. The cause is that an **extruded prism has no interior geometry**: the only collapsible vertices
are the roof ring, and removing one moves a wall top by metres. The building ladder that would pay is
footprint decimation BEFORE extrusion — the "massing" row of the table above — and that is a 2D
operation a mesh DAG cannot express.

**Costs, all measured:**

* Vertex memory: terrain 6 912 → 12 786 vertices per tile (**1.85×**, 28.7 MB → 53.2 MB over 130
  tiles); buildings 72 498 → 159 606 (**2.2×**, 2.32 MB → 5.11 MB). Every level is resident because
  there is no virtual-geometry streaming; the tile quadtree is what bounds it.
* Build time: **~4.4 ms per tile** on the main thread (130 tiles ≈ 570 ms across the boot). The
  simplifier is still the cost, not the rasteriser; the measured bound is **+0.51 ms of it** — paired
  on one binary over a 2 312-triangle field, min-of-20: 3.34 ms without the measurement, 3.85 ms with.
* The skirt curtain becomes **a third of the drawn terrain budget** (256 fixed triangles per tile
  against 512 at the coarse levels), because the ladder does not touch it.

## Gaps

- **The DAG build sits on the main thread and costs ~4.4 ms per tile.** With a budget of two builds per
  `World::Update` that is a ~8.8 ms spike in a streaming frame — a dropped frame in the browser at
  60 Hz. It belongs in the tile worker, which already owns meshing; the ABI now carries `gridverts`, so
  what is missing is the cluster table across `postMessage`.
- **The bound is measured for terrain and for buildings, and only PROVED for terrain.** The
  reported-vs-true table is a height field with a vertical; the building path takes the nearest point
  on the simplified surface instead, and no harness measures that against a prism. The error it
  reports (L1 7.9…11.0 m) is plausible for a collapsed roof ring and unverified.
- **A merged draw range now breaks where the frustum cuts it.** Terrain went 130 draws → 120 for 53
  tiles, so a visible tile costs 2.3 draw calls: its cut spans levels, and levels are not contiguous in
  the vertex buffer. The cull did not make that worse (2.04 → 2.26 per tile) but it did not fix it.
- **The error metric bounds POSITION only, not the normal.** τ = 1 px of geometry can still move
  shading by more than a pixel's worth on a low-relief surface under a 4.6° sun: at 12 km, 24.4 % of
  pixels differ by more than 2/255 from the flat frame (mean 1.27/255, max 64). Karis folds attribute
  error into the quadric with weights and says outright there is no theory for it; this file folds
  nothing yet.
- **The sliver quality floor costs simplification depth.** `4·area²/longestEdge⁴ ≥ 0.10` is what takes
  the fold-over to zero, and it stops a 64² height-field tile at 1138 triangles where the unguarded
  simplifier reached 254. Epic terminate every LOD graph at ONE 128-triangle cluster; on a height field
  we are 8.9× off that and the guard is the reason.
- **The cut is CPU-side, per cluster, per frame.** ~4 200 clusters over 130 tiles is free today, but it
  is not the GPU-driven two-pass cull Karis describes and it will not carry a city.
- **The skirt is a fixed cost the ladder cannot reduce**, and it is now a third of the terrain budget.
  It exists for the seam between tiles at different quadtree levels, which the cluster DAG does not
  address — the DAG is crack-free WITHIN a tile only.
- **`FB_TAU` has exactly ONE owner now, `render/ClusterDag.h`.** The second ladder — a stand of blades
  drawn to `min(RadiusM, MaxWidthM · fPx / τ)` and faded per blade on its own width in pixels — went
  with the geometry it selected, and with it the last consumer of τ outside the cluster cut. **Sprites
  and the ground-detail octaves are still distance-declared**, which is the formulation this file rules
  out, and they are what is left to fix.
- **REJECTED, with the measurement: a declared fade RADIUS for a stand.** High-frequency energy 0.39 at
  44.2 m → 9.14 at 37.9 m → 13.45 at 33.1 m on `weser_meadow_yaw45.png` — a factor of **23 across 8 m**
  — is what a disc with a declared radius does to the ground profile. The screen-space rule that
  replaced it is the one this file mandates; the geometry both applied to no longer exists
  ([`stages/terrain.md`](stages/terrain.md) `## Gaps`).
- **Motion vectors for wind-animated foliage are unbuilt again, and their cost is on record.** Built
  for the grass on 2026-08-07 and deleted with it: the analytic half was as easy as it looked on paper
  — a vertex's previous position is the same vertex function at the previous wind phase, which the
  uniform already carried — but the second evaluation of the bending equation per vertex measured
  **12.23 ms of a 103.6 ms frame** and bought an **18 % reduction in temporal lag** at the declared
  6 m/s ([`stages/taa.md`](stages/taa.md)). Nothing in the frame moves today, so nothing owes a
  velocity; the wind-displaced trunk/branch geometry that will is still unbuilt
  ([`vegetation.md`](vegetation.md)).
- **TAA is now a fact and not an assumption, so τ may be chosen.** This file's whole selection rule
  rests on the sub-τ discrepancy being „the class of error TAA is built to absorb"; the premise was
  unbuilt when the rule was written. It is built, its lag is measured at 0.148 frames on a 3.8 px/frame
  pan, and its aliasing floor is measured at 0.956× of a 16× supersampled truth — so the noise a real
  LOD step has to be visible ABOVE is now 0.085 mean \|Laplace\| in the 8–15 m band instead of 0.240.
- **LOD-dependent damping of vertex animation has no source.** Searched Karis et al. 2021, the UE5
  Nanite documentation and GPU Gems 3 ch. 16 (Sousa 2007, the canonical vegetation-wind reference).
  Sousa establishes the two-band structure (main bending + detail bending) and gives frequencies, and
  says **nothing** about attenuating a band with distance or LOD. The proposal above is therefore an
  unsourced design and must be measured, not cited.
- **No FLIP threshold is set.** FLIP is adopted as the popping gate but the LOD literature publishes no
  pass/fail value, so none is invented here. Deriving one is a measurement task: run a scripted fade
  with the transition enabled and frozen, FLIP both pairs, and set the gate from the frozen pair's
  response.
- **Rejected, with the measurement: naive per-frame stochastic dithering.** Wyman & McGuire (I3D 2017)
  report that replacing the alpha threshold with a fresh random value per frame causes *„continuous
  twinkle"* that TAA does not remove, and cost it at 3–4× traditional alpha testing (European Beech
  386 k tris: 1.69 ms vs 0.39 ms = 4.3×; UE3 FoliageMap 3 000 k: 11.42 ms vs 2.52 ms = 4.5×). The
  working form is **hashed** — object-space anchored, discretised in the screen-space derivative — at
  0.50 ms and 2.86 ms respectively. `## Knowledge` records the conditions.
- **Named-hard, from the source that names it: hashed alpha's stability fails on instanced foliage.**
  Wyman & McGuire: *„All our stability improvements disappear if coordinate frames become sub-pixel …
  it is vital to use coordinates consistent over an entire aggregate surface (e.g., a tree) rather than
  a portion of the object (e.g., each leaf)."* And in their own future work: *„hash inputs more
  sophisticated than object-space coordinates may generalize over a larger variety of highly instanced
  scenes."* Our leaf cards are per-leaf instances, which is the failure case verbatim. **Whether the
  hash can be anchored to the tree instance rather than the card is unmeasured**, and it is the single
  cheapest experiment in this file.
- **Wind and hashing interact and nobody has stated how.** The hash must be anchored in *object* space
  (world space *„fails on dynamic geometry"*), but the wind displacement is applied in the vertex
  shader. Hashing the **undeformed** object coordinate should preserve stability; hashing the displaced
  one should make the noise swim. Untested.
- **`atomic_vec2u_min_max` is a draft, not a feature.** Nanite half 2 waits on
  [gpuweb#5071](https://github.com/gpuweb/gpuweb/issues/5071). The 32-bit fallback (Z-prepass +
  `atomicMin` on 32-bit depth, then a second resolve pass with `atomicCompareExchangeWeak`) is
  **available and unmeasured** — `Scthe/nanite-webgpu` names it and rejects it as *„much slower"*
  without publishing a number.
- **Nanite's own simplification quality is the hidden cost, and it is quantified by its clone.** Karis
  claims (2021, slide 95) that every UE5 LOD graph terminates in a single 128-triangle root cluster.
  `Scthe/nanite-webgpu`, built on stock meshoptimizer + METIS, reached 44 k → 3 k on its test model —
  **a coarse level 23× heavier than Nanite's floor**, which is why that project leans on impostors
  where UE5 does not. Adopting half 1 buys the DAG; it does not buy Epic's simplifier.
## Knowledge

### Screen-space error: the threshold τ and which error it bounds

`sse_px = size_m · f_px / d`, `f_px = 0.5·height / tan(0.5·vFov)`. Worked for this file's own example, at
720p and 60° vFov: `f_px = 360 / tan(30°) = 360 / 0.57735 = 623.5 px`. A 1 cm pebble at τ = 1 px survives
to `0.01 · 623.5 / 1 = 6.2 m`; a 10 cm cobble to 62 m.

**τ ≈ 1 px is the literature's value, and each source states the error it bounds:**

| Source | τ shipped | Error bounded |
|---|---|---|
| Hoppe 1997 (SIGGRAPH), *View-Dependent Refinement of Progressive Meshes* | 0.075 % / 0.15 % / 0.25 % / 0.33 % of window = 0.45 / 0.9 / 1.5 / 2 px at 600²; regulated runs floored at **0.5 px** | Hausdorff distance `H(N_v, N̂_v)`, generalised to an anisotropic deviation space `D_n̂(μ,δ)` whose screen projection **vanishes along the view axis and peaks across it** — hence more refinement at silhouettes |
| Hoppe 1998 (Visualization), *Smooth View-Dependent LOD Control … Terrain* | *„near a value of 1 pixel"* | as above; geomorphs let the tolerance rise to *„a few pixels"* |
| Ulrich 2002 (SIGGRAPH Course), *Chunked LOD* | *„around 2-pixel … at a resolution of 1024×768"*, from a 4 m object-space tolerance | max vertical deviation per chunk |
| Ponchio 2008 (PhD thesis), §3.6.1, §4.3 | **1 px** (Puget Sound, 800×600); **3 px** (Mars, 640×480) | projected model-space error λ, evaluated conservatively at the nearest point of a bounding sphere |
| Karis et al. 2021 (SIGGRAPH Advances), *Nanite* | **< 1 px** | Quadric Error Metric (Garland & Heckbert 1997), **position and attribute error mixed with weights** — Karis: *„There is no theoretical foundation for this. The only defense for it is experimentally."* |

**Two facts this table settles that memory had wrong:**

* **The error is not one thing.** A vertical scalar (terrain lineage), an anisotropic silhouette-aware
  deviation space (Hoppe 1997) and a QEM residual with attributes folded in (Nanite) are three different
  quantities all reported „in pixels". A τ is meaningless without naming which.
* **τ is not resolution-free by nature — it is by convention.** Hoppe expresses τ as a *fraction of
  viewport*; this file holds it in absolute pixels. `[SET]`, and it is the choice that makes the
  „resolution-independent" bullet true.

**Nanite's software-rasteriser thresholds**, for reference if half 2 is ever built: clusters whose
triangle edges are **< 32 px** go to the software path; within it a scanline variant is chosen when the
inner X loop exceeds **4 px** per triangle in the wave.

### Why TAA absorbs an LOD change, and the three conditions

Karis et al. 2021: *„if we only draw clusters that are less than 1 pixel of error they are imperceptibly
different and temporal antialiasing smoothes out any change. TAA is built to blend subpixel differences
over time. It does our work for us so long as the error is subpixel."* Nanite therefore ships **no**
geomorphing and **no** cross-fading — those were considered and rejected as *„expensive at render time
or require significant additional data or both."*

The cut is guaranteed unique because the DAG build **forces the error function monotone along every
root-to-leaf path** — a parent's stored error and bounds are raised to at least its children's. Without
that there is no single crossing and no crack-free neighbourhood.

**Where the dither route is unavoidable, Wyman & McGuire (I3D 2017) give the conditions, all three
required:**

| Condition | Their words | Why it binds here |
|---|---|---|
| anchor the hash in **object** space | *„Hashing world-space coordinates provides stable noise for static geometry … However, this fails on dynamic geometry. Object-space coordinates give stable hashes for skinned and rigid transforms and dynamic cameras."* | our foliage is wind-displaced in the vertex shader; the hash must read the **undeformed** coordinate |
| discretise by the **screen-space derivative**, including along z | normalise `objCoord` by `max(length(dFdx), length(dFdy))`, floor it, and interpolate between the two nearest log2 scales through the CDF of two interpolated uniforms — otherwise motion along the view axis re-randomises τ every frame and *„strobing"* appears | a flight from 12 000 m to the deck is continuous z-motion, which is the exact failure mode |
| noise **below** pixel scale when temporally accumulating | *„when temporal antialiasing, using noise below pixel scale (e.g., 0.3–0.5) allows for temporal averaging"* — the parameter is `g_HashScale`, default 1.0 | at 720p30 this is the setting that decides whether TAA converges or smears |

And the stated failure boundary, which is our densest layer: *„All our stability improvements disappear
if coordinate frames become sub-pixel … it is vital to use coordinates consistent over an entire
aggregate surface (e.g., a tree) rather than a portion of the object (e.g., each leaf)."*

Measured cost, 1920×1080 on a GTX 1080, traditional → hashed → stochastic alpha test:

| Scene | Traditional | Hashed | Stochastic |
|---|---|---|---|
| Bishop Pine, 158 k tris | 0.22 ms | 0.30 ms | 0.75 ms |
| European Beech, 386 k tris | 0.39 ms | 0.50 ms | 1.69 ms |
| UE3 FoliageMap, 3 000 k tris | 2.52 ms | 2.86 ms | 11.42 ms |

Derived from those three rows: hashed costs **+13.5 % to +36.4 %** over a fixed threshold and is
temporally stable; unhashed stochastic costs **3.4× to 4.5×** and flickers. The overhead falls as the
scene grows, which is the direction that suits us.

**Why one sample is not enough without the hash** — Enderton, Sintorn, Shirley & Luebke, *Stochastic
Transparency*, I3D 2010, the paper Wyman & McGuire's method reduces to at one sample: a random sub-pixel
stipple proportional to alpha gives *„the correct alpha-blended color on average, but introduces noise"*,
and their own figures show the noise still plainly visible at **8 and 16 samples per pixel**, converging
near **64**. At 720p30 there is no budget for 8×, let alone 64×; the hash buys spatial stratification and
temporal reuse instead of samples, which is precisely why it is the affordable form.

### Nanite: the numbers, if half 1 is adopted

From Karis et al. 2021, measured on their demo scene:

| Quantity | Value |
|---|---|
| compressed disk footprint | **5.6 bytes / Nanite triangle**, **11.4 bytes / input triangle** |
| per million triangles | **~10.9 MB on disk** |
| demo scene | raw 25.90 GB → memory format 7.67 GB → compressed disk 4.61 GB |
| always-resident root data | **40.5 KB per mesh** |
| cluster / page granularity | ~2 KB per cluster, 128 KB pages, ~1 % slack |
| streaming transcode | ~50 GB/s on PS5, *„fairly unoptimized"* |
| platform floor (UE5 docs) | **DX12 Shader Model 6** — desktop/console, not mobile |
| instance ceiling (UE5 docs) | 16 million, counting all streamed-in instances |

Against [`visual-target.md`](visual-target.md) §1's 4 GB the resident cost scales with **distinct
meshes**, not instances: a few hundred distinct assets is tens of MB. The streamed pages are the
variable, and they are bounded by the same τ that bounds everything else.

### Sources

| # | Source |
|---|---|
| 1 | Hoppe, *View-Dependent Refinement of Progressive Meshes*, SIGGRAPH 1997 — https://hhoppe.com/vdrpm.pdf |
| 2 | Hoppe, *Smooth View-Dependent Level-of-Detail Control and its Application to Terrain Rendering*, IEEE Visualization 1998 — https://hhoppe.com/svdlod.pdf |
| 3 | Hoppe, *Progressive Meshes*, SIGGRAPH 1996 — https://hhoppe.com/pm.pdf |
| 4 | Ulrich, *Rendering Massive Terrains using Chunked Level of Detail Control*, SIGGRAPH 2002 Course — http://tulrich.com/geekstuff/chunklod.html |
| 5 | Ponchio, *Multiresolution structures for interactive visualization of very large 3D datasets*, PhD thesis 2008 — https://vcg.isti.cnr.it/~ponchio/download/ponchio_phd.pdf |
| 6 | Cignoni et al., *Batched Multi-Triangulation*, IEEE Visualization 2005 — http://publications.crs4.it/pubdocs/2005/CGGMPS05a/ieeeviz2005-gpumt.pdf |
| 7 | Karis, Stubbe & Wihlidal, *A Deep Dive into Nanite Virtualized Geometry*, SIGGRAPH 2021 Advances in Real-Time Rendering — https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf |
| 8 | Epic Games, *Nanite Virtualized Geometry* (engine documentation) — https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine |
| 9 | Wyman & McGuire, *Hashed Alpha Testing*, I3D 2017 — https://casual-effects.com/research/Wyman2017Hashed/Wyman2017Hashed.pdf |
| 10 | Enderton, Sintorn, Shirley & Luebke, *Stochastic Transparency*, I3D 2010 — NVIDIA / Chalmers |
| 12 | Andersson et al., *FLIP: A Difference Evaluator for Alternating Images*, Proc. ACM Comput. Graph. Interact. Tech. 3(2), 2020 — https://research.nvidia.com/publication/2020-07_FLIP |
| 17 | Sousa, *Vegetation Procedural Animation and Shading in Crysis*, GPU Gems 3 ch. 16, 2007 |
| 19 | Garland & Heckbert, *Surface Simplification Using Quadric Error Metrics*, SIGGRAPH 1997 |
| 21 | gpuweb, *64 Bit atomics (storage buffers)* draft proposal — https://github.com/gpuweb/gpuweb/issues/5071 |
| 22 | Scthe, *nanite-webgpu* — https://github.com/Scthe/nanite-webgpu |
| 23 | Kapoulkine, *meshoptimizer* — https://github.com/zeux/meshoptimizer (PR #704 `meshopt_simplifySparse`, discussion #750) |

The numbering is the one [`visual-target.md`](visual-target.md) started; the gaps in it are the rows that
went to another file, so a number means the same paper everywhere in `doc/render/`.
