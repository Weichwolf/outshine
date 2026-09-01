Type: feature
State: open
Area: engine, world, generators
Tags: measured, owner

# A road AT GRADE is a material on the ground, not a ribbon over it

**Benchmark** — Unreal: a Landscape Spline DEFORMS the landscape heightmap and PAINTS its material
layer; the road is the landscape. RAGE: roads are baked into the terrain the map ships, materials
and all. **Both agree, and neither ships a road as a separate mesh laid on top.** A bridge is the
exception in both: a structure, its own body, resting on the terrain only at its abutments.

## Why, and it is already measured

Two meshes that merely TOUCH always leave a sliver: a ground triangle that crosses a carriageway
edge is above the road on one side and below it on the other, and no amount of pressing fixes a
seam the mesh has no vertex on. Measured on `cross-plane30` before the repair below:

    ground: the carriageway's footprint corners        104
    ground: of those, a ground vertex shares the spot    0

Sewing those corners INTO the ground mesh took the burial from 1.305 m to 0.540 m and, more to the
point, made the picture right rather than nearly right. The invariant is now an oracle and every
one of the 246 cells holds it.

**The consequence is the item.** If the ground already carries every vertex of the footprint, the
ribbon on top is a second body describing a surface the ground now describes itself.

## How: THREE PASSES, and the order is the whole design

| pass | over | what it does |
|---|---|---|
| **CLASSIFY** | everything | project the OSM footprint onto the ground, TESSELLATE the terrain along the projection's boundary, give the interior triangles the OSM material |
| **PLAFOND** | infrastructure | pull those classified triangles flat to what the building rules allow -- bounded gradient and crossfall for a carriageway, one level slab for a building |
| **MODEL** | buildings and bridges | what RISES or FLOATS: walls and roof on the slab, a span on its abutments |

**A road never reaches pass three.** It is ground that has been classified and plafonded, so
`terrain standing over a road` stops being a defect that can be measured and becomes a sentence that
cannot be said. A building reaches all three: its floor slab is ground, its walls are a body that
rises from the slab and shares the slab's boundary vertices. A bridge reaches one and three: it
rests only at its abutments and the terrain beneath it is classified and plafonded on its own.

**Where the tree already stands, measured 2026-09-01.** Pass one PROJECTS -- every ring vertex is
classified through `ClassAt` and carries its colour and its class uv -- but it does NOT tessellate,
so the material bleeds across a triangle that has no edge on the boundary. Pass two does not exist;
the corridor press written this round is its approximation and it is why a ribbon still has to be
drawn. Pass three exists and currently draws roads it should not.

## What will be true

- [ ] Every vertex of a RESTING footprint is a vertex the ground mesh carries. Holds today:
      `ground: footprint corners NO ground vertex shares` reads 0 across all 246 cells.
- [ ] A resting way draws NO ribbon of its own -- the ground carries its material instead, and
      `streets: ways laid as ribbons` counts only what floats.
- [ ] Every END vertex of a floating span is a vertex the welded terrain/OSM body carries.
- [ ] Negative control that goes RED: move one footprint corner by 1 cm without re-sewing and
      require the shared-corner oracle to fail.

## Pass one is blocked, and the blocker is named

The tessellation is written: any ring edge whose two ends carry DIFFERENT class rows is halved, the
new vertex is classified from its own place, and the closure is the same red-green one the refinement
uses. Four passes put the boundary within an eighth of an edge.

It divides nothing, and the measures say why:

    class field: the vegetation table is ready     1        14 rows
    class field: the fraction it has no data for   1.000    everywhere
    the ring's vertices a land class names         0        of every vertex
    class field: triangles the boundary divided    0

**It was not a watermark**, and the guess cost a build. Two hypotheses were wrong before the measure
was right, so the measure is what is written here:

    class field: features the fine tier holds     1
    class field: of those it has taken            0

`ClassField::Ingest` held the feature and refused it, by a rule that says so in as many words:

    if (layer == OsmLayerName(OsmLayer::Streets) ||
        layer == OsmLayerName(OsmLayer::StreetPolygons)) { continue; }

**The class field skipped STREETS on purpose**, because a street was drawn as a ribbon and a ribbon
does not want a class beneath it. That single `continue` is pass one. It is gone, the feature is
taken, and `outshine/places` went from 9 PASS 1 UNPREPARED to **10 PASS** -- CentralPark had been
unprepared all session because its ground varied too little, and road classes are what it was
missing.

A watermark repair went in beside it and is worth its own line: `OsmField` now carries a GENERATION
that changes only when the declaration's CONTENT changes, and `ClassField` resets its marks when it
does. Without the content test the generation rose on every restand and the class field rebuilt
itself 352 times in one run instead of twice.

**What is still red:** the taken feature reaches the builder and every probe still finds no data --
`the fraction it has no data for` reads 1.000 with a complete structure at version 2. The next step
is inside `ClassBuilder`, and it is the last thing between here and pass one working.
