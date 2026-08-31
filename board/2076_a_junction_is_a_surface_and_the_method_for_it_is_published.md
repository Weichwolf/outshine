Type: feature
State: open
Area: world, generators
Tags: infrastructure, osm, measured, benchmark

# A junction is a SURFACE, and the method for building it is published

**Benchmark** — RAGE: intersections are authored map geometry, drawn by hand. Unreal: a Landscape
Spline is laid by an artist and its connection points are placed. **NEITHER faces this problem**,
because neither builds its roads from a public map at run time -- so for once the two references do
not settle it and a third body must be cited. **CARLA (MIT) is admissible on this tree's own rule**:
it is open and readable, it is the reference for the driving simulation this engine is also going to
be, and it meshes ASAM OpenDRIVE rather than inventing a road model.

## What is measured here

    Kaiserberg   vertices two bodies SHARE      94 068
                 vertices in all             2 736 516      3.4 per cent

Two ways meeting at an OSM node have different directions, so their profile rings are rotated
against each other and their corners are not in the same place. Snapping welds only what already
coincides, and at a junction almost nothing does. Every way sweeps straight through the intersection
and the bodies interpenetrate, which is what board:2082's goal forbids in as many words.

## THE METHOD EXISTS AND IS IMPLEMENTED FOUR TIMES

Researched rather than invented, and the finding is that this is not hand-rolled anywhere serious.
The shape of it, in four independent implementations:

1. thicken each centreline to its own width
2. intersect the resulting edges with the neighbours' at the node
3. **TRIM each centreline back** to the perpendicular through the outermost such intersection
4. the junction polygon is the trimmed ends joined by the collision corners

| where | licence | note |
|---|---|---|
| **A/B Street / `osm2streets`** | Apache-2.0 | the one documented in PROSE rather than only in code |
| SUMO `NBNodeShapeComputer` | EPL-2.0 | the same computation inside `netconvert` |
| StreetGen, arXiv 1801.05741 | -- | buffer intersection plus `ST_BuildArea`, stated as a database query |
| Wilkie, Sewall & Lin, IEEE TVCG 2012 | -- | offsets from the MINIMUM TURNING RADIUS, and a greedy method for over- and underpasses that needs no `layer` -- which is this tree's exact situation |

ASAM OpenDRIVE 1.8 stands behind the same shape: a junction is a boundary polygon plus an elevation
grid, not a pile of overlapping roads.

**And `MinRadiusM` is already declared** for the five road classes that state one -- tertiary 200 m,
secondary 300, primary 400, trunk 500, motorway 720 -- and read by nothing. Wilkie's offsets come
from exactly that number.

## What will be true

- [ ] A node where two or more ways meet produces ONE junction body, and the ways are trimmed back
      to it so no two bodies interpenetrate
- [ ] The junction's corners are the ways' own trimmed corners, so they are SHARED rather than
      coincident by luck -- the 3.4 per cent above is the number that must move, and it must move
      because the geometry changed and not because the snap got finer
- [ ] The trim uses `MinRadiusM` where a class declares one, which is what makes a motorway ramp's
      fillet different from a residential corner
- [ ] Measurement that shows this is wrong: two bodies that overlap in plan and in height. It is
      every junction today
- [ ] Negative control: a way with no neighbour at either end is not trimmed and does not move

## What this does NOT cover

Lane-level geometry. SUMO computes per-LANE polylines at a default 3.2 m and connects them through
the junction; this tree sweeps one carriageway per way. Lanes are their own item and the driving
simulation will want them.

## STEP THREE OF FOUR IS BUILT: the ways come back from the node

The trim is the part that removes the interpenetration; the polygon is the part that fills what it
leaves. Step three is built and measured at Kaiserberg:

    way ends a junction trimmed      12 720
    the deepest trim                     24.00 m
    ends STILL crossing, cap bit      3 581      = 28 per cent

**So 72 per cent of junction ends no longer plough through their neighbour, from 100 per cent.**

Each end is trimmed by `(wOther + wMine * cos) / sin` over every other way at the node -- the
outermost crossing of the thickened edges -- which is the same expression `NBNodeShapeComputer` and
`osm2streets` arrive at. It runs away with the cotangent when two ways meet almost head on, so it is
capped at four half-widths: **a trim longer than the road is worse than the overlap it removes**,
and the count of ends the cap bit is published rather than hidden, because those are exactly the
bodies still passing through one another.

Two ways that are collinear at a node are not trimmed at all (`sin < 1e-3`), which is right: a way
split for a tag change is one road and has no junction.

**The vertex-sharing number FELL, 94 068 -> 87 504, and that is the trim working.** Pulling the ends
back opens the gap the junction body will stand in; sharing cannot rise until that body exists and
its corners ARE the trimmed ends. The number to watch is `ends STILL crossing`, and it must reach
zero by a polygon rather than by a bigger cap.

    outshine/places 10 PASS, tidy baseline 4428, both unmoved

- [x] the ways are trimmed back so their bodies stop short of one another
- [x] a junction BODY fills the gap, and its corners are the trimmed ends
- [ ] `ends STILL crossing` reaches zero -- 3 581 of 12 720 today, and they are the ends where the
      cap bit rather than ends the junction missed

## STEP FOUR IS BUILT, and the shared corners more than doubled

Each way's trimmed end is recorded as a GATE while it is swept -- where the carriageway stops, which
way it points and how wide it is -- so a junction's rim is the roads' OWN corners rather than a
second set beside them. `RaiseJunction` takes the gates at a node in bearing order and fans a closed
slab from their common centre; the centre stands at the mean of the gates' grades, which is what
makes a junction on a slope tilt with the roads reaching it instead of sitting level and cutting
into one of them.

    Kaiserberg   junction bodies raised          8 131
                 vertices two bodies SHARE      87 504 -> 188 822
                 vertices in all             2 869 232          = 6.6 per cent
                 triangles                   5 451 046 -> 5 590 530

**Nodes are built in a DECLARED order** -- sorted by key -- because a hash map's order is the
machine's, and determinism outside the shaders is not optional here.

**LOOKED AT, from directly above, where a junction cannot hide**: Zurich's old town reads as a
CONTINUOUS network -- filled corners, no gaps, no stars where several ways meet. That is the picture
a road network makes from the air, and it is the first time this tree has drawn one.

And a third ordering defect was found the same way as the two before it: the junction build sat
beside the trim measures, which run BEFORE the sweep that fills the gates, so it would have built
8 131 junctions out of an empty map. Three of these in one item now. The pattern is worth the
sentence: **in a function this long, WHERE a block sits is a decision, and it is invisible in the
diff.**
