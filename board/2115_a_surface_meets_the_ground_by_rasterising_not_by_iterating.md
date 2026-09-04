# A surface meets the ground by rasterising, not by iterating

State: open

Putting a surface onto the ground -- a road ribbon, a building footprint, a water body -- and
tessellating the result is a STANDARD PROBLEM with standard answers, and this tree solves it with
neither of them.

## What is here

`GroundYield.cpp::Refine` splits the longest edge of every triangle until `WantedEdgeM` is
satisfied, up to `kMostPasses` times, with a `FlatMap` of edge midpoints rebuilt each pass. So the
cost is `passes x |triangles| x hash`, and every pass revisits triangles that were fine several
passes ago. `base/spatial/Refine.h::Divide` is the 1-to-4 red-green split underneath it and is
correct and generic -- the primitive is right, the LOOP around it is the finding.

## Why it iterates

**Because it cannot ask for the answer directly.** `WantedEdgeM` is a function of POSITION -- how
fine this patch of ground has to be, given what stands on it. The loop approaches that target by
halving, one pass at a time, which is what a solver does when the target is unknown. It is not
unknown here.

`Drape` is a REGULAR GRID: `kFinestCellM = 32 m`, `kCellPerRung = 8`, `kDrapeRungs = 6`. On a
regular grid a resolution target is not approached, it is INDEXED. That is the whole idea behind
restricted quadtree triangulation, and it is the reason terrain LOD stopped being an iterative
problem in about 2000.

## The two families, and which one this engine needs

| | how it works | what it costs | what it gives |
|---|---|---|---|
| **geometric** | clip the polygon against the mesh, triangulate the pieces | preload time | real geometry: a silhouette, a collision surface, a thing physics can stand on |
| **classification** | extrude the polygon to a prism, stencil it against the terrain's depth | frame time, every frame | pixels only -- nothing to drive on |

**Cesium uses classification** (`ClassificationPrimitive`, 3D Tiles vector data) because a viewer
draws a map. **Unreal and RAGE bake geometry**, because a car has to drive on the road. This engine
is an interactive physics simulation, and the frame budget is the thing that must not move -- so
the answer is GEOMETRIC, decided by both references and by the target, not a preference.

And a geometric clip against a REGULAR grid is not a clip. It is a RASTERISATION: walk the cells
the polygon covers by scanline, emit whole cells inside, clip only the boundary cells. That is
O(covered cells), it is the most optimised routine in all of computer graphics, and it replaces
O(all triangles x passes).

## What will be true

- the resolution of a patch is READ from the grid rung rather than approached by halving
- a surface meets the ground by walking the cells it covers, not by testing every triangle
- the primitive (`Divide`) stays; the loop around it goes
- the number this has to beat is the current `told.RefineMs`, quoted here before the work begins

## What will show I was wrong

`ground: of that, refining` in the ledger, and the preload total beside it. If the rasterising walk
is not faster on all eight places, the iteration was not the cost and this item was a guess. The
number is not recorded yet -- taking it is the first step, not the second.

## Not in this item

board:2116 -- `Refine` names four unrelated things and a sweep over the word would hit all four.

## It is a TESSELLATED PROJECTED GRID, and that is the whole item

Named 2026-09-04, and it reframes everything above. The three passes this item was circling --
`Refine` (split edges until fine enough), `Cut` (find where seams cross faces), `Sew` (stitch the
holes) -- are not three problems. They are one problem stated backwards: the mesh is built
irregular and then REPAIRED into coherence.

A projected grid is coherent BY CONSTRUCTION. A regular lattice is projected onto the surface and
displaced by the height field; neighbouring cells share vertices because they are neighbours in the
lattice, so there is no seam to find and none to sew. Resolution comes from the lattice's own
level, which is indexed rather than approached.

## What the three passes cost today, measured on OldTown

```
  ground: of that, refining               87 ms
  ground: of that, cutting the seams       71 ms   (was 146)
  ground: of that, sewing them             98 ms   (was 692)
  heap taken under ground-yield          2787 MB   cumulative, four rebuilds
```

**All three go to zero under a projected grid**, and so does most of that heap: the allocations are
`next.reserve(index.size() * 2)` once per pass per rebuild and a fresh `CellGrid` per pass, both of
which exist only to rebuild an index that a lattice never rebuilds.

## What Unreal does, what RAGE does

Unreal's Landscape IS a regular grid per component, displaced by a heightmap, with LOD as lattice
level -- the seams between components are a KNOWN, ENUMERATED edge case, not a search. RAGE's
terrain is likewise gridded and baked. **Neither searches for seams**, because neither creates
them. That agreement closes the question of shape; what is mine is the resolution rule, since the
world arrives over the wire.

## What will show I was wrong

`ground: of that, refining`, `cutting the seams`, `sewing them` all reading 0.000 ms because the
passes no longer exist, and `heap taken under ground-yield` falling by the order the arithmetic
above predicts. If a projected grid still needs a seam pass, the construction was not coherent and
this item misread the problem.

## WHERE IT LIVES: the engine core, beside the shadow

Decided 2026-09-04. Projecting a lattice onto a surface and tessellating it is a VERB, not a
subject -- the same kind of thing as a shadow. A shadow is not owned by whatever casts it; the
engine computes it and every subject gets one. A projected grid is the same shape of answer:

  - the ENGINE provides `project a lattice onto this field at this resolution`
  - a GENERATOR asks for it and receives geometry -- terrain, water, and anything else with a
    surface to cover

This follows the rule already written down for the seam solver: generic MATHEMATICS and PHYSICS
belong to the engine, and what NAMES something stays in the generator. A lattice, a projection and
a displacement name nothing; `TreeFoliage` and `BuildingShape` do.

It also settles who else may use it. `water/` covers a polygon with a surface and today has its own
path; a projected grid is the same verb at a different resolution with a different displacement.
Two callers is the number that makes the abstraction real rather than guessed -- and unlike the
seam relaxation, which had one caller and therefore stayed put, this one has two before it is
written.

## The measurement this rebuild is held to

A structural rewrite MOVES EVERY DIGEST, so the digest stops being the guard and the PIXELS become
it. `test/scripts/pixels.py` reads two shots and reports how many differ and by how much of 255.
Reference for the eight places is kept under `build/shots/reference/`. The bar: no pixel differs by
more than 1 of 255 unless the difference is looked at and named.

## The reference, looked up 2026-09-04, and what it corrects above

"Projected grid" is HALF the answer and the half it leaves out is the one the loop exists for.
A lattice projected onto a field settles RESOLUTION. It does not settle whether a building's
outline or a kerb lands on a triangle EDGE, and that is what `Cut` and `Sew` are doing: a grid
edge crosses the outline, so the triangle is split and the halves are stitched back.

The standard answer names it: a footprint or a kerb is a BREAKLINE, and the structure that carries
one is a CONSTRAINED DELAUNAY TRIANGULATION. ArcGIS's TIN documentation states the property this
whole item is about -- "using constrained Delaunay triangulation, no densification occurs, and each
breakline segment is added as a single edge" -- against the unconstrained case, where "breaklines
are densified by the software with Steiner points" and one input segment becomes many. PostGIS
ships it as `ST_TriangulatePolygon`; CGAL's 2D CDT is what Intergraph's LPS uses for exactly this
(terrain segmentation and building footprint delineation); the technique is twenty years old and
in every GIS.

So the four phases map onto ONE structure:

| here | with a CDT |
|---|---|
| grid, then `Refine` until the edge is approached | the grid's points are input points |
| `Cut` where an outline crosses a triangle | the outline is a CONSTRAINT, and therefore an edge |
| `Sew` the halves back together | there is no seam to sew |
| `Press` | -- |

Refine/Cut/Sew is the detour taken when an edge cannot be REQUIRED. Requiring it is the primitive,
and it is one this tree does not have yet.

MEASURED 2026-09-04, Kaiserberg, one rebuild: refining 94 ms, cutting the seams 93, sewing them 81,
pressing 42, counting 12 -- 322 ms of a 1051 ms rebuild, and the rebuild is what stands between
this tree and both halves of goal 3 (the preload is 1.9-4.3 s of which roughly half is compute, and
the same rebuild is what a moving camera used to trip over).

The bar stays as written above: pixels, not digests.

Sources: ArcGIS "Fundamentals of TIN triangulation"; PostGIS `ST_TriangulatePolygon`;
Wikipedia "Constrained Delaunay triangulation"; GeometryFactory, CDT for photogrammetry.
