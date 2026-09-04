Type: bug
State: active
Area: world, engine, render, generators
Tags: measured, memory, performance, determinism, owner
Supersedes: 2112

# A tile is baked by a WORKER into its own scratch and handed over WHOLE

**Benchmark** -- RAGE: a streaming thread builds one map section into a buffer, swaps the
completed buffer in, and reuses the buffer; the render thread only ever sees finished sections.
Unreal: a cook per level chunk, the resource created on the RHI thread, the render thread sees
finished resources; `FMemStack` with `FScopedMemMark` hands a worker scratch that is RESET, never
freed. **Both agree** on three things at once: the peak is one cell, the handover is a snapshot
by construction, and the frame only places what is already built.

## Where it stands, measured 2026-09-04

```
  Shibuya   856.7 MB held with the ceiling lifted, 668.9 MB of it buildings (618 raised geometry)
  CentralPark   fields 281 + frame copies 168 = 449 MB of a 933 MB PEAK
  the stored vertex                     20 bytes, asserted (StoredVertex.h)         DONE
  a distant building is coarser         RaiseLump has a caller (BuildingField.cpp:513) DONE for Massed
  the ground tile                       carries a cluster DAG, selected in the frame  DONE (18fd6806)
```

What holds the peak is the BAKE'S GRAIN, not the geometry's size: `BuildingField::Built_` is ONE
`Raised` for every building of every tile; nothing empties it; a `std::vector` doubling past a
power of two holds one and a half worlds at that instant. After the handover only the INDICES
are read off it -- positions come from the frame copies (`Surrounds::WallPlaces/RoofPlaces`),
a SECOND copy the renderer reads and the sim appends to. Buildings are meshed synchronously in
`GroundStack::Restand` -> `Footprints_.Build` (`GroundStack.cpp:128`) from the frame path.

And the meshers allocate per call: `BuildingMesh` nineteen local vectors, `RoadMesh` eight, once
per building -- 24 000 allocations per region at OldTown. Not the frame's cost (0.1 %); the
invariant, and what makes a mesher unsafe from two workers.

## The solution -- one change, not three

The unit is the VECTOR tile, which the bake is already grained to (`Build()` works on
`next.Tile`). What changes is who holds the buffer:

- a WORKER bakes ONE tile to completion into a `Scratch` it owns -- `Mesh(plan, scratch, into)`,
  the shape `SubjectProxy::SubjectScratch` and `OccupancySink::Storage` already have; the
  scratch is RESET between tiles, never freed, because remains would make the result depend on
  call order
- the finished piece -- a `StoredVertex` run and its indices, per tile -- is HANDED OVER whole
  through the same queue `TilePool` already hands terrain tiles through
- the renderer's side is an ARENA of tile-sized pieces, placed and released by tile; the
  `Raised` accumulator and the four frame-copy vectors stop existing rather than being freed
  sooner
- the fine mesh and its proxy are produced in the SAME bake (`Fine` and `Massed` both), so the
  frame CHOOSES and a distant building is coarser, never absent (board:2123)

Three things fall out: the PEAK is one tile's working set; the HANDOVER is a snapshot by
construction; the REBUILD leaves the frame path because the frame only places finished pieces
(board:2124). Determinism: pieces are placed in the watermark's DECLARED order whatever order the
workers finished them in.

## What will be true

- [ ] `BuildingField::Built_` is gone; the CPU holds footprints and per-tile pieces in flight
- [ ] The frame copies are gone; the renderer reads an arena the sim never appends to
- [ ] Shibuya preloads under the 512 MB ceiling and draws; `world: the buildings` at OldTown
      under 5 MB
- [ ] The meshers allocate nothing per call, counted with the tagged heap before and after
- [ ] Pieces land in declared order: a case shuffles worker completion and the picture holds
- [ ] Negative control: append one tile to a world-sized vector again and the peak ceiling goes
      RED

## Ruled out, measured

- "let the mesher write into the frame copy": wrong the moment two threads exist -- a worker
  writing into what the renderer reads is the live-state defect by name
- 32 -> 20 bytes per vertex: right, and Shibuya stayed red, because the peak is the grain
- a distant building DELETED: a skyline that flickers; both references refuse it
