# Vegetation — 256 templates and the 0–40 m stack

**Origin:** §5 of [`visual-target.md`](visual-target.md), split out because it is a subsystem and not a
pass: one template drives several stages. Neighbours: [`classification.md`](classification.md) (what
selects a template), [`stages/terrain.md`](stages/terrain.md) (the ground fragment, which is the only
consumer of a template today), [`lod.md`](lod.md) (the one ladder vegetation keeps of its own), and
[`../body-format.md`](../body-format.md) §1.1 (wind as MEDIUM + JOINT + `surface`, which is where a grown
plant's motion belongs once the body format carries it).

## Spec

### 256 templates from albedo

> Owner: *„wenn dir der grobe albedo reicht kannst du den auf 8 bit komprimieren und bekommst 256
> vegetationstemplates … müssen dann natürlich fliessend übergehen."*

Quantise the coarse albedo triple to an 8-bit index into 256 vegetation templates. A template declares a
species mix, densities, ground cover and clutter — not a texture.

- **Blend the distributions, not the images.** Interpolating species mix and density across a boundary
  leaves no seam; interpolating textures leaves a visible one.
- **Latitude and elevation are a plausibility filter** on the index. Albedo alone is seasonal — a winter
  photograph would otherwise make a deciduous forest permanently snowbound.
- **Growth, not assets.** `~/Git/wasm-tree` is not an idea, it is a working library: trees grown from JSON
  parameters as watertight meshes with instanced leaf cards, venation normal maps, hierarchical wind and
  an octahedral-impostor LOD ladder. Its `src/core/` is deliberately **GL-free** (~1500 lines:
  `mesh_grow`, `leaf_gen`, `mesh`, `json`, `vmath`) and transfers as-is; its `src/render/` is GLES3 and
  does not.
- **Its sixteen species transfer too.** They are Central European — ahorn, birke, buche, eberesche,
  eibe, eiche, esche, fichte, hainbuche, kastanie, kiefer, linde, saeulenpappel, tanne, trauerweide,
  ulme — which was too narrow while the target was a worldwide flight sim. The target is
  **Hameln on the Weser**, and that is exactly the Weserbergland list: beech/oak/hornbeam/ash on the
  slopes, spruce/pine in the plantations, willow at the water, lime/chestnut/maple in the streets.

### The stack, 0–40 m

**The 0–2 m layer does not exist anywhere and it is the one a pedestrian sees.** A template must
therefore declare a **stack**, not a flat species mix:

| Layer | Height | Method | Exists |
|---|---|---|---|
| ground cover — moss, leaf/needle litter, ivy | 0–0.1 m | terrain material detail from the template ([`stages/terrain.md`](stages/terrain.md)) | **as a material**, blended by the relief field |
| **grass / herb** | 0.1–0.8 m | **an aggregate term in the ground fragment, never geometry** ([`stages/terrain.md`](stages/terrain.md)) | **yes, as a fragment term** |
| perennials | 0.3–1.5 m | `mesh_grow`, small parameters | no |
| shrubs | 1–4 m | `mesh_grow`, small parameters | no |
| trees | 4–40 m | `mesh_grow`, sixteen species ready | **the library, not the call** |

**Perennials and shrubs need no new machine** — structurally a perennial is a very small tree, so
`mesh_grow` already grows one given a small `height_m`, few `trunk_steps`, high `branch_chance` and
`bare_steps: 0`. Those are JSON files, not code.

**Nothing below the size of a tree is geometry** ([`../goal.md`](../goal.md)), so the two lowest rows of
this stack are shading and the three above it are meshes. **Trees are the next layer**, and they are
where the aggregate machinery has a measured precedent to answer to.

### The sward — what a meadow declares, and the rule for what counts

The reference scene stands on `wiese` at 52.106 N on **6 August, 17:40 UTC**, sun el 11.2°. That fixes
the target: `landuse=meadow` here is **working grassland**, not a conservation flower meadow — the
Arrhenatheretum elatioris (LRT 6510) in its fertilised, species-poor form, and in August it is
**Grummet**, the second growth about six weeks after the June cut. So: a closed 0.25–0.45 m leaf mass,
a *scatter* of re-emerged culms at 0.40–0.60 m, and no flat panicle horizon (that went with the first
cut).

**The rule that decides which defects matter.** At 60° vertical FOV over 720 px one pixel is 0.0833°, so
a feature must exceed 2 px = 0.167° to survive — **8.7 mm at 3 m**. Width, curvature, tilt spread,
tussock structure, coverage, colour and shadow clear that bar. **Midrib, sheath, ligule and blade
cross-section do not**, and building them before the others is effort spent below the resolution of the
view the game is played from.

**Every quantity below is a DECLARATION, not a shape.** The stand is a fragment term, so a width and a
count are read as the two factors of a leaf area index and the angular quantities are read as the
population the derived constants of `render/Sward.h` were integrated over — `kMeanSin` over tilt and
arch, `G(el)` over tilt, arch and azimuth, `kTuftAmp`/`kTuftCells` over the tussock.

| Quantity | Target | Source |
|---|---|---|
| **blade width** | 5–10 mm (*Arrhenatherum*), 4–10 mm (*Dactylis*), near parallel-sided | Rothmaler; Burgenland-Flora |
| **length : width** | 40–55 : 1 | from the widths above |
| **tilt distribution** | erectophile, mean ~55–75° from horizontal, spread 0–90° | `[SET]` — no source found for the distribution itself |
| **blades per m²** | the template's own declaration, today `wiese.perM2 = 800`; real tiller counts are 5 000–20 000/m² | `vegetation.json` `origins."grass.perM2"` |
| **LAI** | 1–5 for meadow; Grummet ≈ 3 | Spektrum, Lexikon der Biologie |
| **vertical coverage** | 95–100 % | definition of working grassland |
| **open soil at eye height** | 0 % | dito |

**Under a meadow there is never soil.** There is thatch — dead leaf felt, stubble and moss, and it is a
ground material of its own (`grasfilz`, `## State`). It is what the aggregate's residual ground shows
where the stand's own cover falls short of 1.

| Contract | Acceptance / measurement anchor |
|---|---|
| **The declaration is the density, and code may not overrule it** | a delivered density below the template's declaration may not be paid back in WIDTH: area is bought in COUNT. Acceptance: what the fragment computes an LAI from is the template's own `perM2 × widthM`, or the template changes |
| Coverage is measured, not estimated | 1 m² rendered vertically at known calibration on the vegetation bench; the number is a fraction, not an impression |
| **The stand responds to the sun** | at el 11° the backlit stand must exceed the frontlit one: p95 by a factor ≥ 2, and its R/G must rise |
| **A sward is a volume that eats light** | the ground between the plants must be darker than the plants, not brighter. This is an analytic height function plus a density-driven darkener — **not** a shadow pass |
| Growth is clumped | tussock centre plus tillers; an evenly scattered population reads as a drilled crop. It survives as the crown rung of the canopy-top field ([`stages/terrain.md`](stages/terrain.md)) |
| **Declared weather does NOT drive the plant** | reversed by decision, 2026-08-07: nothing below the size of a tree moves ([`../goal.md`](../goal.md)). The scene keeps declaring its wind and `render/WindField.h` keeps serving it; the consumers are a branch and a rotor |

### The blade is a LINEAR REFLECTANCE, and it is one measurement for the whole file

**A blade colour is what one leaf reflects, and nothing scales it.** It lives in the space
`ground-materials.json` already uses — linear sRGB/Rec.709 primaries, D65, CIE 1931 2° — so a template
declares the same kind of number for its blade as for its floor. A *stand* of those blades is darker
than one blade, and turning the leaf into the canopy is the sward model's job (`render/Sward.h`); a
factor in the declaration that pre-pays for it is a second, silent model.

| Contract | Acceptance / measurement anchor |
|---|---|
| **One number, one meaning** | what `vegetation.json` declares for a blade is what `VegetationTemplates::Row.Grass/Dry` carries, bit for bit. No gain, no linearisation, no display bytes |
| **The leaf level is measured, not anchored** | `ground-materials.json` takes a soil's LEVEL from a field albedo and only its CHROMATICITY from a laboratory spectrum, because a prepared sample of loose grains is not the field surface. A leaf in an integrating sphere **is** the leaf, so its own spectrum carries both |
| **The canopy factor is the model's, and it is checkable** | leaf → canopy must land on an independently measured canopy of the same material. ECOSTRESS's *Green Rye grass* SOD converts to Rec.709 Y **0.0839** against the leaf mean **0.1731**, a factor 0.485; the closed-canopy limit (1−√(1−ω))/(1+√(1−ω)) at ω = ρ+τ = 0.27 gives 0.079 |
| **The blade class is shared** | one `bladeClasses` row serves every template that grows the same leaf. What separates a forest-floor sedge from a pasture grass is `perM2`, `heightM` and `dryFraction`, which the template declares |

### Alpine sward — a second grassland class, because `wiese` is a MOWN one

`landuse=meadow` is a managed hay meadow and every one of `wiese`'s numbers says so. Above the treeline
the same shape of stand is shorter, has less leaf area, carries more standing dead and stands on rock.
That is a different class and it is `alpenrasen`.

| Quantity | Value | Source |
|---|---|---|
| **LAI** | 2.7 | MEASURED — Rossini et al. 2012, *Biogeosciences* 9, 2565–2584, doi:10.5194/bg-9-2565-2012. Site IT-Tor Torgnon, 45°50′40″ N 7°34′41″ E, **2160 m**, unmanaged *Nardus stricta* grassland; destructive LAI, 12 plots of 30 × 30 cm, LI-3100. Max 2.7 (2009) / 3.0 (2010), both peaking DOY 194–201, i.e. **before** the scene's 6 August |
| canopy height | 0.12 m | derived — lower bounds of the common generative height range of the community's dominants in Kaplan et al. 2019, *Key to the Flora of the Czech Republic* (via Pladias): *Nardus stricta* 0.10, *Anthoxanthum alpinum* 0.15, *Poa alpina* 0.10 |
| blades/m² | 1165 | derived — `LAI · kMeanSin / (widthM · heightM)`, no freedom left once the three above are fixed |
| senescent share | 0.45 | `[SET]` in a bracket: above `wiese` 0.30 because the class is never cut (Rossini et al. name the site *unmanaged* and report standing yellow/dead biomass as „a significant fraction … during much of the growing season"), below `acker` 0.92 |
| substrate | `kalk` + `grasfilz` at 0.58 | REGIONAL: all six reference cameras stand in the Northern Calcareous Alps. The litter share is derived as the LAI ratio 2.7/4.64 against `wiese`'s felt-covered 1.0 |

**`natural=fell` and `natural=tundra` cannot be assigned.** shortbread 1.0's `land` layer carries only
`heath`, `scrub`, `grassland`, `bare_rock`, `scree`, `shingle` as natural covers, and neither kind
appears in 100 z14 tiles of the Alps. The loss is upstream of `fb-tiles`; a row for it here would be a
dead path. What arrives is `natural=grassland`, **3.63 %** of the mapped land in that sample.

### Ground shader and grass shader are ONE system — and now there is one scale

> Owner, 2026-08-07: *„ich denke der bodenshader und grasshader müssen sich auch 'kennen'. wenn
> geometrie nicht mehr nötig ist, wird sie zur fragmentfarbe des bodens."*

**The sward IS the ground fragment.** Not a second grass representation next to the terrain: one
declaration, evaluated as what the stand does to the light on average. The near scale — a stand of drawn
blades ending where a blade's width stopped covering τ pixels — was deleted on 2026-08-07, so the
sentence holds with its second half empty: **there is no edge to hide, because there is no boundary
between two systems.**

| Contract | Acceptance / measurement anchor |
|---|---|
| **The declaration is shared, never the result** | one `vegetation.json` row, one LAI, one leaf-angle population `G(el)`, one shared header (`render/Sward.h`) rather than two constants that must be kept equal. This binds the moment a second consumer exists — a tree's canopy is the next one |
| The aggregate carries the SAME terms, not a matching colour | aggregate albedo, occlusion, transmission lobe and angular dependence, from the declared inputs |
| It cannot be built by THINNING | dropping instances is the popping TAA cannot resolve. An aggregate drops nothing — it evaluates the same thing differently, which is why it can carry coverage at all |

**A flat stand is accepted, and the reference titles are the yardstick** —
[`stages/terrain.md`](stages/terrain.md) `## Spec` states it where the aggregate lives. So **do not buy
back stand thickness** with a geometry band at terrain edges or a height term in the aggregate; nobody
misses what Witcher 3 and Fallout 4 also lack.

### The tree generator — one mesh out of one declaration

**A species is a JSON file and nothing else.** The sixteen files of `~/Git/wasm-tree/species/` move into
`sim/assets/world/species/` **byte for byte** and are the whole input. No `.cpp` carries a species, no
species carries code; a seventeenth tree is a seventeenth file. This is Principle 2 applied to a plant.

**The generator is `sim/src/world/`, not `render/` and not `core/`.** It turns a declaration into world
geometry, which is what `BuildingField` (OSM → walls) and `TerrainLoader` (DEM → mesh) already do one
directory over. `render/` owns passes and encode order and would have to reach *up* into a producer;
`core/` is value types and the incorruptible judges, and a mesh generator is neither.

**C++17, `namespace outshine::World`, one class per file.** The prototype's five C files are not ported
— `world/terrain/` stays the only C island the tree has.

| File | What it is |
|---|---|
| `TreeVec3.h` | the growth's value type and its frame construction (RMF double reflection) |
| `TreeRandom.h` | xorshift32 seeded from the declaration — the one source of chance |
| `TreeSpecies.h/.cpp` | THE DECLARATION: growth, leaf and shading parameters parsed out of one species JSON |
| `TreeMesh.h` | THE RESULT: bark mesh, leaf mesh, **leaf points**, bounding box |
| `TreeGrower.h/.cpp` | extrusion growth: bark mesh + leaf points, buffers reused across trees |
| `TreeLeaf.h/.cpp` | the single leaf / needle shoot of a species, five kinds |
| `LeafAngleDistribution.h/.cpp` | **G(el) MEASURED at the grown tree** |

**The mesh is normalised and the metres are declared.** `TreeGrower::Grow` always delivers base at
`y = 0`, centred in x/z, **height exactly 1**; `TreeSpecies::HeightM` is the scale that makes it a tree.
The prototype reads `height_m` and `spread_m` nowhere at all — here they are the only numbers that carry
a unit, and they gain their first reader.

#### The acceptance is the reference, run

The prototype's `src/core/` is compiled natively and executed, and its output is the yardstick: **vertex
and index count of both meshes, leaf-point count and bounding box, per species, for all sixteen.** A
deviation is admissible only when it is NAMED and carries its number. „Looks like a tree" is not an
acceptance.

#### `leaf_pts` is the point of the whole thing

Each leaf point is a **position on the shoot surface and the outward direction of its stalk**. It is the
one output that is not geometry, and it is what closes the circle grass never closed:

> **The far scale must not assume a leaf-angle population the near scale does not have.** `Sward.h`
> carries `kG0/kG1/kGp` as a Monte-Carlo over a *declared* tilt distribution — an assumption. A tree
> declares no leaf angle at all; its leaf angles are the outcome of the growth. So they are **measured
> on the tree that was actually grown**, and near and far read the same three numbers.

`G(el) = E[|n·s|]` over the leaf population, same definition as `Sward.h`. The lamina rolls freely about
its stalk, so with `u` the measured stalk direction the normal is uniform on the circle perpendicular to
`u` and the expectation over that roll is closed:

```
E_roll |n·s| = (2/pi) * sqrt(1 - (u·s)^2)          (n uniform on the great circle perpendicular to u)
G(el)        = (2/pi) * mean over leaf points, averaged over the beam azimuth
```

— which needs no Monte-Carlo over an invented distribution and no leaf card. `LeafAngleDistribution`
delivers the sampled `G(el)`, the least-squares fit `G = g0 + g1·sin(el)^p` in `Sward.h`'s own form with
its residual, and the stalk-elevation histogram the fit came from. **This round measures it and no
consumer reads it yet** — the consumer is the tree's own canopy shading, one layer on.

#### The crown carries the declared leaf area index, and it buys it in COUNT

The species declares `lai` (one-sided leaf area per m² of crown projection, measured by forestry) and
`leaf_card_h` (its leaf's own length in metres, botany). Those two plus the **grown** crown projection
fix the only free quantity there is — how many laminae one attachment point carries:

```
laminae_total      = lai * crownProjM2 / laminaAreaM2
laminae_per_point  = laminae_total / leafPoints
```

**Area is bought in count, never in leaf size** — the same rule the sward's density contract states one
section up. A crown that pays its index by growing the leaf draws a beech leaf the size of a plate.
Paying in count leaves exactly one number free to judge the growth by:

| Acceptance | Anchor |
|---|---|
| `lai` at the subject bench equals the declaration | it does by construction; the measurement that means something is the one below |
| **`laminae_per_point` between 1 and 5** | a shoot point stands for one leaf cluster. A large number is a crown with too few shoots and it shows in the picture as rosettes on a bare skeleton |
| the crown hides the trunk from every direction but from below | measured as green coverage of the crown's own box in the backlit silhouette |

**A shoot without a tube is still a shoot.** `pixelHeightFrac` decides which shoots get bark; it may
decide **nothing** about where a shoot goes, whether it branches, or how many leaves it carries. The
leaf-point cloud is therefore the same at every rank, and it — not the bark — is what the bounding box
and the tree's height are measured over.

**The vertex count is a budget that is SOLVED, not a ceiling that cuts.** Truncating the growth queue
drops whichever tips the breadth-first order reached last, which is the outermost shoots — the crown
itself. Coarsening `pixelHeightFrac` until the mesh fits drops tubes instead.

#### What this round is NOT

No placement, no LOD, no impostor, no wind, no leaf cards, no scene. `leaf_card_*` and `leaf_droop` are
parsed because they stand in the declaration, and read by nothing. One mesh out of one declaration.

### Vegetation keeps its own LOD ladder

Judged in [`lod.md`](lod.md) and recorded here because it is the exception to „one ladder for
everything": a cluster-DAG does **not** subsume vegetation. Karis names aggregates as the case where the
cost model itself breaks, and Epic's own answer to foliage is a separate system with voxelisation in it.
`~/Git/wasm-tree`'s impostor ladder is therefore not made redundant.

## State

**The tree generator is built, and it reproduces the prototype BIT FOR BIT.** `sim/src/world/`:
`TreeVec3.h` · `TreeRandom.h` · `TreeSpecies.{h,cpp}` · `TreeMesh.h` · `TreeGrower.{h,cpp}` ·
`TreeLeaf.{h,cpp}` · `LeafAngleDistribution.{h,cpp}`, C++17 in `namespace outshine::World`, linked into
`walk` and `wasm` alike (`TREE_SRCS` in the Makefile). The sixteen species files sit **byte-identical**
under `sim/assets/world/species/`. `make treebench` builds the bench; nothing in the frame calls the
generator yet.

**The acceptance, measured 2026-08-07 against `~/Git/wasm-tree/src/core/` compiled natively and run:**
all sixteen species, five buffers each (bark verts, bark indices, leaf verts, leaf indices, leaf points)
— **80 of 80 buffers byte-identical: 2 159 272 floats and 996 096 indices, 12.6 MB, zero deviations.**
Vertex/index/leaf-point counts and the bounding box therefore agree to the last digit and no deviation
needed naming.

**That identity was given up on 2026-08-08, on purpose, and the table below is what it cost.** The
prototype's declarations grew a winter skeleton: `min_radius` stood at `base_radius / 16`, so the finest
shoot the grower would make was a **3 cm branch**, and `TreeGrower::SpawnLateral` refuses any child with
`order_radius * r <= min_radius`. Growth therefore stopped two orders below the declared `max_order` —
measured on buche, rank 0: **order 3 had 8 tips and order 4 none, out of a declared four orders**, and
the crown consisted of 166 shoots. The consequence at the subject bench was
**`lai` 0,391 against the declared 6,0** and a silhouette that was **1,5 % leaf** by green coverage of
its own crown box. The prototype is no longer the yardstick for the shoot system; it remains it for the
leaf mesh, the RMF frame and the bark topology, none of which changed.

**The sixteen at rank 0 (`pixelHeightFrac` = `TreeStage::RankPixel(0)` = 1/4096 of the tree's own
height), measured 2026-08-08, `make treebench --pixel 0.000244140625`, native `-O2`:**

| species | leaf points | laminae | laminae/point | lai built | lai declared | bark tris | grow ms |
|---|---|---|---|---|---|---|---|
| ahorn | 66 983 | 302 246 | 4.51 | 5.50 | 5.5 | 101 276 | 46.3 |
| birke | 124 826 | 253 100 | 2.03 | 3.50 | 3.5 | 93 562 | 12.7 |
| buche | 129 070 | 325 833 | 2.52 | 6.00 | 6.0 | 144 484 | 34.9 |
| eberesche | 17 866 | 40 600 | 2.27 | 3.50 | 3.5 | 30 514 | 5.8 |
| eibe | 29 292 | 583 932 | 19.93 | 7.00 | 7.0 | 53 934 | 7.0 |
| eiche | 5 978 | 782 920 | 130.97 | 5.00 | 5.0 | 26 664 | 2.6 |
| esche | 24 311 | 261 364 | 10.75 | 5.00 | 5.0 | 44 162 | 4.6 |
| fichte | 76 503 | 1 000 000 | 13.07 | **1.13** | 8.0 | 91 958 | 10.6 |
| hainbuche | 69 166 | 145 453 | 2.10 | 5.00 | 5.0 | 106 504 | 20.3 |
| kastanie | 26 163 | 245 143 | 9.37 | 5.50 | 5.5 | 73 396 | 11.8 |
| kiefer | 195 308 | 676 500 | 3.46 | 3.50 | 3.5 | 93 054 | 18.1 |
| linde | 79 333 | 157 411 | 1.98 | 6.00 | 6.0 | 132 670 | 17.0 |
| saeulenpappel | 69 052 | 10 122 | 0.15 | 4.00 | 4.0 | 55 728 | 7.5 |
| tanne | 21 914 | 1 000 000 | 45.63 | **2.08** | 9.0 | 35 632 | 6.1 |
| trauerweide | 100 894 | 437 341 | 4.33 | 4.00 | 4.0 | 286 282 | 31.3 |
| ulme | 70 142 | 170 596 | 2.43 | 5.50 | 5.5 | 102 012 | 23.0 |

**Nine of sixteen carry an honest crown** — `laminae/point` 2.0…4.5. The seven that do not name their
own defect and are listed in `## Gaps`; `fichte` and `tanne` are the two that hit the 1 000 000-instance
buffer cap and therefore report a leaf area index BELOW their declaration instead of pretending.

**A TREE IS GROWN PER SPECIES, NEVER PER INSTANCE, and the two numbers decide it.** Growth costs
**0.18–1.10 ms** (mean 0.417 ms, best of 50, native `-O2`, Apple silicon) and the mesh costs
**390–2042 kB** (mean 770 kB). All sixteen together are **6.8 ms and 12.0 MB**. Per instance a
5000-tree stand would be 2.1 s of growth and **3.7 GB** of mesh; per species it is 12 MB and a
transform. The leaf mesh is free by comparison — 0.0014–0.031 ms.

**Topology is toolchain-independent, coordinates are not.** Counts and indices are identical across
`-O0 -O1 -O2 -O3 -Os`, `-ffp-contract=off` and **emcc `-O1` executed under node**; the *positions* drift
by at most **6.8e-6** of the unit tree height between native and wasm (normals 4.8e-5, leaf vertices
3.5e-4), which is FP contraction and libm, and on a 30 m beech is 0.2 mm. **`-ffast-math` breaks it** —
`kiefer`'s needle shoot loses one needle because `(int)(0.85f/0.0085f)` is exactly 100 and reassociation
puts it below.

**`G(el)` is measured for all sixteen and read by nobody.** `LeafAngleDistribution` delivers the sampled
curve, `Sward.h`'s three-constant fit and the stalk-elevation histogram; the numbers and the check that
validates them are in `## Knowledge`. The consumer — the tree's own canopy shading — is the next layer.

**Nothing of the template system is built.** No 8-bit quantisation, no template table with a stack, no
plausibility filter. The only vegetation in the frame is the aggregate term of the ground fragment
([`stages/terrain.md`](stages/terrain.md)), which reads a table indexed by the albedo bucket: nine rows,
each declaring a ground class, a litter class and the sward's `perM2` · `heightM` · `heightJitter` ·
`widthM` · `dryFraction` and two colours.

**`~/Git/wasm-tree`, measured 2026-08-06 by reading `src/render/render.c` and the species files** — the
transferable half is smaller than this spec previously assumed:

| Claim | What the source actually contains |
|---|---|
| sixteen species covering the stack | all sixteen are **TREES** (`height_m` 12…40, no value below), and `tr_ground` is a plain ground plane. **The 0–2 m layer does not exist at all** |
| a hierarchy of wind levels attached to a hierarchy of geometry | **three sine bands summed into ONE scalar** `sway`, evaluated in a single shader |
| trunk, branches and twigs each displaced | `u_time` and the two wind uniforms are read by **exactly one program**, `CARD_INST_VS` — the instanced **leaf card**. The grown trunk/branch mesh is **not displaced at all** |
| per-species wind | **2 of 16 species files declare `wind_amp`/`wind_freq`** (birke 0.022/1.9, trauerweide 0.030/1.3). The other fourteen fall back to the renderer default 0.012/1.6 |

The wind's closed form and what follows from it for motion vectors are in [`lod.md`](lod.md) — the
result belongs to the LOD argument, the deficit belongs here.

**The floor under the sward exists as a material.** `grasfilz` is the 17th class of
`sim/assets/world/ground-materials.json`: chromaticity 1.000 : 0.674 : 0.280 and visible/shortwave
ratio 0.579, both path B over the two ECOSTRESS *Avena fatua* litter spectra (vh354/vh355, UCSB ASD);
`albedoBroadband` 0.20 `[SET]`, bracketed 0.199–0.234 by two measured shortwave ratios that disagree.
`wiese` references it instead of `erde_trocken`, and it carries the moss overlay at 0.15.

**A thatch is not a green floor.** `grasfilz`'s measured G/R is **0.674** against `erde_trocken`'s
**0.675** — the two are the same red-brown to 0.001, and only the blue differs (0.280 against 0.382).
The thatch fixes the SUBSTANCE under a meadow and moves no hue.

**`swardClosure` is live**, a template field read by `src/world/VegetationTemplates.cpp`: the terrain
row's ground and litter reflectance are pulled toward `mix(greenLinear, dryLinear, dryFraction)` — the
template's own blade class and its own dry share, no shader constant. `wiese` declares **1.0**;
`alpenrasen` declares nothing, so its measured LAI leaves 17 % of a nadir fragment showing the
limestone below.

**The blade is measured and the gain is gone.** `vegetation.json` carries one `bladeClasses` row,
`suessgras`, and thirteen templates reference it:

| | linear triple | Y | ratio |
|---|---|---|---|
| green blade | `[0.1506, 0.1892, 0.0803]` | 0.1731 | 0.796 : 1.000 : 0.425 |
| dry blade | `[0.3526, 0.2377, 0.0988]` | 0.2521 | 1.000 : 0.674 : 0.280 |

Chromaticity path B over four ECOSTRESS **green** grass-leaf spectra (`vegetation.grass.avena.fatua`
vh352/vh353, `vegetation.grass.bromus.diandrus` vh350/vh351, all UCSB ASD, all 2015-03-18) and the two
**senescent** ones of the same species and campaign (vh354/vh355) — the identical pair that gives
`grasfilz` its chromaticity, so standing dead blade and the felt it becomes cannot disagree about which
material was measured. **The path B implementation reproduces this repository's own published numbers
exactly**: per-sample ratios 1.000:0.668:0.287 / 1.000:0.681:0.272, mean 1.000:0.674:0.280, „own visible
luminance factor" 0.2521, visible 0.2229 / shortwave 0.3855 / ratio 0.579, and the green-grass
chromaticity 0.807:1.000:0.385 quoted in `moos`.

**What the retired declaration was**: two DISPLAY bytes per template plus a global
`grassReflectanceGain = 0.50` tuned against a tone curve replaced twice since. For `wiese` the shader
saw a sward of Y **0.1232**; it now sees **0.1968**, and the declaration and the delivery are the same
number for the first time.

**The chain's leaf → canopy factor is measured, not assumed.** Evaluating `swardAggregate` against a
Lambertian of the same illumination gives an effective canopy albedo of `A_eff / colIn` = **0.298** at a
nadir view with a 40° sun, **0.456** grazing, **0.73** at the reference scene's 11° sun. For `wiese`
that is A_eff 0.0586 / 0.0897 / 0.1428 against the measured sod canopy **0.0839** and against
`kGroundBounce`'s own „grass ~0.10". Under the retired gain the same view chain gave **0.0343 /
0.0525**, i.e. **0.77 EV** below the measured canopy.

**Half of the previous round's unexplained tone spread was in this declaration.** Rock against meadow,
as the chain delivers it: `kalk` 0.2857 against the meadow's effective canopy albedo was **8.33×** at a
nadir view (the round before measured 9.21 in a picture) where the reference photograph carries 2.39.
It is now **4.88×** for `wiese` and **4.08×** for `alpenrasen` — **0.77** and **1.03 EV** of the 1.95 EV
gap closed, without touching a rock reflectance or the tone chain.

## Gaps

- **`osmDefault` is a constant where the question is per place, and no constant answers it.** Measured
  over the six fit poses, the share of the near class grid under no closed way at all is 0.590
  Nebelhorn · 0.341 Herzogstand · 0.214 Innsbruck · 0.042 Hochkönig · 0.004 Hochries · 0.000 Zugspitze,
  and 0.152 averaged over 100 z14 tiles of the Alps. **It does not follow elevation** — Hochkönig at
  2941 m is the second lowest of the six — so unmapped ground is where the mappers did not go, not
  where the trees stop. The old justification, „the commonest unmapped cover of this landscape", was
  never measurable; the commonest MAPPED cover over those tiles is forest at 45.70 %, not meadow at
  17.18 %. `osmDefault = alpenrasen` was tried and MEASURED: it moves the frozen-mask distance the
  wrong way at 4 of 6 cameras (`narbe` |ΔL| Nebelhorn 3.96 → 8.96, Herzogstand 3.74 → 6.68, Innsbruck
  12.86 → 16.53, Hochries 28.27 → 33.81). `wiese` stays, and it still asserts three things OSM did not:
  mowing, a closed sward, 0 % open soil.
- **`alpenrasen` reaches 3.63 % of the mapped alpine land and none of the Nebelhorn tile.** The class
  is right and the selector is thin: what stands above the treeline arrives as `natural=grassland` only
  where somebody drew it, and at Nebelhorn 63.7 % of the tile is drawn by nobody at all.
- **`kWholeDry` 0.35 / `kTipRun` 0.25 were written for a blade shader that no longer exists.** What
  survives the deletion is the senescence — a grass leaf dies from the tip down — so the pair stays and
  the population mean dryness is 0.431 × the declared `dryFraction`. **Neither number is measured
  against a phenological series**, and every template's `dryFraction` is `[SET]` against that 0.431.
  Removing the pair was tried and MEASURED: `boden` |ΔL| improves at 3 of 6 (Nebelhorn 8.57 → 5.68,
  Herzogstand 9.19 → 6.41, Innsbruck 2.84 → 0.01) and `narbe` |ΔL| worsens at 4 of 6; the picture goes
  khaki at Nebelhorn and washes out the Hochkönig valley. It was not shipped.
- **`alpenrasen.widthM` is `wiese`'s and is not sourced.** Only the product `perM2 × widthM` reaches the
  fragment, so it costs no picture today; *Nardus stricta* is a bristle and the true width is far below
  11 mm.
- **`alpenrasen.ground.class = kalk` is regional.** It is right for all six reference cameras (Northern
  Calcareous Alps) and wrong by the full `kalk`↔`waldboden` distance on the crystalline Central Alps.
  The class field has no lithology to ask.
- **The 256 templates do not exist**, nor does the template stack, nor anything that reads one. The
  index side (quantisation, plausibility filter) is [`classification.md`](classification.md)'s gap.
- **The generator has no consumer.** A tree mesh exists and nothing in the frame asks for one: no
  placement, no LOD, no impostor, no wind, no material. `make treebench` is the only caller. The first
  rendered tree is `gpu_walk --rig`, and it is the next round.
- **Perennials and shrubs still have no producer.** Structurally they are small trees and `TreeGrower`
  grows one given a small `height_m`, few `trunk_steps` and `bare_steps: 0` — but no such file exists,
  and the claim that the parameters reach that low is untested. Ground cover and the sward remain
  shading terms in the ground fragment.
- **Three fields of a species declaration are parsed and read by nothing.** `leaf_card_w/h`,
  `leaf_cards`, `leaf_card_budget` (eight species) and `leaf_droop` (trauerweide) belong to the
  leaf-card stage, which does not exist. `leaf_midrib` / `leaf_vein_*` were the prototype's normal-map
  bake and are **not carried at all** — no species declares one, so nothing was dropped.
- **The one model assumption in `G(el)` is untested against a picture.** „The lamina rolls freely about
  its stalk" is what closes the roll analytically and it is what the prototype's leaf cards also do
  (`canopy_build_instances` rolls the card normal at random about the leaf axis). It is stated, not
  measured, and the thing that would measure it is a rendered canopy.
- **Seven of sixteen crowns are too coarse to hold their own declared leaf area index**, and the meter
  that says so is `laminae/point` (`## State`). Measured 2026-08-08 at rank 0:

  | species | laminae/point | why |
  |---|---|---|
  | eiche | **131** | `trunk_steps` 14 and `branch_chance` 0.6: the whole skeleton is 330 shoots, an order of magnitude under a beech's |
  | tanne | **46** (capped) | crown grows 28 m wide against a declared 7 m — the `spread_m` gap below, and it inflates the projection the index is multiplied by |
  | fichte | **13** (capped) | dito, 31 m against 6 m |
  | eibe | 19.9 | `conical` 0.35 with `max_order` shoots too short |
  | esche | 10.8 | `branch_chance` 0.7 over a 24-step leader |
  | kastanie | 9.4 | dito, 20 steps |
  | saeulenpappel | **0.15** | the opposite fault: 69 000 shoot points inside a 4 m columnar crown |

  Every one of them is a **declaration** defect, not a grower defect — the same grower delivers 2.0–4.5
  for the other nine. `fichte` and `tanne` are additionally clamped by the instance budget and report
  the index they built (1.13 and 2.08) rather than the one declared. Not fixed in this round: each needs
  its own skeleton reconsidered against the species, which is a botanist's judgement on a rendered tree.
- **The subject bench draws sub-pixel laminae and undersamples its own crown.** At the `eye` framing a
  0.10 m beech leaf on a 30 m tree covers well under one pixel, so the backlit silhouette measures
  **31.2 % green coverage of its crown box** where a leaf area index of 6 is nearly opaque. The area is
  there (`lai` 6.00 built); the rasteriser is throwing it away. The rig draws true laminae at every
  distance and has no card rank of its own — that is what has to change, not the growth.
- **`spread_m` and the grown crown disagree by up to a factor 4.6, and this is the round's real find.**
  Both metre fields were declared in all sixteen files and read by NOTHING in the prototype, so nobody
  had ever compared them. Measured now — grown box width (mean of x and z, at height 1) against the
  declared `spread_m / height_m`:

  | worst too wide | | worst too narrow | |
  |---|---|---|---|
  | fichte | **4.56×** (0.171 declared, 0.781 grown) | trauerweide | **0.66×** (1.000 declared, 0.657 grown) |
  | tanne | **4.14×** (0.175, 0.724) | ulme | **0.66×** (0.571, 0.375) |
  | birke | 1.95× (0.409, 0.799) | linde | 0.77× (0.600, 0.465) |

  Mean over sixteen 1.44, range 0.66–4.56. The two spire conifers are the extreme: `fichte` declares
  35 m × 6 m and grows a crown 27 m wide. **`spread_m` is therefore not a parameter today, it is a
  comment** — either the growth must be made to honour it or the field must go. It cannot be decided
  by argument; it needs a botanist on a rendered tree.
- **`height_m` has a reader and no user.** The mesh comes out at height 1 and `TreeSpecies::HeightM` is
  the scale that makes it metres. Nothing scales anything yet.
- **REJECTED, with the measurement: matching two scales by COLOUR.** At **identical albedo** the drawn
  stand and the terrain under it still stood **21 display codes apart in R−G and 17 in L**, because the
  blades carried the stand's Beer–Lambert occlusion (⟨occ⟩ = (1−e^−kL)/(kL) = 0.389 for `wiese`) and a
  forward transmission lobe worth up to 2.5× while the terrain carried neither. **No albedo can close a
  hand-off**; only the same terms on both sides can, which is why there is one scale now
  ([`stages/terrain.md`](stages/terrain.md) `## Gaps` carries the frame cost that decided it).
- **`swardClosure` lost its derivation and kept its value.** `wiese`'s 1.0 was derived as the gap
  fraction at the cover stage's fade radius (44.16 m at 1.70 m eye, Beer–Lambert with LAI 3 and
  G = 0.5). There is no fade radius any more, so the number stands without a basis and has to be
  re-derived or retired.
- **The sward's colour is now applied twice by two mechanisms, and nobody has measured the overlap.**
  `VegetationTemplates.cpp` pulls the material's own albedo toward the sward colour (`swardClosure`),
  and the fragment then mixes that material toward the aggregate's radiance by the stand's own cover.
  Where cover < 1 the residual ground is already green. Structural, unmeasured.
- **The dryness the loader assumes and the dryness the fragment realises are two numbers.**
  `swardClosure` mixes at the declared `dryFraction`; the fragment realises it as `kWholeDry` whole
  dead blades plus a `kTipRun` tip ramp, whose area-weighted mean for `wiese` is **0.129 against the
  declared 0.30**. That is the same „code may not overrule a declaration" the density contract makes,
  one field over.
- **The wind is one shader program and two species** (`## State`). What is missing here, before any
  „map LOD level to wind band" plan can start, is the **wind-displaced trunk and branch geometry** and
  `wind_amp`/`wind_freq` for the other fourteen species. The band-to-level mapping itself is
  [`lod.md`](lod.md)'s open design and is **unsourced** there.
- **`mesh_grow` has never been compiled into this tree.** The claim that `src/core/` transfers as-is is
  a reading of the source, not a build. Nothing measures what it costs to grow a stand at load time,
  which is the number the whole „growth, not assets" decision rests on.
- **No species has a seasonal state**, and as of this round two templates are pinned to a DATE instead.
  `wiese.forbs` names Wilde Möhre and Wiesen-Flockenblume because they flower on 6 August and
  Wiesenkerbel does not; `acker` declares 0.12 m stubble at `dryFraction` 0.92 because winter wheat on
  Weserbergland loess is threshed by then. Both are correct for `mods/demo/scene.json` and wrong for
  any other date. A season parameter is its own round with its own acceptance; until it exists, every
  number that moves with the calendar is a hard-coded 6 August.
- **Closure is angle-independent and the gap fraction is not.** At a steep view the same Beer–Lambert
  gives a gap fraction near 0.10, so the floor's chroma should show and is suppressed. The lever is
  `footM` inside `groundMat` (`render/stages/TilesStage.cpp`), which would make the floor emerge as the
  view steepens.
- **The wind chain below has no consumer.** `render/WindField.h` and the elastica fit are complete and
  measured; nothing reads them since the stand went static. The consumers they were built for — a
  branch and a rotor — are the next layers, and until one exists the chain is knowledge without a
  subject. What stays open in it, each with what it moves:
  - **`kGustAmp = 0.5` is the number the picture is most sensitive to.** The streamwise turbulence
    intensity at a canopy top is of that order, but only the phase-locked part of it belongs on ONE
    wave, and the split was not found. Py et al. give the size of the real motion — a space-averaged
    rms canopy-surface velocity of 0.03–0.06 m/s at 2–4 m/s of wind, i.e. about 1 cm of tip travel on
    a 0.69 m plant. Same order, on a canopy that is not ours; a plausibility check, not a calibration.
  - **The response is QUASI-STATIC and the forcing sits exactly on resonance**, which is the wrong
    place to be quasi-static. The lock-in result says the wave runs AT `f0`; a quasi-static element
    answers with the stiffness while a resonant one answers with the damping. For a leaf in air the
    aerodynamic damping is heavy, so the true amplitude is probably below the static one — by how much
    is unmeasured. This is separate from the `[SET]` above.
  - **`f0` is `[SET]` and it is a factor 2.3 wide.** 1.80–4.16 Hz, from carrying alfalfa and wheat over
    by `f0 ∝ L⁻²`; no eigenfrequency of a meadow grass was found. It moves the wavelength and the
    frequency together and leaves the phase speed exactly where it is, which is why the anchor that IS
    measured is the phase speed. `kBladeCd` (1.9) and `kLaminaKgM2` (0.15) are `[SET]` the same way and
    move only the amplitude.
  - **The element's azimuth relative to the wind is not in the load**, and the wave is the **2D primary
    mode only** — its crests are straight and infinitely long. The bending equation assumes the lamina
    broadside to the flow; the real instability goes three-dimensional (Py et al. §5.3 note the primary
    wave's characteristics survive), which is what makes a real honami a band of finite width rather
    than a ruled surface.
  - **ONE flow serves every class in the frame.** The canopy the profile is read over is the sward
    height of the DENSEST declared template — a property of the table, so walking cannot change it —
    but a heath and a meadow in the same frame see different canopy-top winds and different
    eigenfrequencies. A per-class flow would make `ω` a function of place, which is a wave problem and
    not a lookup.

## Knowledge

The screen-space-error rule is derived in [`lod.md`](lod.md) `## Knowledge`; `wasm-tree`'s wind closed
form and its consequence for motion vectors are in [`lod.md`](lod.md) `## Spec`.

### G(el) per species — MEASURED, 2026-08-07

Over the leaf points of the grown tree, 91 elevations × 64 azimuths, `make treebench --angles`. The
model assumption is one sentence and it is in `LeafAngleDistribution.h`; the fit is `Sward.h`'s own
`G = g0 + g1·sin(el)^p` and `resid` is the worst absolute deviation of the fit from the samples.

| species | G(0°) | G(90°) | g0 | g1 | p | resid | mean stalk el |
|---|---|---|---|---|---|---|---|
| ahorn | 0.4872 | 0.5267 | 0.4872 | 0.0392 | 2.070 | 0.00035 | 28.6° |
| birke | 0.4968 | 0.5068 | 0.4968 | 0.0097 | 2.050 | 0.00026 | 31.6° |
| buche | 0.4722 | 0.5581 | 0.4722 | 0.0861 | 2.095 | 0.00017 | 24.3° |
| eberesche | 0.4707 | 0.5603 | 0.4707 | 0.0897 | 2.065 | 0.00016 | 23.8° |
| eibe | 0.4995 | 0.5030 | 0.4995 | 0.0034 | 6.000 | 0.00044 | 32.6° |
| eiche | 0.4932 | 0.5173 | 0.4932 | 0.0240 | 2.520 | 0.00010 | 30.5° |
| esche | 0.4664 | 0.5701 | 0.4664 | 0.1041 | 2.100 | 0.00051 | 22.8° |
| fichte | 0.5027 | 0.4947 | 0.5028 | −0.0079 | 1.820 | 0.00019 | 33.5° |
| hainbuche | 0.4780 | 0.5464 | 0.4780 | 0.0683 | 2.105 | 0.00021 | 25.8° |
| kastanie | 0.4910 | 0.5204 | 0.4910 | 0.0291 | 2.240 | 0.00025 | 29.8° |
| kiefer | 0.4827 | 0.5374 | 0.4827 | 0.0546 | 2.155 | 0.00009 | 27.4° |
| linde | 0.4697 | 0.5631 | 0.4697 | 0.0936 | 2.090 | 0.00026 | 23.6° |
| saeulenpappel | 0.4589 | 0.5882 | 0.4589 | 0.1317 | 2.210 | 0.00272 | 21.7° |
| tanne | 0.5020 | 0.4976 | 0.5020 | −0.0045 | 1.335 | 0.00033 | 33.1° |
| trauerweide | 0.4729 | 0.5554 | 0.4729 | 0.0828 | 2.050 | 0.00022 | 24.6° |
| ulme | 0.4618 | 0.5796 | 0.4618 | 0.1189 | 2.120 | 0.00123 | 21.8° |

**The three conifers validate the estimator against a value nobody put in.** For a direction uniform on
the sphere `|u·e_y|` is uniform on [0,1], so the mean elevation is `∫₀¹ asin(u) du = π/2 − 1 = 32.70°`
and `G(el) ≡ 0.5` at every elevation. `eibe` 32.63°, `tanne` 33.05°, `fichte` 33.52°, and their G stays
inside 0.494–0.503 across the whole range — the whorled needle shoots come out isotropic, which is what
an isotropic population must measure and is not something the code was told.

**Trees are near-spherical, the sward is erectophile, and that is the whole reason for measuring.**
`Sward.h` declares `kG0 0.4050 · kG1 0.2496 · kGp 1.5700`, i.e. `G` swinging 0.405 → 0.655; the widest
tree (`saeulenpappel`) swings 0.459 → 0.588 and the narrowest (`tanne`) not at all. Shading a beech
canopy with the grass constants would put the extinction coefficient **17.3 % too high at nadir**
(0.6546 against 0.5581) **and 14.2 % too low at the horizon** (0.4050 against 0.4722) — and it would get
the SIGN of the angular trend right only by accident. Two populations, two measurements, one form.

**Two fits are unidentifiable and say so.** `eibe`'s `p` = 6.000 sits on the scan's upper bound and
`tanne`'s 1.335 is arbitrary: with `g1` at 0.0034 and −0.0045 the exponent has almost nothing to act on.
For those species `G ≈ g0` is the whole content, and the residual (0.00044 / 0.00033) says so.

### The wind chain, and where every number comes from

The subject is `render/WindField.h` and `sim/tools/elastica.py`. Nothing reads them today (`## Gaps`);
they are kept because a branch and a rotor will, and because the anchor is published.

**The flow. Three published relations, one `[SET]`.**

| Relation | Statement | Source |
|---|---|---|
| phase speed | *„the combined parameter c = λf, which is the phase velocity, would not show any lock-in behaviour: we have globally **c ≈ U** as in a Kelvin–Helmholtz instability"* — `U` is a hot wire *„located just above the crop surface"*, i.e. `U_h` | [30] §4, §2.1 |
| frequency | the wave runs at the plants' own free-vibration frequency, independent of the wind: `f/f0` = 1.06 (alfalfa) and 0.81 (wheat), and a Sukhatme d-test puts both at 1 | [30] §2.2 |
| wavelength | `λ/h` rises with `Ur = U/(f0 h)` from ~1 to ~4 over `Ur` 0…6, which is what `λ = c/f = U/f0` says: `λ/h = Ur` exactly | [30] fig. 5(a) |
| eigenfrequencies | alfalfa **1.05 Hz** (0.8–1.5) at `h` 0.69 m; wheat **2.50 Hz** (2.0–3.0) at `h` 0.68 m | [30] Table 1 |
| gust amplitude | none found for the coherent share | `[SET]` |

The three relations are not independent — any two fix the third — which is why only two of them can be
declared and the third has to come out right. The declared scene lands at `Ur = λ/h = 1.46`, inside the
band [30] fig. 5(a) covers.

**The response. One equation, one dimensionless number, one fit whose ends are not fitted.** [31]
eq. (5.5)/(5.7) is a uniform cantilever standing normal to the flow, loaded by the cross-flow momentum
of the stream it turns:

```
d3(theta)/ds3 = Cy · cos^2(theta),   theta(0) = 0,  theta'(1) = theta''(1) = 0
Cy = rho · Cd · L^3 · u^2 / (2 B)                     (the SCALED Cauchy number)
```

`B` is the flexural rigidity per unit width and is **not declared anywhere** — it follows from the
eigenfrequency, because a clamped-free beam's first mode fixes `sqrt(EI/mu) = 2 pi f0 L^2 / beta^2` with
`beta = 1.8751`. The element's WIDTH cancels between the load and the rigidity, which is why a template
that declares a width does not also get to declare a stiffness. Solved and fitted in
`sim/tools/elastica.py` over `Cy` 0.02…44.4 (44 is a hurricane on this canopy):

| | form | residual |
|---|---|---|
| tip angle | `Θ = (π/2)·(t + (1−t)·t²·(1.09246 − 2.01180·t + 0.19703·t²))`, `t = x/(1+x)`, `x = Cy/(3π)` | max \|err\| **0.2702°**, max relative **0.65 %** |
| shape | `θ(s)/Θ = 1 − (1−s)³` — no free parameter at all | max \|err\| **0.0655** over the whole family |

Neither asymptote of the tip fit can be moved by the three coefficients: `x` carries the linear
cantilever's `Θ = Cy/6` exactly at zero load, and `t → 1` carries `π/2` at infinite load. The cubic in
`t` vanishes at both ends. The shape needing no fit is the more surprising result — the linear
cantilever's own slope shape under a distributed load survives reconfiguration.

**The solver is checked against the paper it comes from and not against itself.** The Vogel exponent of
the drag it produces, `F ~ U^(2+V)`:

| `Cy` range | 0.02 → 0.05 | 0.27 → 1.66 | 2.93 → 17.95 | 7.25 → 44.4 |
|---|---|---|---|---|
| **V** | **−0.000** | −0.070 | **−0.870** | **−0.961** |

[31] Table 1 gives **−2/3** for a plate or a fibre by dimensional analysis and by [33]'s theory, and
their own least-squares fit of the measured range gives −1.4. The solved exponent leaves 0 at low load,
as a rigid body must, and settles between the two published figures at high load.

**The wind profile.** `U_h/U_10 = ln((h−d)/z0) / ln((10−d)/z0)`, with `d = 0.67 h` and `z0 = 0.13 h`
`[SET]` inside the published bands for a dense canopy (`d/h` 0.6–0.7, `z0/h` 0.05–0.13). At a 0.30 m
sward the ratio is **0.16856**; the two ends of the bands give 0.155 and 0.190, i.e. ±10 % on
everything downstream. The 10 m reference height is a definition, not a choice — the Beaufort scale and
every met report are stated there.

**The rest, all `[SET]`, with what each one moves:**

| Constant | Value | Moves | Why it is not sourced |
|---|---|---|---|
| `kEigenHzAtRefLen` | 2.3 Hz at 0.527 m | frequency and wavelength together; **not** the phase speed | bracketed 1.80–4.16 Hz by carrying [30] Table 1 over with `f0 ∝ L⁻²`. No meadow-grass eigenfrequency found |
| `kGustAmp` | 0.5 | the visible amplitude, linearly | the coherent share of the canopy-top turbulence intensity was not found |
| `kBladeCd` | 1.9 | the amplitude, through `Cy` | a flat plate normal to the flow is 1.98 at infinite aspect ratio and 1.17 at 1; a grass lamina is 48:1 |
| `kLaminaKgM2` | 0.15 | the amplitude, through `B` | leaf mass per area over dry-matter content for a meadow grass; no trait-database value fetched |
| `kAirRhoKgM3` | 1.225 | 1.0 % of `Cy` at this scene's 100.6 m | ISA sea level, and the error is stated rather than modelled |

### Sources

| # | Source |
|---|---|
| 30 | Py, de Langre & Moulia, *A frequency lock-in mechanism in the interaction between wind and crop canopies*, J. Fluid Mech. 568, 2006, 425–449 — doi 10.1017/S0022112006002667 |
| 31 | Gosselin, de Langre & Machado-Almeida, *Drag reduction of flexible plates by reconfiguration*, J. Fluid Mech. 650, 2010, 319–341 — doi 10.1017/S0022112009993673 |
| 32 | Raupach, Finnigan & Brunet, *Coherent eddies and turbulence in vegetation canopies: the mixing-layer analogy*, Boundary-Layer Meteorol. 78, 1996, 351–382 — cited through [30] for the 3 < λ/h < 5 of the WIND structures |
| 33 | Alben, Shelley & Zhang, *Drag reduction through self-similar bending of a flexible body*, Nature 420, 2002, 479–481 — the V = −2/3 asymptote [31] Table 1 attributes to them |

The numbering is [`visual-target.md`](visual-target.md)'s; a number means the same paper everywhere in
`doc/render/`.
