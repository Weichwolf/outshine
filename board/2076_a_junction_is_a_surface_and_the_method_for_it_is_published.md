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
and the bodies interpenetrate, which is what board:2075's goal forbids in as many words.

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
