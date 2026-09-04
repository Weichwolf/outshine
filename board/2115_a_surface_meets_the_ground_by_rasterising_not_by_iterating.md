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
