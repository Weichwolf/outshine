Type: bug
State: active
Area: world, render
Tags: measured, picture

# The ground wears the floor UNDER the vegetation, and nothing stands on it

**Benchmark** — Unreal: a landscape layer paints the SURFACE and `FFoliageType` instances are
scattered on top of it; the layer weight also drives where the foliage goes, so ground and cover
cannot disagree. RAGE: terrain materials plus a separate grass/prop scatter driven by the same
material index. **Both agree, and the matter is closed**: the ground material is what is under the
plants, and the plants are separate instanced geometry. Neither paints a forest green onto the
terrain.

## This tree does the first half and not the second

`ground-materials.json` says it in its own subject line: *"A ground material is an independent
layer that a vegetation template REFERENCES; foliage and clutter sit on top of it and never
replace it."* The file is right and its twenty classes are right. **Nineteen of the twenty are
brown, grey or blue.** The only green one is `moss` at [0.126, 0.180, 0.093]:

| class | linear albedo |
|---|---|
| forest_floor | [0.224, 0.135, 0.049] |
| leaf_litter | [0.264, 0.190, 0.113] |
| needle_litter | [0.209, 0.126, 0.073] |
| grass_thatch | [0.280, 0.189, 0.078] |
| earth_dry | [0.235, 0.159, 0.090] |

There is no `grass`, no `meadow`, no `crop` and no canopy, and there should not be: a beech wood's
floor IS brown, and `grass_thatch` is DEAD grass, which is brown too. **The brown is correct.**

## Measured

At the rebuild that produced Heidelberg's picture, logged past the ledger:

    named=253515  unmapped=0  version=3  verts=653400

38.8 % of the ring's vertices wear a REAL land class -- not one falls to the unmapped row. The
classification works. The Koenigstuhl comes out (143, 123, 108) because it is being drawn as what
it is underneath: `forest_floor`. What a viewer sees of a beech wood from outside is the CANOPY,
and no canopy is drawn.

`src/generators/Forest.cpp` exists and is a complete generator. `git grep Forest` outside its own
files finds exactly one caller -- `Shipped.cpp`, the registry that lists it. **No place scenario
puts a tree anywhere**, and no measure in `--audit --measures` mentions a tree, foliage, a canopy
or vegetation of any kind.

## A CASE NOW REFUSES ON IT, on its first live run

`building triangles the world meshed` was a rebuild delta reading 0 on a warm start, so
`ClientShot.h`'s second check -- *"meshed N triangles and its picture varies by V ... the frame
holds the sky and the ground and NONE of the geometry built for it"* -- could never fire: its
entry condition was `row.Triangles > 0`. Made a state (board:2063), it fired at once:

    CentralPark  3 905 744 building triangle(s), varies by 0.8423 of 255 along its rows
    UNPREPARED -- the frame holds the sky and the ground and NONE of the geometry built for it

The bar is 1.0 and a blank frame reads 0.0000. **CentralPark sits at 0.8423, nearer the blank
frame than a real one.** Looking at it says why: Manhattan's skyline stands correctly along the
horizon, and the whole foreground -- the park the camera is standing IN -- is featureless brown
ground with road ribbons over it. Central Park is lawns, trees and a reservoir, and this engine
draws its soil.

That is this item, measured by a case rather than by an eye, and it is the strongest evidence in
it: the refusal is not about a missing class, it is about nothing standing on the class.

## THE DESERT WAS AN INDEX IN THE WRONG TABLE

`ClassField::ClassAt` returns a TEMPLATE row -- `ClassStructure::Evaluate` hands back
`(w0 & 0xFF)`, which `ClassBuilder` filled from `VegetationTemplates::Rule::Tpl`. There are 14 of
them. `Picturing` used it to index `GroundMaterials`, of which there are 20, and its guard
`which < wearing.Count()` let every one through because 14 < 20. It checked that the number FITS
the table, not that it is the table's number.

| template | got | should have |
|---|---|---|
| 0 `mixed_broadleaf` | 0 **`sand`** [0.394, 0.251, 0.091] | `forest_floor` with its sward |
| 2 `meadow` | 2 `earth_moist` | a closed graminoid sward |
| 9 `settlement` | 9 `forest_floor` | `earth_dry` |
| 11 `water` | 11 `needle_litter` | `water` |

`sand` at [0.394, 0.251, 0.091] is the bright orange-brown that lay over every picture this tree
made. Reading `VegetationTemplates::Rows()[which].Ground` instead -- the table that is INDEXED BY
TEMPLATE and already carries the sward -- moves Heidelberg:

| point | wrong table | right table | |
|---|---|---|---|
| the Neckar | (100, 83, 82) | **(64, 85, 102)** | B > G > R: water |
| valley meadow | (98, 87, 90) | **(120, 119, 102)** | the sward at its declared dry fraction |
| Koenigstuhl | (143, 123, 108) | (102, 93, 98) | forest floor, not sand |
| Altstadt roofs | (173, 166, 154) | (171, 165, 153) | unmoved: buildings are not ground |

`Rows()` and `RowBytes()` had NO CALLER before this -- a complete, flat, GPU-shaped table nobody
read, the third such thing found in one night beside `ClassStructure::Words()` and
`Stage::AutoExposure`.

## WHAT IT COSTS, derived before anything is written

`vegetation.json` already declares every density. Over the fine class grid -- 2048 m square,
4.19 km2 -- they come to:

| template | trees | grass blades |
|---|---|---|
| mixed_broadleaf | 117 441 | 377 487 360 |
| conifer_forest | 138 412 | 146 800 640 |
| meadow | 3 355 | **3 355 443 200** |
| settlement | 25 166 | 1 761 607 680 |

At 200 triangles a tree, `mixed_broadleaf` alone is **23.5 M triangles** over that one grid.
Venice's ENTIRE world today is 1.28 M. Grass as geometry at 90/m2 costs 1.80 M triangles for a
100 m square and 7.20 M for 200 m.

**So the answer is not one thing, it is two, and the numbers decide which is which.**

- **A SWARD IS A SURFACE, not blades.** Beyond a few tens of metres a lawn's reflectance IS the
  blades' and not the soil's, and `vegetation.json` already carries exactly that: `bladeClasses`
  gives `graminoid` a `greenLinear` of [0.1506, 0.1892, 0.0803] and a `dryLinear` of
  [0.3526, 0.2377, 0.0988], every template gives a `dryFraction`, and `meadow` declares
  `swardClosure: 1.0` -- a closed sward through which no soil is seen. Mixing the sward into the
  ground albedo by closure and dry fraction is not a cheat, it is what those four numbers were
  measured FOR. This is the cheap half and it is where the green comes from.
- **A TREE IS GEOMETRY near and an IMPOSTOR far**, which is Unreal's foliage HLOD and RAGE's
  billboard cards. 117 441 trees cannot all be geometry; the count that CAN is a measurement,
  not a guess, and it is board:2058's cluster work that decides it.

`trees.status` in every template reads `"declared"` -- the file's own word for the half that has
never been placed.

## What will be true

- [x] The SWARD reaches the ground's albedo. **It never needed building.**
      `VegetationTemplates.cpp:151-157` already mixes `greenLinear` and `dryLinear` by
      `dryFraction` and covers the floor by `swardClosure`, at LOAD time, into `Row::Ground`.
      What was wrong is that `Picturing` read a different table with an index that did not
      belong to it -- see below.
- [ ] A land class that means TREES scatters the generator its template already names, at a
      density the frame budget allows, with an impostor beyond it. The ground keeps its floor
      material underneath -- this item adds a layer, it does not repaint one.
- [x] `outshine/places/RenderCentralPark` goes from UNPREPARED to PASS on its own bar, without
      the bar moving -- **and it was NOT the trees that did it.** board:2064 put the class
      evaluation in the fragment shader and the park's own shape appeared: bounded woodland,
      lawns, paths, water. `varies by 0.7991 -> 1.113` against a bar of 1.0 that never moved,
      `outshine/places` 8 PASS 1 UNPREPARED -> 9 PASS. The box is honestly ticked and the reason
      is written here so nobody reads it as evidence for a canopy.
- [ ] Measurement that shows this is wrong: the Koenigstuhl's pixels at Heidelberg's declared
      hour. They read (143, 123, 108) when this item was filed, R > G, and must read G > R once a
      canopy stands over them. **Re-measured after board:2064: (102, 94, 95), still R > G.** The
      per-pixel class moved the hill toward neutral and did not make it green, which is right --
      it changed where a class applies and not what stands on it.
- [ ] Negative control: a place whose classes are all mineral -- rock, scree, paving -- gains no
      instances and its pixels do not move.
- [ ] The frame budget is stated with the answer, because this is the first item that adds
      geometry to every vegetated square metre of the world. `Forest` is a RECURSIVE generator and
      CLAUDE.md lists high geometry with recursive generators FIRST among the five things the
      budget is laid out for; that is what it is for, and what it costs is the item's to say.

## THE TREES ARE PLACED AND NOTHING READS THEM

Measured on Heidelberg, one rebuild:

    generators: bodies they placed          1316 bodies
    instances its draw sources made         1311 instances
    restand: instances it carries              1 instances

`Picturing.cpp:129` fills `World.Instances` through an `Instancing` sink and
`Picturing.cpp:135` publishes its size. **`grep -rn "World\.Instances" src/` returns those two
lines and nothing else.** `Generators::Forest` -- a complete `Making`, species table, alpine limit,
density per row -- is reached by nothing outside `src/generators/`, and `ForestDraw` hands
`{Em, Nm, AslM, YawRad, Scale}` per tree into a vector no consumer opens.

So the ground wears the floor not because the trees are missing but because the trees are DROPPED,
1311 of them per rebuild. That is the ninth complete-but-unwired capability found this session and
it is the one this item is about.

## WHAT BLOCKS THE CANOPY, and it is two numbers rather than any code

A crown needs an ALBEDO and a SHAPE, and this tree has an origin for neither:

- **the foliage albedo.** `ground-materials.json` locks every one of its seventeen albedos to a
  measured broadband value with a measured chromaticity, by two independent paths that agree to
  0.011 per channel. `vegetation.json` carries `bladeClasses.graminoid.greenLinear` for a GRASS
  BLADE, sourced the same way. **There is no broadleaf or conifer canopy reflectance anywhere in
  this tree.** Using the graminoid green would be a stand-in with a named weakness -- the practice
  this tree already uses for `kWet` -- and it is the cheapest honest option; sourcing an ECOSTRESS
  canopy spectrum through the file's own path B is the right one
- **the crown geometry.** `Forest::Stem` carries `HeightM`, `HeightSigma` and `TrunkRadiusM` and
  says nothing about a crown's radius or its profile. A crown that reads from ABOVE needs a
  horizontal extent, and every number in it would be `[SET]` with no measurement behind it

**Neither was invented.** A canopy painted from two made-up numbers would look better in the next
screenshot and would be exactly the "Symptombehandlung" this tree refuses -- and it would poison
the one measurement this item declares, because the Koenigstuhl would go green for a reason that
has nothing to do with a tree standing on it.

- [ ] a foliage albedo with an ORIGIN, or the graminoid green named as a stand-in with its weakness
      written where the number is read
- [ ] a crown shape whose numbers say where they come from, even if that is `[SET]` with a reason
- [ ] `World.Instances` reaches the geometry, and the count that reaches it is published beside the
      count that was made -- 1311 and 1 today
