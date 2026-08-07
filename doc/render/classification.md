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

**144 of 206 street features — 69.9 % — are absent from the class raster**, and the ones present carry
a **cartographic stroke, not a width**: `w3_roadstyle` returns texels at a reference texture size
(`FB_STYLE_REF_TEX` 1024), so a residential street 5.5–6.0 m wide is painted 3.52 m and a 1–3 m stream
and a 10–40 m river are painted 4.40 m each. Those numbers are a map style and **must not be adopted as
metres.**

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

**`sim/assets/world/vegetation.json` is the right place and a third file would be a defect.** Each
template already carries a `keySrgb` list — a list of keys that select it. The change is that the key
space becomes `(layer, kind)` instead of a bake colour, as a sibling list on the same row, with a
declared `rank` for the overlap order:

```
{ "name": "wiese",
  "osm": [ {"layer":"land","kind":"meadow","rank":30}, {"layer":"land","kind":"grass","rank":30},
           {"layer":"land","kind":"grassland","rank":30}, {"layer":"land","kind":"park","rank":32},
           {"layer":"land","kind":"village_green","rank":32}, ... ] }
```

**A further OSM layer is a row, not an edit**, which is the whole test: `layer` is a string the shared
vector store already resolves (`OsmField::Layer`), so `barrier`, `landuse` or anything `tiles/` later
carries is reachable without touching C++. The nine templates cover the 15 kinds `land` delivers plus
`farmyard` and `plant_nursery`; the two that stay unreachable are the two this file already names —
`nadelwald` (shortbread carries no leaf type) and `ufer` (a distance-to-water derivation, never a tag).

### Three outcomes, three treatments, and a counter is not an error

> Owner, 2026-08-07: *„fehlgeschlagene klassifizierung muss im error log erscheinen"*

| Case | Treatment |
|---|---|
| **no OSM datum at this place** — 14.48 % of the reference tile, of which only 1.01 % lies under a closed way at all | expected, and the truth about OSM. A **declared** default class and a **counter**. Never a line per occurrence, or the log is unreadable |
| **a tag arrives that the table does not know** | **`Log::Error`, with the layer and the tag string.** This is the case the server watches with `style_unknown_kind`, and it is the one that hid 81 barrier ways for thirty hours |
| **tile missing, parse failed, geometry unusable** | **`Log::Error`, loud, with the tile** |

### The near field needs a cache the tile quadtree cannot give it

This is the structural consequence and it is why this is a round of its own rather than a swap of one
function. The per-tile class array is indexed by tile slot and its ground resolution is
`SpanM(z)/TS`; `kMaxZ` = 14 puts a hard floor of **2.9342 m** under it, so **rasterising the vectors
into the existing array would fix the semantics and leave every acceptance number above unchanged.**
A 5.5 m street still could not be free of grass.

What the near field needs is therefore a **world-anchored** cache, independent of the quadtree:
snapped to a world lattice, scrolled in whole texels, and carrying **weights and not an index** —
per texel the four largest area fractions and the four class ids they belong to, so the shader does
the same four table reads it does today and the weights are *coverages* rather than bilinear
distances. Naming the resolution decision, as required: **four channels is a choice, because `land`
alone carries 15 kinds**; the measured overlap is 2.41 % of the tile over nine pairs, so a texel with
more than three classes in it is rare and four is a bound with a measurement under it, not a guess.
Below **≈ 0.37 m** any raster is resampling the vectors' own quantisation (`extent 4096` over
1502.33 m = 0.3668 m per unit, mean residual 0.140 m against raw OSM), so a finer cache must justify
itself; the fray the class boundary is allowed to carry is then the cache texel, and it is
centimetres by choice of that texel and not by a blend width.

**The cache is derived and never authoritative**, and the check is a pair: the same world point, two
cache resolutions, one class.

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
| **Tile zoom** | one array layer per resident tile, each in its OWN zoom at the same texel count, so the class raster's *ground* resolution doubles per zoom step and a boundary lands on a different lattice at a split. In the STEADY state that is real and invisible, and MEASURED as such: within one zoom every tile shares a grid, so only a split can move a boundary; the z13→z14 split sits at `SpanM(z)·kSseK/kEdgeTau` = 3000.8 × 623.54/384 = **4873 m**, where one z14 texel (±2.93 m) subtends 0.60 mrad = **0.41 px** at 688 px/rad. Sub-pixel. **What that measurement does NOT cover is a coarse tile drawn CLOSE because its child has not arrived**, and that is a second instance with its own size: a z13 parent under the walker's feet answers at 5.86 m/texel. Fixed for the ground cover, bounded for the terrain shader (`## State`) |
| **The consumer's own sampling lattice** | this WAS the reported defect. A stage that re-samples the class on a grid anchored to the camera makes the class a function of where the viewer stands, even though the class texture itself is innocent. Measured: grass coverage in a fixed WORLD strip swung **5.8×** with a period of exactly 2.0 m — the field spacing. The lattice must be anchored to the world, and the anchor must be exact |

Everything the distance *is* allowed to change — the relief octaves, the Toksvig roughness, the litter
mix — is derived from the pixel footprint further down the same function.

**Temporal smoothing is not a fix.** Hysteresis, a fade over time or a blend between the old and new
answer all leave the class a function of the viewer and merely make the dependency harder to see. The
acceptance is not "it looks stable": it is that a **fixed world point returns an identical class index
across a whole walk**, including points inside a transition zone, where it is not free.

**"Not yet known" is a state and it must be carried, not filled in.** Where the data for a place has
not arrived, the honest answer is that there is no class there — and a consumer that decides EXISTENCE
must then produce nothing. Letting a coarser ancestor answer in the meantime is the same defect as the
mip filter with a clock on it: it makes the class a function of the streaming state, and the streaming
state is a function of where the camera has been. No grass reads as "not loaded"; the wrong grass reads
as the world changing its mind.

### The class boundary is CLEAN and HARD — the fray is centimetres

> Owner, 2026-08-07: *„die grenzen müssen sauber verlaufen und relativ hart was die bodentextur angeht.
> mit ausfransen meine ich lediglich cm."*

**Two different things were conflated here and only one belongs to this file.**

| | Scale | Whose |
|---|---|---|
| **The ground class boundary** — where meadow becomes field in the ground shader | **centimetres** | this file |
| **The vegetation edge** — how trees thin out at a forest margin | metres | the tree layer, a PLACEMENT question |

The ground boundary is not an ecological zone. It is a **line treatment**: an OSM boundary is a
mathematical straight edge, and a few centimetres of world-fixed irregularity on the class weights stop
it reading as a drawn vector. Nothing more. **A road stays hard, and so does a field edge.**

| Contract | Acceptance / measurement anchor |
|---|---|
| The boundary is clean and hard | no ramp in metres, no per-class-pair width table for the ground |
| The fray is **centimetres**, and it is world-fixed | frequency declared in metres, evaluated at the world coordinate — never at a texel, a raster cell or a screen coordinate. A field whose frequency scales with the raster jumps at a zoom step, only less visibly |
| **Stability outranks the fray** | if the two collide the fray is dropped and reported as open. An edge that looks soft but breathes when you walk is worse than a hard edge that stands |

**Research that answered the OTHER question, kept because the tree layer will need it.** Measured
forest-edge widths for beech-on-limestone in this landscape: two thirds of all edges are **under 5 m**
(Lewark 1971, Hann. Münden, ~300 km of edge length); beech makes a ground-deep steep edge at ~1 m,
spruce 0.5 m, only oak on a S/W aspect reaches 5 m. The 20–30 m "ideal forest edge" of the guidance
leaflets is a *Leitbild* traced to romantic landscape painting (Gehlken 2014) and is fallow where it
occurs at all. **That is a placement rule for trees, not a blend width for the ground.**

### One transition that is not a gradient, and it is not the ground's

The unpaved field path is a **stripe pattern**, not a blend: grass — wheel rut — grass (centre strip) —
wheel rut — grass. Five bands, four hard edges, verge 1.0 m per side on a 5.50 m crown width (KTBL 443).
It is the one case where "clean and hard" produces structure rather than a single line, and it belongs
to whoever draws the path.


## State

**The raster colour is now an INDEX by construction, not by convention.** `World::ClassifyRaster` is the
only reader of the decoded bake: it resolves each texel through the 32³ LUT to one byte and the colour
bytes are dropped. The GPU array is `R8Uint`, so no fragment can reach a cartographic colour even by
mistake — the error this file warns about (road fill at 0.79 linear where asphalt is 0.12) is no longer
expressible. Measured side effect: `albedoVramMB` 130 → 0, `classVramMB` 43.33
([`stages/terrain.md`](stages/terrain.md) `## State`).

**The class is read at level 0 and the class texture has no other level.** The mode-filtered mip chain
that stood here made the class a function of the pixel footprint, i.e. of the viewer's distance — owner
report 2026-08-06, *„bei Bewegung wechselt die Klassifizierung hin und her."* MEASURED at the reference
scene (identical camera, mip-filtered read vs. level-0 read, class index rendered per pixel):
**0.000 %** of ground pixels below 30 m disagree, **6.05 %** at 100–300 m, **5.72 %** at 300–1000 m and
**7.66 %** beyond 1000 m — that share of the far field was being renamed by the filter alone, and it
changed whenever the camera moved. Reprojected onto ground cells, the level-0 read gives the same class
at the same place from two camera positions 8 m apart across **5 927** boundary-free 0.5 m cells
(100.0000 %). Side effects: `classVramMB` 40 → 30 (the chain was 4/3 of level 0), one `WriteTexture`
per tile instead of ten, frame time 6.80 → 6.93 ms at a run-to-run spread of 0.15 ms, i.e. inside the
noise. The mode filter also **deleted thin classes** — a road one texel wide loses every majority vote —
which is visible as broken road lines in the far field of the old class image.

**There is ONE class path left, and it is the fragment's own.** The second path — a 49×49 height/class
field that `World` rasterised for the cover stage — was deleted with that stage on 2026-08-07, and with
it the defect it carried: the field was centred on the EYE, so its samples resampled the class raster at
sliding points. The finding it produced survives as a rule rather than as a field, because the lattice
the stand is hashed on is now the graticule and nothing else
([`stages/terrain.md`](stages/terrain.md) `## Spec`).

**The class the shader draws is still the drawn leaf's, and the fixed-zoom form is measurably dead for
it.** Expressing the 130 drawn leaves of the reference scene at `kMaxZ` needs `Σ 4^(14−z)` = 11 776
class tiles = 2 944 MiB and 11 776 array layers against a 2 048-layer device cap — 5.75× over. What
the drawn class actually costs is bounded instead: 0 pixels of 921 600 differ between a walking frame
and a converged one at 1.0 m per pass, 2.23 % at 16 m per pass. Named in
[`stages/terrain.md`](stages/terrain.md) `## Gaps`.

**Nothing of the vector path is built, and the measurements above are the "before" half of its
acceptance.** What was done in the round of 2026-08-07 is the reading, the arithmetic and the
decisions: the server's chain was read and its three answers judged (one adopted, one found implicit,
one rejected with a measurement), the declaration was placed on the row that already carries a key
list, the three log outcomes were separated, and the structural blocker was found and stated —
`kMaxZ` = 14 puts a **2.9342 m** floor under every per-tile class raster, so the near field needs a
cache the quadtree cannot give it. **No line of the classifier exists.** The two blockers that were
named at the start of that round were already gone: `OsmVector::Str` and `OsmField::Str` read string
tags, and `OsmField` is the shared geodetic vector store across tile seams, already declaring
`{"buildings", "land"}` with `land` unread.

**Vegetation branch: one of three inputs is wired.** The albedo path exists as a 32³ LUT over the key
colours (`sim/src/world/VegetationTemplates.h` and the table it indexes); position and OSM vector are not
read by the classifier at all. There is no class model in the sense of `## Spec` — no class list with
declared inputs, no conflict resolution, and nothing downstream that reads a class rather than a colour.

**Building branch: nothing of it is built.** What exists is footprint extrusion and one albedo for
everything — no type, no region, no epoch, no decay. Measured in `sim/src/world/BuildingField.cpp` on
`/t/vector/14/8617/5404` (Hameln): **1634 of 1706 footprints carry exactly the provider's fill value 5**
— 95.8 %, integer — over an Altstadt that is three and four storeys of half-timber. The code therefore
substitutes a `[SET]` **9.0 m** (2 × 2.9 m floor-to-floor to the eaves plus a 3.2 m pitched roof on a
~9 m span at 35°) for that fill, and the right fix is named upstream: `fb-tiles` must carry OSM's
`building:levels`.

## Gaps

- **Two of three vegetation inputs are unwired.** Position (latitude, elevation, slope, aspect) and the
  OSM vector layers are named in the Spec and read by nothing. The vector tiles are already fetched and
  parsed for building footprints, so the acquisition half is done and the classifier half is not.
- **The measured consequence: `nadelwald` and `ufer` are unreachable.** The OSM raster paints deciduous
  and coniferous forest with the same `(70,105,60)`, and `ufer` is a distance-to-water question. Both
  classes exist in `vegetation.json` with no key colour, which is the class model asking for the input
  it does not get.
- **The 8-bit albedo quantisation and the latitude/elevation plausibility filter do not exist.** They are
  the index side of [`vegetation.md`](vegetation.md)'s 256 templates.
- **The building branch has no inputs at all beyond the footprint.** Geo-coordinate, base albedo, epoch
  and decay are declared here and read by nothing. There is no building-type list, no regional
  distribution, and no generator that takes one.
- **Height is a default, not a measurement, on 95.8 % of the reference town** (`## State`). Every
  building-type decision downstream inherits that: a type that implies storeys cannot be checked against
  a height that came from a constant.
- **Uniform decay is a named defect with no mechanism to avoid it.** The rule (edge, plinth, weather
  side) is written down in the `architect` agent's acceptance list; nothing in the tree computes a
  decay field with those anchors.
- **The vector classifier is specified and not built**, and the acceptance numbers stand measured at
  their "before" values: straight-boundary RMS **1.192 m**, **144 of 206** street features absent from
  the class raster, residential street painted **3.52 m** where it is 5.5–6.0 m, stream and river
  painted **4.40 m** alike. They cannot be re-taken once `/bake/osm` is deleted.
- **`vegetation.json` is keyed by the bake's palette.** Every one of the nine templates selects itself
  through `keySrgb`, i.e. through a colour `tiles/src/style.h` chose. Deleting the bake without moving
  that key first leaves the class model with no key space at all.
- **The class list itself is unwritten, on both branches.** The build order puts the class model first;
  what exists is a colour LUT, which is step zero of it. Until the lists, their inputs and their conflict
  resolution are written down, every consumer is free to re-derive — which is exactly what the chain
  forbids.

## Knowledge

Nothing is derived here yet. The measurements this file rests on are stated in place with their subject:
the shared forest colour `(70,105,60)` in the OSM raster and the missing key colours for `nadelwald` and
`ufer` (`## Spec`), and the 1634-of-1706 footprint-height fill with the `[SET]` 9.0 m substitution
(`## State`). The ground-material parameters the vegetation class feeds are derived in
[`stages/terrain.md`](stages/terrain.md) `## Knowledge`; the raster's resolution and its
consequences are in [`../world/terrain.md`](../world/terrain.md); the epoch/decay parameter model is in
[`../goal.md`](../goal.md).
