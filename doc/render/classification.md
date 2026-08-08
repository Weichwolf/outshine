# Classification — the chain before the first pass

**Origin:** §4.0 of [`visual-target.md`](visual-target.md), split out because it is **not a pass**: it is
the chain that decides what every pass then draws. Neighbours:
[`stages/terrain.md`](stages/terrain.md) (the first and today the only consumer of the class),
[`vegetation.md`](vegetation.md) (the template the vegetation branch indexes),
[`stages/buildings.md`](stages/buildings.md) (the consumer of the building branch),
[`../world/terrain.md`](../world/terrain.md) (where the raster
and the vector tiles come from). The epoch and decay parameters this chain reads are declared in
[`../goal.md`](../goal.md) — three each, a selection, not a blend.

## Spec

### 0. `SiteContext` — one value, many generators

> Owner, 2026-08-06: *„könnte alles aus der gleichen Basisklasse abgeleitet sein, die Position, Albedo,
> Epoche und Decay kennt"* · *„ach ja stimmt und OSM Daten"* · *„slope ist auch noch relevant bzw.
> Terrain-Normale"* · *„das Wetter spielt dann auch noch mit rein"*

Both branches below read the **same inputs**, so those inputs are one declared value and not a
convention each generator repeats:

```
SiteContext { position · normal · baseAlbedo · climate · epoch · decay · osm }
        ↓                    ↓                    ↓                    ↓
   vegetation           buildings            ways, walls,        anything later
                                             masts, rock
```

**It is a context DATUM, not an inheritance root.** A tree *is not* a placeable; it is *generated from*
one. So no `Tree : Placeable` — a value in, a generator out. That is also this tree's own rule
(`CLAUDE.md`: composition over inheritance) and it keeps generators swappable without touching the
input shape.

**What that buys is consistency by construction, not by convention.** Because epoch and decay hang on
the *place* rather than on each object, no generator can read them differently: a collapsed roof cannot
end up beside a freshly cut hedge. That is a failure class which otherwise only shows up in the finished
image, where it can no longer be attributed to anything.

| Input | What it settles | Note |
|---|---|---|
| **position** | region and its building tradition; the species list; latitude for the seasonal filter | a Weser town builds differently from a Bavarian one — the same mechanism as the Weserbergland species list |
| **normal** | slope **and aspect**. Magnitude decides what stays put — scree above, soil below, a steep face sheds litter. Aspect is not a nuance in Central Europe: a south face carries oak and dry grass, a north face beech, spruce and moss, and its soil is measurably darker | must be **smoothed over a fixed world radius**, never the triangle normal — otherwise the class flickers with tessellation and, under Nanite, changes with the LOD level. See §"A class is decided per ground location" |
| **baseAlbedo** | the coarse split, and for buildings the residential/commercial/industrial bucket | **an INDEX, never a reflectance.** This exact confusion drew road fill at 0.79 linear where asphalt is 0.12 ([`stages/terrain.md`](stages/terrain.md)). In a shared context the same error would propagate to every branch at once |
| **climate** | the long-run mean — precipitation and temperature decide what *grows* here | distinct from weather, see below |
| **epoch** | the same footprint becomes half-timbered, post-war or ruin; the same ground becomes managed or overgrown | the project's core sentence applied per site: one dial, one dataset, three looks |
| **decay** | degree of dereliction, **as one of three discrete stages** | **never uniform** — render at edge, plinth and weather side. The `architect` names uniform decay as a defect |

**Epoch and decay are DISCRETE — three stages each — and that makes them a shader SELECTION, not a
blend.** Owner, 2026-08-07: *„es muss auch nicht graduell sein. drei epochen und drei decay stufen
reichen"* · *„dann ist es eine reine shader auswahl"*.

That is a simplification with two consequences worth stating, because both remove a class of problem
rather than deferring it:

| | |
|---|---|
| **The interpolation question never arises** | there is no "what does decay 0.37 look like". And that is right on its own terms: decay is a **replacement**, not a cross-fade — half-transparent rust over half-intact paint looks like neither |
| **Each stage must stand on its own, and that is testable** | with a continuous dial nobody can say whether an intermediate value is correct. With three stages, **every one must be defensible by itself and a critic must be able to tell them apart** — no stage may be a midpoint |

**Not built, and deliberately so.** Owner: *„wir bauen decay jetzt noch nicht. das ist rein
shader-seitiges design, nur der parameter muss überall schon vorbereitet sein."* Today `epoch` and
`decay` appear **nowhere in the code** — the only greps are false friends (chaff decay, Unix epoch). The
task in scope is to thread two small integers with a declared default (present, intact) to every place a
material sits — ground, cover, buildings — where they are **carried and not read**. A field that arrives
and is ignored is more honest than one that is missing, and it is the difference between touching the
shared declaration once and touching it twice.

The scene does not grow for this: `mods/demo/scene.json` stays at position, direction, time, wind and
cloud cover. Epoch and decay belong to the mod ([`../mods.md`](../mods.md)), not to the measuring
bench.
| **osm** | everything the raster cannot express: road, forest, water, hedge, power generator | beats the other inputs, see the conflict rule |

**Weather is NOT in the context, and that is the point.** The class must not depend on it, or the world
would rebuild itself whenever it rains and the tree species would change with the forecast. Weather
changes the **state** of a class, never the class:

| | changes over | acts on |
|---|---|---|
| position, normal, albedo, osm | never | the class |
| climate | decades | the class |
| epoch, decay | years | the class |
| **weather** — wind, precipitation, humidity, temperature, cloud | **minutes** | the **state** of the class |

A slope is dry grassland (class, stable) and wet today (state, transient) at the same time. The ground
keeps `erde_trocken` and raises `moisture`, and its albedo falls to **0.45×** (Idso et al. 1975, and the
material table already carries the field) — which is exactly the spec's own sentence, *„wet earth is not
a second class, it is `erde` with the dial up."* Weather's two consumers are already specified: motion
through `MEDIUM.flow` ([`body-format.md`](../body-format.md) §1.1 — the same flow that bends a branch and
turns a rotor) and material moisture in the ground shader.

### ONE chain, TWO branches

> Owner: *„vegetations klassifizierung über albedo und position und osm (strasse, wald, feld, etc) ->
> bodenshader -> foliage,clutter -> scrubs -> trees. klassenmodell zuerst, dann shader, dann geometrie."*
> · *„Gebäude müssen später wie die Vegetation von den Geokoordinaten, Grundalbedo, Epoche und Decay
> abhängig sein."*

**Not two chains. One structure with two branches** — inputs in, class out, generator from the class.
What differs between them is which inputs they read and what the class names; the shape, the build order
and the conflict rule are shared.

| | Vegetation branch | Building branch |
|---|---|---|
| Inputs | albedo · position · OSM vector | **geo-coordinate · base albedo · epoch · decay** · OSM footprint |
| Result | a **vegetation class** (mixed broadleaf, meadow, …) | a **building type** (Weser-Renaissance half-timber, post-war dwelling, barn, ruin …) |
| Generated from it | ground material + the 0–40 m layer stack ([`vegetation.md`](vegetation.md)) | massing, roof form, facade articulation, material |

### The vegetation branch

**The data flow is fixed:**

```
albedo + position + OSM vector  ──▶  CLASS  ──▶  ground shader  ──▶  foliage · clutter  ──▶  shrubs  ──▶  trees
```

Every stage reads the class and never re-derives it. A tree does not ask the albedo what it is standing
on; it asks the class, and the class was decided once.

**Classification takes THREE inputs, and today only one is wired:**

| Input | What it settles | State |
|---|---|---|
| **albedo** | the coarse split — green vs. field vs. sealed vs. water | built (32³ LUT over the key colours) |
| **position** — latitude, elevation, slope, aspect | the plausibility filter of [`vegetation.md`](vegetation.md), plus scree above / soil below and what a steep face can hold | **not wired** |
| **OSM vector** — road, forest, field, water, park, hedge, treerow | everything the raster cannot express | **not wired** |

The third one is not a refinement, it is load-bearing, and it is already measured: the OSM raster paints
deciduous and coniferous forest with the *same* colour `(70,105,60)`, so `nadelwald` is unreachable by
albedo — and `ufer` is a buffer around water, which is a geometry question, never a colour. Both classes
exist in `vegetation.json` today with **no key colour at all**, waiting for exactly this input. The
vector tiles are already fetched and parsed for the building footprints; the classifier simply does not
read them.

### The building branch

The same shape, four inputs plus the footprint. What each one decides:

| Input | What it settles | Why it is not optional |
|---|---|---|
| **geo-coordinate** | the regional building tradition | an old town on the Weser builds differently from one in Bavaria or Finland. This is the same thought as the Weserbergland species list on the plant side — a region is a *type distribution*, not a texture |
| **base albedo** | dwelling / commercial / industrial, plus hints at roof and wall colour | the OSM raster colour again, and with the same warning as on the ground: it is an **INDEX that selects a type**, never a reflectance value ([`stages/terrain.md`](stages/terrain.md)) |
| **epoch** | the same footprint becomes pre-industrial half-timber, a contemporary block, or a ruin | this is the project's core sentence applied to buildings: *one dial, one data set, three looks* |
| **decay** | the degree of dilapidation, as a continuum | plaster crumbles **at edge, plinth and weather side**, the roof falls in, growth takes over. **Not uniformly** — the `architect` agent names uniform decay as a defect verbatim: *„bröckelt es an den richtigen Stellen (Kante, Sockel, Wetterseite) oder gleichmäßig, was falsch wäre?"* |
| **OSM footprint** | the ring and the eaves height | the only part that is measured rather than generated, and the only part that exists today |

**Where the pieces of this branch live, so nothing is stated twice:**

| Subject | Owner file |
|---|---|
| what epoch and decay *are* as world parameters — three epochs × three decay steps, discrete, a selection and not a blend; **not built, and read nowhere** | [`../goal.md`](../goal.md) |
| the acceptance rules for a generated building — roof landscape, materiality, siting, where decay must show | the `architect` agent |
| the body format that makes an aged building the *same* body with different numbers — an epoch dial may lower **joint stiffness** instead of loading a second model (a loose door is the same door with a softer `sprung`) | [`../body-format.md`](../body-format.md) §1.1 |
| the draw itself — prisms today, articulated massing later | [`stages/buildings.md`](stages/buildings.md) |

### The build order, shared by both branches

**It is not the same as the data flow:**

1. **Class model first.** What classes exist, what decides between them, how the inputs combine and who
   wins on conflict. A wrong class makes every later stage draw the wrong thing correctly.
2. **Then the shaders.** Ground material ([`stages/terrain.md`](stages/terrain.md)) and facade
   material, then the surfaces above them.
3. **Then the geometry.** Foliage and clutter, then shrubs, then trees — in that order, because each is
   sparser and larger than the one before, and a mistake in the dense layer is visible from 1.70 m while
   a mistake in the sparse one is not. On the building side: massing, then roof, then facade
   articulation, for the same reason in reverse — the silhouette is what reads first.

### The source is the VECTOR, and the raster is a derivation with a stated accuracy

> Owner, 2026-08-07: *„die klassifizierung wird sich vor allem darin zeigen müssen, dass
> nutzungsgrenzen gerade sind und kleine strassen und flüsse sichtbar werden."*

Both of those are things a raster cannot do, and that is why they are the acceptance. The numbers
below are the state of the built path, measured 2026-08-07 against the raw vector tiles; they are the
"before" half of the pair and they can only be taken while `/bake/osm` still exists.

**The finest class raster this build can produce anywhere on Earth is 2.9342 m per texel, and that is
a construction, not a budget.** `World`'s `kMaxZ` = 14 stops the quadtree, so the finest tile spans
`SpanM(14)` = 1502.33 m at the reference latitude over `TS` = 512 texels. Every consequence below
follows from that one number:

| | measured | derived expectation |
|---|---|---|
| perpendicular wobble of a **straight** OSM `land` boundary in the class raster | **RMS 1.192 m** over 12 segments ≥ 117 m, 40 samples each; **0.955 m** over the 11 that no second feature crosses; worst single residual 14.5 m | a straight edge quantised to a texel lattice has a residual uniform on ±½ texel, so RMS = `texel/(2√3)` = 2.9342/3.464 = **0.847 m**. Measurement and theory agree to 13 % — the raster is doing exactly what a raster does |
| bilinear support of the ground shader's class read | ±1 texel = **±2.934 m** | a feature narrower than **5.868 m** can therefore never be free of its neighbours' material |

**And a narrow feature mostly does not arrive at all.** Counted over the 3×3 z14 block around the
reference standpoint (8620–8622 / 5403–5405), against `tiles/src/raster.c`'s own `fb_lod_line_ok`,
which the client triggers at `tex=512`:

| kind | features | length | stroke in the class raster | reaches it at all |
|---|---|---|---|---|
| track · path · service · footway · cycleway | **144** | **86.4 km** | — | **no** — gated at `lod_ts ≥ 1024` |
| residential · living_street · unclassified | 28 | 28.3 km | **3.52 m** | yes |
| tertiary / secondary / primary / trunk / rail | 34 | 24.7 km | 4.69 / 5.87 / 7.34 / 8.80 / 2.93 m | yes |
| stream · ditch · river (`water_lines`) | 12 | 12.5 km | **4.40 m, all three alike** | yes |

**144 of 206 street features — 69.9 % — are absent from the class raster.** That share is a property
of **this client**, not of the bake, and the distinction matters: `fb_lod_line_ok` gates on `lod_ts`,
`bake_native` passes the requested `tex` as `lod_ts`, and both clients ask **`tex=512`**
(`AppWalk.cpp:402`, `AppWasm.cpp:331`). At `tex=1024` every one of the 144 draws. The bake is not
short of detail; the client asked for a small texture.

**What resolution does NOT buy is width, and that is the finding that survives.** `w3_roadstyle`
scales its stroke *with* `tex`, so the metre width is scale-invariant:

| `tex` | m/texel | residential stroke | service/track/path/footway/cycleway | class VRAM at 124 tiles |
|---|---|---|---|---|
| **512** (built) | 2.9342 | **3.52 m** | absent | 32.5 MB |
| 1024 | 1.4671 | **3.52 m** | drawn, 2.05 m | 130 MB |
| 2048 | 0.7336 | **3.52 m** | drawn, 2.05 m | 520 MB |
| 4096 | 0.3668 | **3.52 m** | drawn, 2.05 m | 2 081 MB |

A residential street is painted **3.52 m at every texture size** where it is 5.5–6.0 m, and a 1–3 m
stream and a 10–40 m river are painted **4.40 m each** at every texture size. Those are map strokes
and **must not be adopted as metres.** Raising `tex` moves the boundary quantisation (RMS falls as
`texel/(2√3)`: 0.847 → 0.423 → 0.212 → 0.106 m) and buys presence — it never buys a width, and it
costs VRAM as `tex²`.

### What the tile server already decides, and which half of it we take

`tiles/src/style.h` and `tiles/src/raster.c` run this whole chain already — decode MVT, map `kind`,
rasterise. Read before building, as instructed, and the three answers are not the three that were
expected:

| Question | What the server has | Verdict |
|---|---|---|
| **`kind` → class** | `w3_landcolor`, **60 kinds → an RGB colour**, with a rose-grey debug fallback, a `/health` counter `style_unknown_kind` and one stderr line per distinct unknown kind | **the kind ENUMERATION is worth taking, the mapping is not** — it maps to a palette for a map image, not to a class. Its 60 kinds are the honest list of what OSM emits here and are more than the 15 `land` currently carries; the *meaning* lives in `vegetation.json` |
| **overlap priority** | inter-layer: **declared**, six literal lines — `ocean` → `land` → `water_polygons` → `sites` → `street_polygons` → `buildings`, then `streets` lines in two passes (non-rail, then rail), then `water_lines`. Intra-`land`: **none — the provider's feature emission order, last drawn wins** | **the inter-layer order is adopted verbatim.** The intra-layer order is the finding: it is implicit, therefore not deterministic, and `land`'s features overlap by 2.41 % of the tile. It must be **declared per row** |
| **line → area width** | `w3_roadstyle`, texels at a 1024 reference, plus `fb_lod_line_ok` as a per-kind visibility floor on `lod_ts` | **not adopted.** See the table above: it is a stroke. The width has to be a declared metre value on the shared vector store, because the later infrastructure geometry must read the same number the classifier used |

**Neither class model is superfluous, and saying so is the point.** The server's table is a palette with
a rich kind list; `vegetation.json` is a class model with a poor key space — its nine templates are
keyed by `keySrgb`, i.e. by the *colour the server chose*. That is the coupling this round cuts: the
key belongs on the tag, not on the paint.

### The declaration, and it is one file that already exists

**`sim/assets/world/vegetation.json` is the right place and a third file would be a defect.** The key
space is `(layer, kind)` — the tag, never a paint — as a list on the same row that already carries the
template, with a declared `rank` for the overlap order and, for a line layer, a `widthM`:

```
{ "name": "versiegelt",
  "osm": [ {"layer":"streets","kind":"residential","rank":83,"widthM":5.5},
           {"layer":"sites","kind":"parking","rank":62}, ... ] }
```

**99 rows, 7 layers, and the layer list is DERIVED from them** — `VegetationTemplates::Layers()` — so no
layer name exists in C++ at all. That matters because the tile server's layer set depends on the
place: thirteen of the schema's layers are absent from the demo tile, and Manhattan carries its water
in `ocean`/`water_polygons` and its street surface in `street_polygons`, which the Weserbergland tile
does not have. A classifier wired to one region's layer set is blind in another.

**`kind: "*"` is a declaration, not a fallback.** `buildings` emits no `kind` at all, so one wildcard row
says *the kind does not change the class here*. A kind with neither an own row nor a wildcard is a
`Log::Error` — that is the mechanism, and it fired on its first outing: `sites/sports_centre` (the
British spelling) was unclassified, logged with its tag, and fixed by **one JSON line and no C++**.

**The rank bands follow the server's declared inter-layer order** — land 10–49 → water_polygons 50 →
sites 60 → street_polygons 70 → buildings 75 → streets 80 → water_lines 90. **Inside `land` the
principle is that a broad land-use envelope loses to the more specific cover mapped inside it**: a wood
inside a residential outline is a wood. That is the ordering the server does not have, over features
that overlap by 2.41 % of the tile.

**The width is a METRE value from the kind and it is decided here once**, because the later
infrastructure geometry must read the number the classifier used. German design-standard carriageway
widths without verges (RAA RQ 28 → 12.0 motorway; RAL 2012 RQ 11.5/9.5/7.5 → trunk/primary/secondary;
RASt 06 6.5/5.5/4.75/3.5 → tertiary/residential/living_street/service; ERA 2010 2.0 cycleway; KTBL 443
3.0 track crown; 1.8 footway, 1.5 path). Rail is the single-track ballast crown 3.8 m, ICAO Annex 14
code 4 gives runway 45 and taxiway 23. The waterway widths are `[SET]` representatives — OSM carries no
width on a water_line — and their only job is to keep a brook and a river an order of magnitude apart:
drain 1.0, ditch 1.5, stream 2.0, canal 8.0, river 12.0.

### The DEM answers where OSM has no answer, and it answers two different questions

> Every OSM reference implementation — OSM2World, F4map, MSFS — carries the same fallback, and for the
> same reason: OSM says where rock is MAPPED, and a landscape needs an answer where nothing is.

The two limits are separate because they are not the same statement. Both are declared in
`sim/assets/world/vegetation.json`'s `alpineLimit` block and read by `world/AlpineLimit.h`.

| Limit | Gates | Threshold | Band |
|---|---|---|---|
| **treeline** | woody instances only — the ground stays what it is, because closed alpine sward runs for hundreds of metres above the last tree | `treelineBaseM` 1900 m at 47.4° N, falling `treelinePerDegM` −58.8 m per degree of latitude | `treelineBandM` 200 m upward to the tree species limit; the FLOOR is jittered downward by `treelineJitterM` 150 m at a 700 m wavelength, the CEILING is not |
| **slope** | instances AND ground | the winning class's own `slope.plausibleDeg[1]` in `ground-materials.json` — `waldboden` 35°, `grasfilz` 40°, `sand` 34° | `slopeBandDeg` 4°, the width of the measured angle-of-repose range for angular coarse debris (33…38°) |

**Both edges of the treeline are one measured band and neither is a value with an error bar.** The
Northern Calcareous Alps carry closed forest to 1900 m and the last individual tree at 2100 m; density is
full at or below the first and exactly zero at or above the second.

**The ceiling is not jittered and that is physics, not caution.** The species limit is a climatic
isotherm — Körner & Paulsen 2004 measure a 6.4 ± 0.7 K growing-season soil temperature at 46 treeline
sites worldwide — and it really is flat over one massif. The closed-forest limit below it wanders 100…200 m
with aspect, avalanche tracks and centuries of Almwirtschaft, so THAT is what the noise moves. Jittering
the ceiling would put a tree above the measured species limit, and a single threshold, jittered or not,
draws a contour line across every slope in the picture.

**The slope threshold is NOT declared in `alpineLimit`.** It is the ground class's own number, which
already carries its own origin per class. Two numbers for one fact is how they drift apart.

### Three outcomes, three treatments, and a counter is not an error

> Owner, 2026-08-07: *„fehlgeschlagene klassifizierung muss im error log erscheinen"*

| Case | Treatment |
|---|---|
| **no OSM datum at this place** — 14.48 % of the reference tile, of which only 1.01 % lies under a closed way at all | expected, and the truth about OSM. A **declared** default class and a **counter**. Never a line per occurrence, or the log is unreadable |
| **a tag arrives that the table does not know** | **`Log::Error`, with the layer and the tag string.** This is the case the server watches with `style_unknown_kind`, and it is the one that hid 81 barrier ways for thirty hours |
| **tile missing, parse failed, geometry unusable** | **`Log::Error`, loud, with the tile** |

### There is no class raster, and that is the point

> Owner, 2026-08-07: *„können shader nicht direkt mit den osm vektordaten rechnen?"*

They can, and the measurement decided it before a line of cache was kept. **The outlines live in a
storage buffer and every fragment evaluates them**, the way a glyph rasteriser evaluates a contour:
resolution-free, so the class has no resolution that could make it a function of the viewer. The
failure this file spent three rounds forbidding by rule is now not expressible.

**The one condition it stands on is that no fragment tests against everything, and that is a number.**
Measured over the 3×3 z14 block at the reference standpoint — 20.3 km², 2 246 area features, 25 501
edges after every line is widened to its declared metre width:

| acceleration cell | edges per cell, mean | p95 | p99 | worst | features per cell, mean | worst | buffer |
|---|---|---|---|---|---|---|---|
| 4 m | 2.90 | 6 | 11 | 25 | 1.86 | 14 | 16.6 MB |
| 8 m | 3.16 | 8 | 12 | 35 | 1.93 | 14 | 4.9 MB |
| **16 m (built)** | **3.87** | **10** | **16** | **45** | **2.11** | **14** | **1.8 MB** |
| 32 m | 5.81 | 18 | 27 | 73 | 2.47 | 19 | 1.0 MB |

**The acceleration structure carries exactly two things per cell**, and the first of them is why the
common case costs no geometry at all:

| | |
|---|---|
| **base** | the winning class of every feature that covers the cell **without a boundary in it** — one byte, no edges touched |
| **seed** | for each feature that does have a boundary in the cell: its winding number at the cell's south-west corner, and that cell's edges of it |

A fragment walks **corner → (px, cy) → p**, two axis-aligned legs that cannot leave the cell, so only
this cell's edges can cross them and the winding is exact. Both legs use one sign rule each, and only
their *consistency* matters — the winding is tested against zero and never read as a number.

**Two tiers, and the split is a property of the VECTOR FETCH and of nothing else.** `kMaxZ` does not
appear anywhere in this chain: z14 within 1024 m of the camera (a 3×3 block guarantees 1502.33 m) and
z11 within 8192 m (a 3×3 block guarantees 12018.6 m). The far tier carries the **area** layers alone,
because at a kilometre a 7.5 m road is under a pixel and the street lines are two thirds of the z11
edge count. Beyond 8192 m there is no datum and the declared default is the honest answer.

**One geometry, one predicate, two evaluators.** `ClassField::ClassAt` (CPU, no GPU in the call at all
— that is the headless side) and `clsTier` (WGSL) read the *same bytes* with the *same* rule.
Refinement is one-way and lives only in the shader: the boundary fray is
`max(0.05 m, footM)` wide and the CPU does not have it. **So the two agree to within the fray, and
that bound is stated rather than assumed zero.**

**Four coverage weights per texel are retired with the raster that needed them.** At a point there is
exactly ONE class; the second one exists only to antialias the boundary, and the shader knows the
exact distance to it. That is strictly more than four quantised area fractions could say.
### The conflict rule

Stated once so it is not invented per stage: **OSM vector beats position beats albedo.** A mapped road is
a road even where the raster is green; a north-facing scree slope is scree even where the albedo says
meadow. The raster is the fallback, which is the same statement [`visual-target.md`](visual-target.md) §4
already makes — the world defaults to nature and OSM overlays civilisation where it is mapped. On the
building branch the same order holds with epoch and decay applied **after** the type is chosen: they
transform a type, they do not select one.

### A class is a property of the PLACE and never of the viewer

If a distant tile resolved to a different class than the same ground up close, the world would change
species — or building tradition — as the camera approaches. The classification runs **once**, and the
ladder of [`lod.md`](lod.md) only changes how its result is *drawn*.

**A filter may decide what is DRAWN. It may never decide what is THERE.** That is the whole rule, and
it is worth more than the two instances below, because both were found in the picture rather than in
the code — by the owner, twice, in the same words: *„bei Bewegung wechselt die Klassifizierung hin und
her."*

| Axis the class must not depend on | Why it is a separate instance |
|---|---|
| **Mip level** | a class index is nominal, so its only defensible filter is the MODE — and a mode filter has no in-between: level 3 says meadow, level 4 says field, and one step across the mip boundary flips the material under the walker's feet. The `R8Uint` array carries **exactly one level** and the shader reads level 0 unconditionally |
| **Tile zoom** | one array layer per resident tile, each in its OWN zoom at the same texel count, so the class raster's *ground* resolution doubles per zoom step and a boundary lands on a different lattice at a split. In the STEADY state that is real and invisible, and MEASURED as such: within one zoom every tile shares a grid, so only a split can move a boundary; the z13→z14 split sits at `SpanM(z)·kSseK/kEdgeTau` = 3000.8 × 623.54/384 = **4873 m**, where one z14 texel (±2.93 m) subtends 0.60 mrad = **0.41 px** at 688 px/rad. Sub-pixel. **What that measurement does NOT cover is a coarse tile drawn CLOSE because its child has not arrived**, and that is a second instance with its own size: a z13 parent under the walker's feet answers at 5.86 m/texel. Fixed for the ground cover, bounded for the terrain shader (`## State

**The class is EVALUATED from the vectors and there is no class raster on either side.** `world/ClassField.h`
owns the whole chain: it streams the OSM vector tiles into `OsmField`, resolves every feature through
the declared `(layer, kind)` table, lays the features down in declared rank order into a 16 m
acceleration grid and packs the result into ONE storage buffer that the fragment and the CPU read with
the same predicate. `TilesStage` binds it at slot 13; the per-tile `R8Uint` class array, its layer
allocator and `/bake/osm` in the class path are gone.

### The four acceptance numbers, before and after

Before, 2026-08-07, against the baked class raster; after, 2026-08-07, against the CPU evaluator
sampled on a world-snapped lattice (5 cm at the standpoint, 2 cm in the settlement 1.24 km west).

| | before | after |
|---|---|---|
| perpendicular wobble of a **straight** OSM `land` boundary | RMS **1.192 m** over 12 segments; **0.955 m** over the 11 no second feature crosses; worst 14.5 m | RMS **0.1803 m** over 78 segments ≥ 25 m, 3 053 transects; **0.0689 m** over the 94.4 % no second feature crosses within 0.5 m; worst 0.997 m. **6.6× / 13.9×** |
| residential street, drawn width (real 5.5–6.0 m) | **3.52 m** at every texture size | **5.50 m** median, n = 93 — the declared value, exactly |
| stream against river | **4.40 m, both alike** | stream **1.98 m**, river **11.98 m** — the declared 2.0 and 12.0 |
| grass-free width | nothing under **5.868 m** could be free of its neighbours (±2.934 m bilinear support) | the fray is **max(0.05 m, footM)**: a 5.5 m street is pure over **5.40 m** at 1.70 m eye height |

Two more widths measured the same way: `service` 3.55 m (declared 3.50, one 5 cm sample step),
`track` 3.05 m (declared 3.00), `footway` 1.80 m (declared 1.80).

### The class does not depend on the viewer, and it is a measurement rather than a rule now

| Check | Result |
|---|---|
| **CPU against GPU**, the acceptance headless demands | 1 280×720 at the reference scene, class-viz frame plus the depth buffer, every ground pixel reprojected and asked of `ClassField::ClassAt`: **100.0000 %** agreement over **448 837** pixels in the 3 solid colours (98.2 % of ground pixels). Over all 457 014 compared pixels including the resolve filter's 1 489 edge-blend colours: 99.913 %. 1 514 pixels (0.33 %) lay inside the fray and are excluded by the declared bound |
| **World-fixity and cache consistency in one** | two runs, camera **600 m** apart, the acceleration grid re-anchored between them, class sampled on the same world-snapped 25 cm lattice over the 599 × 1200 m overlap: **0 of 11 496 000 samples differ** |
| Mip level, tile zoom, consumer lattice | not applicable: no raster exists to have a level, a zoom or a lattice |

### The three outcomes are separated and counted

| Case | Built | Measured at the reference scene |
|---|---|---|
| no OSM datum at this place | declared default (`osmDefault`, `wiese`), a **counter**, never a log line | **18.5 %** of the near grid's cell centres (17.2 % in the settlement) |
| a tag the table does not know | `Log::Error` with layer and kind, once per distinct pair, plus a feature counter | fired on `sites/sports_centre`; **0** after one JSON row |
| layer absent from the tile | **counter** — 13 of the schema's layers are absent from the demo tile and that is a property of the PLACE | `OsmField::MissingLayers()` |
| tile missing, bytes undecodable | `Log::Error` with the tile | `OsmField::BadTiles()` |

### What it costs

| | before | after |
|---|---|---|
| class VRAM | 32.5 MB (124 tiles × 512² × 1 B) | **3.48 MB**, one buffer — and it does not grow with the tile count, because it has nothing to do with tiles |
| albedo VRAM | 0 (already) | 0 |
| structure rebuild | — | **max 9.7 ms**, and it runs when the camera leaves its grid by 448 m (near) / 3800 m (far) or a vector tile lands. `walkbench` green over 1.4 / 4.2 / 15 / 150 m/s: p99 19.1 / 20.7 / 20.7 / 18.9 ms, > 2.5 frame periods **0.00 %** at every speed |
| per fragment | one `textureLoad` of an `R8Uint` array + 4 storage reads | one cell read + **3.87 storage reads on average**, p99 16 |

### The decoder bug this round found, and it had eaten every line feature

`OsmVector` emitted a `Ring` **only on ClosePath**, and a LINESTRING never sends one. Every way in every
tile therefore arrived with `RingCount` 0 — no error, no counter, a legal-looking empty feature. **206
street features and 12 water lines per 3×3 block, silently.** That is why the previous round could
only measure lines through the tile server's own raster. Now each MoveTo and the end of the geometry
stream close the open run.

The second half of the same class of defect: `OsmVector::Parse` returned the same bare `false` for *this
tile has no such layer* and *these bytes will not decode*. An island has no `water_lines`; that is not a
parser failure. Separated, with `present` as an out-parameter.

### The alpine classes, and the two limits — built 2026-08-08, binary `b46d7330`

**The tile server already carried the data and nothing in `tiles/` had to change.** Checked on the z14
tiles of `hochkoenig` 8786/5734, `zugspitze` 8691/5734 and `nebelhorn` 8662/5734, and on z9…z14 at
hochkoenig: `land/bare_rock`, `land/scree`, `land/shingle` and `water_polygons/glacier` are emitted at
every one of those zooms. **The defect was on our side**: all four rows selected `heide`, a heath on
`sand` with 0.004 trees/m², so the Mandlwand drew khaki-green with birches on it and the Zugspitze
summit grew trees at 2 962 m. They now select three new templates — `felsflur`, `schutthalde`,
`gletscher` — with every woody density at zero. 12 templates, 104 osm rows.

**The instance gate, measured on the six fit cameras.** Highest surviving stand above sea level, from
`walk treeline` (an exact per-run count, not an estimate):

| Camera | eye ASL | species limit | before | after | refused above the line / too steep |
|---|---|---|---|---|---|
| `herzogstand` | 1 600 m | 2 087.9 m | 1 702.2 m | **1 694.5 m** | 0 / 6 879 |
| `hochries` | 1 569 m | 2 079.6 m | 1 546.2 m | **1 546.2 m** | 0 / 6 883 |
| `innsbruck` | 1 945 m | 2 105.6 m | 2 419.7 m | **1 998.7 m** | 10 / 2 124 |
| `nebelhorn` | 2 224 m | 2 098.7 m | 2 186.9 m | **2 002.4 m** | 11 / 106 |
| `zugspitze` | 2 962 m | 2 098.8 m | 2 949.8 m | **no stand at all** | 27 / 0 |
| `hochkoenig` | 2 941 m | 2 098.7 m | 2 939.4 m | **no stand at all** | 27 / 0 |

**Four of six carried a tree above 2 100 m; none does.** The before column is the baseline binary's `footMax` plus its own logged
`groundM + eyeM + liftM`; the after column is logged directly.

**The scatter's own distribution was not touched.** Cell size, hash, jitter, size draw and the
near-to-far sort are unchanged; the gate is one further draw against `WoodyFraction × (1 − BareBySlope)`
on its own hash salt, evaluated after the density test so the four extra DEM lookups are only paid by a
cell that already holds a stand.

**It made the frame cheaper, because a refused stand is a stand not drawn.** 360-frame turntable at
1280×720 from the `hochkoenig` standpoint, both binaries pinned: p50 **8.137 → 4.542 ms**, p95
**14.549 → 4.934**, p99 **16.502 → 5.286**, mean 8.779 → 4.545. 10 051 impostor stands above the species
limit became 0; drawn tree triangles 271 694 → 144 484. Begin*Pass count per frame **7 → 7**.

### What is NOT built

**The building branch has nothing beyond the footprint.** Geo-coordinate, base albedo, epoch and decay are
declared in the Spec and read by nothing. Height is a `[SET]` 9.0 m on 95.8 % of the reference town,
because `fb-tiles` does not carry `building:levels`.

**`nadelwald` and `ufer` are still unreachable**, and now for a stated reason rather than a missing key:
shortbread carries no leaf type, and `ufer` is a distance-to-water derivation, not a tag. Both rows
carry an empty `osm` list, which is the class model naming its own hole.

## Gaps

- **The treeline law has two named residuals and models neither.** `treelinePerDegM` −58.8 m/° comes
  from two anchors of the same maritime-mountain type, 47.4° N/1900 m (Northern Calcareous Alps) and
  61.0° N/1100 m (Jotunheimen). Extrapolated it reaches sea level at 79.7° N where the real polar
  forest limit reaches it near 70° N, so it is ~10 degrees too generous at the top and only the clamp
  at zero carries it there. And it is far too low in continental interiors: the Sierra Nevada of
  California at 37° N carries its treeline at 3 300 m against this law's 2 512 m. Inside the Alps the
  same Massenerhebung effect is worth 300…500 m between the northern rim and the central chain. No
  input this engine has can see either.
- **`waldboden`'s 35° may be too low for an Alpine Schutzwald**, which stands on 40…45° routinely. The
  number is `[SET]` in `ground-materials.json` and is now READ rather than merely declared, so the cost
  of it being wrong went up. At `nebelhorn` the slope gate refused 6 879 of ~7 900 candidates — 87 % —
  and the photograph agrees there; at `herzogstand` it refused 6 % of 35 535 and the photograph shows
  forest on that flank. Not falsified, not confirmed.
- **`heide`'s ground class is `sand` and that is wrong above the treeline.** Alpine dwarf-shrub heath
  (Latschen, Alpenrosen) is dark green; `sand`'s orange-brown reads mauve under haze and it covers
  large parts of the mid field at `hochkoenig`. `heide` also has to serve the Lüneburger Heide, where
  sandy ground is right, so the fix is a split and not an edit. Not this round's subject.

### Water as geometry — three cycles, and what each of them actually found

**Cycle 1 built the wrong thing and the critic proved it without reading the source.** The class-view
frames before and after were **bit-identical, 0 differing pixels on two tiles** — so nothing new was
drawn. Cause: `World.h`'s `OsmField Vectors{14, {"buildings"}}` parses one layer, `Layer("water_polygons")`
returns −1, `f.Layer` is `uint16_t`, and the comparison is never true. Zero surfaces, zero vertices,
`SetWaterMesh` never called.

**The dark shapes taken for water were shadowed vegetation.** Water in an orthographic frame from
2 500 m is not dark, it is *bright*: p50 luminance 116 against an image median of 112.7, and the darkest
2 % are **41 % depleted** at water. The discriminator is chromaticity — B−R ≥ +15 inside the outline,
≤ +12 outside, a clean cut over 262 144 pixels. Cross-correlating that mask against the bake's finds its
maximum at **dx = dy = 0 at every threshold**: the class placement was never wrong.

**The 731 m canal is `kind=drain, tunnel=1`** — a culvert. `tiles/src/raster.c` draws water_lines with
no tunnel test, so over half the bake's water on that tile is underground. Chasing 100 % coverage there
would be rebuilding the reference's artefact; `WaterField` skips it and the defect as originally framed
is void.

**Cycle 2 drew, and folded.** A fan triangulation over a concave lake outline covered 13.0 % of the
bake's water at **51.9 % precision** — half of what it drew lay outside the shore. Ear clipping took
precision to **96.7 %** at the same recall: what is drawn is now water.

| triangulation | recall | precision |
|---|---|---|
| fan | 13.0 % | 51.9 % |
| **ear clip** | 13.1 % | **96.7 %** |

**Both suspects for the low recall were refuted by measurement.** The tile carries 8 water polygons,
each ONE ring of 7 to 68 points — so neither the `Exterior` filter nor the 512-point cap fires, and the
field collects 84 surfaces and 53 courses over the resident block, 2 383 triangles. The geometry was
built; it was not winning the depth test.

**It was coplanarity, and the fix is the one every engine uses.** The DEM already carries a flat surface
under a water body (Weser at Hameln, p5..p95 = 1.12 m over 1.5 km), so the water mesh and the terrain
mesh land in the same plane and fight for every pixel. Lifting the water 0.15 m — under the terrain's
own 47 m support spacing, over any float error at ECEF magnitudes, the same reason a decal is lifted:

| | recall | precision |
|---|---|---|
| fan, coplanar | 13.0 % | 51.9 % |
| ear clip, coplanar | 13.1 % | 96.7 % |
| **ear clip, lifted** | **26.7 %** | **97.0 %** |

Against the right denominator it is higher still: of the bake's 3 784 water pixels roughly 2 170 are the
culvert we correctly omit, so **1 042 of ~1 614 real water pixels = ~65 %**.

**Two surfaces, not one.** Owner, 2026-08-07: *„von mir aus gesehen braucht es zwei erdoberflächen.
wasser und erde"*, and *„alle spiele machen wasser seperat weil es animiert werden muss"*. The slope
still visible is the CLASS, not the mesh: `wasser` is still painted on the tilted terrain wherever the
geometry does not cover. The class stops being a ground class once the mesh covers what it claims —
until then two surfaces assert the same thing and one of them is wrong.


### Water is geometry, not a class — and the regime that will judge it

Owner, 2026-08-07: *„wasser kann keine steigung haben"*, *„wasser würde ich wie jeder engine als eigene
geometrie rendern"*. Painted as a ground class, a lake follows the hillside under it and a 3 m brook is
a sampling problem on a 16 m grid — **17 % coverage over 731 m of the Hannover canal, unchanged by the
centreline rewrite because that rewrite never touched it**. A surface is not level because a shader
says so; it is level because its mesh is.

`world/WaterField` is the answer and it is deliberately BuildingField's shape: a lake is a footprint
whose height comes from the shore instead of from a tag. The level is the ring's **5th percentile** of
terrain, not its minimum — measured on the Weser at Hameln over 26 292 water pixels, min 62.97 m
against p5 63.00 m against **max 129.93 m**, so outliers exist at both ends and only one of them is
cheap to be wrong about. A ring point more than 5 m above the level is counted, because that is OSM and
the DEM disagreeing about where the bank is. **Nothing draws it yet — there is no WaterStage.**

**The regime that judges stage 3**, from the same conversation, recorded because it decides what is
worth building:

| | measures | target |
|---|---|---|
| **320×180, numeric** | ΔE per cell, sky gradient, horizon position, haze profile against a live webcam frame | the trainable signal |
| **full resolution, LLM** | "which of these two is the photograph", order randomised | **50 % — indistinguishable** |

At 320×180 a tree at 100 m is two pixels and a building ten: silhouette and mass survive, texture does
not. That band carries exactly what an engine controls — **large-scale radiometry** — and drops what it
can never match. The owner's reasoning is the load-bearing part: get the coarse light right and the
full-resolution frame reads as real without being identical, so the answer to a bad comparison is never
more detail.

The reference is a fixed camera (foto-webcam.eu: position and altitude published, **no EXIF** —
verified, the images are re-encoded by gd-jpeg and carry nothing but a quality comment). Orientation
and field of view are solved ONCE per camera from landmark buildings whose OSM position we already
render, and **the residual after that fit is the admission test**: a camera whose landmarks cannot be
reconciled by any single pose is rejected rather than used, or the fit absorbs our own error into the
reference. Weather is the trap — an overcast frame against a clear render is a weather difference and
not an engine defect — so frames are selected by weather first and driven from `/wx` later. Season is
an open gap: the epoch regulator is declared and not built.

Performance is proven by a **360° yaw in 6 s** (60 °/s, 1° per frame at 60 fps, 360 frames): every tile
enters and leaves, the class field re-anchors, and unlike the walking benchmark the path is identical
every run — that benchmark's p99 scattered 5.1 ms on one binary.


### Stage 1 against the OSM bake — first critic cycle, 2026-08-07

Binary pinned `c411c12e…`, three places at z14, orthographic, 2.93 m/px. What the critic measured and
did **not** fault:

| | Weserbergland | Hameln | Hannover |
|---|---|---|---|
| class agreement on mapped bake pixels | 97.6 % | 88.7 % | 83.4 % |
| residual after eroding a 1 px band | **0.37 %** | **1.47 %** | **1.44 %** |
| after 2 px | 0.12 % | 0.82 % | 0.51 % |
| bake road pixels with our sealed class within 2 px | 100 % | 99.1 % | 96.5 % |
| building IoU | — | 0.874 | 0.889 |

**Nearly the whole disagreement is a one-pixel band at class edges** — the best registration is
dx = dy = 0 over a ±4 px search, no bake building is missing from ours and none of ours is absent from
the bake, and a building edge falls off within a single pixel. Geometry and placement are not the
problem.

**Two of the three defects are one defect.** A line feature is extruded to a polygon and then tested
inside/outside, which is a binary answer: a 2 m way at 2.93 m/px is **0.68 px** wide and is either hit
whole or missed whole. Hameln has **3 073 connected components of sealed surface against the bake's 31**,
2 873 of them ≤ 4 px, sitting exactly on way axes; Hannover's canal loses **407 m of 814 m** with a
longest gap of 137 m. Both are the same missing quantity: **coverage**.

The fix is the one the owner named — the font-rendering form. Distance to the CENTRELINE against the
declared half width gives coverage as a number (`clamp(0.5 + (halfWidth − dist)/footM, 0, 1)`), so a
0.68 px road draws at 0.68 coverage instead of 0 or 1. It replaces the stroked contour built earlier the
same day and costs FEWER edges, because a line stops being a polygon.

**Third defect, and it is honest that it stays open:** a Hameln forest polygon covers 2.0 ha of what the
bake draws as residential. Both sides resolve overlapping land use by feature order without sorting, and
the critic's own reading is that ours is likely the more correct of the two. It is a form difference
against the reference either way.


- **`/bake/photo` and the PNG path in the tile worker are still in the tree.** The class no longer touches
  the bake — `/bake/osm` has no reader left and neither client requests it in its default mode — but
  `fb_stream_pyramid`, `WriteAlbedoLayer`, the photo array and the worker's PNG decode still exist behind
  the EVS toggle. Deleting them is a wide, mechanical removal across `TerrainLoader`, `TileWorkerMain`,
  `TilesStage`, `Renderer`, `World` and both clients, and it was not done in this round.
- **The far tier answers with the DECLARED DEFAULT beyond 8192 m.** Flat ground beyond that subtends
  688 × 1.7 / 8192 = **0.14 px** from a 1.70 m eye, so it is only relief above the horizon that is
  affected; how much of the frame that is has not been measured.
- **A feature narrower than the pixel still flickers.** The fray blends the boundary over `footM`, which
  antialiases an edge but not a road narrower than a fragment: the fragment centre is either on it or
  not. A raster's mip averaged that; this does not. Not measured, not fixed.
- **`kSeedCap` = 32 and `kRefCap` = 255 are `[SET]` caps with a measurement under them** (worst cell 14
  features, 45 edges) and a counted fallback: over the cap the feature is written into `base` instead
  of getting a seed, which keeps its class and loses its boundary. `SeedOverflow()` is **0** at the
  reference scene and in the settlement.
- **Both branches' position input is still unwired.** Latitude, elevation, slope and aspect are named in
  the Spec and read by nothing; the conflict rule's middle term therefore does not exist yet.
- **Uniform decay is a named defect with no mechanism to avoid it.**
- **`verify-trees` is red for `world/terrain` and `mods/demo`**, unchanged by this round (9 orphans before
  and after).

## Knowledge

### A way is a SAMPLE of a curve, and the chords are the sampling artefact

Owner, 2026-08-07: *„eine strasse ist kurviger als die osm vermessung"*. A surveyor sets a node where
the bend needs one; the straight line between two nodes is an artefact of that sampling, not the road.
Drawing the polyline draws the sampling grid. So every ring is resampled along a **centripetal
Catmull-Rom through its own nodes** before it becomes edges — it passes through every declared node, so
no surveyed position moves, and it only adds points where the curve leaves the chord by more than
`kCurveTolM`.

**It is done at bake time and not in the shader, and that is the load-bearing choice.** Which class a
point belongs to is decided by a winding test against the edges, and how soft the boundary is by the
distance to them. Curving only the *distance* would leave inside/outside and edge position on two
different geometries — the same class of contradiction that went unnoticed for thirty hours in the DEM
registration. As geometry, both evaluators see one curve.

**Measured on the 3×3 z14 block at the reference standpoint** (each a pinned binary):

| tolerance | edges | buffer | build |
|---|---|---|---|
| polyline (before) | 105 304 | 3.56 MB | 7.1 ms |
| 0.15 m | 638 172 | 11.92 MB | 50.6 ms |
| **0.60 m (shipped)** | **393 293** | **7.91 MB** | **31.5 ms** |
| 1.20 m | 293 889 | 6.31 MB | 27.7 ms |

**The cost is intrinsic, not a parameter to tune away**: even a 1.20 m tolerance keeps 2.8× the edges,
because the ways really do bend that much. A line feature is the expensive half — each span becomes a
four-edge quad, so subdividing it into n pieces costs 4n.

**Centripetal bought nothing measurable and is kept only for correctness.** The first build used the
uniform parameterisation while its own comment claimed centripetal; switching to the real thing moved
638 172 edges from 699 202 and cost 34 % more build time. So the overshoot the centripetal form exists
to prevent was not what drove the count — the survey's own curvature was. Recorded so the experiment is
not repeated.

**Open:** the 31.5 ms build is a refill stall on tile arrival, four times what it was. It is a p99 event
and it is not yet measured in the moving benchmark.


Nothing is derived here yet. The measurements this file rests on are stated in place with their subject:
the shared forest colour `(70,105,60)` in the OSM raster and the missing key colours for `nadelwald` and
`ufer` (`## Spec`), and the 1634-of-1706 footprint-height fill with the `[SET]` 9.0 m substitution
(`## State`). The ground-material parameters the vegetation class feeds are derived in
[`stages/terrain.md`](stages/terrain.md) `## Knowledge`; the raster's resolution and its
consequences are in [`../world/terrain.md`](../world/terrain.md); the epoch/decay parameter model is in
[`../goal.md`](../goal.md).
