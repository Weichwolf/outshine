Type: bug
State: withdrawn
Area: world, compositor, render
Tags: measured, memory, owner

# The world holds only what the renderer draws and the physics touches

**Benchmark** -- RAGE: the render mesh goes to the GPU and the CPU keeps a `phBound`, a SEPARATE
and coarser collision body; a building is a handful of extruded convex prisms and never its wall
triangles. Unreal: the same split by another name -- `UStaticMesh`'s render data lives in
`FStaticMeshLODResources` (GPU) and collision in `UBodySetup`'s simplified convex elements, and
`bAllowCPUAccess` is OFF by default precisely because keeping the render vertices addressable
doubles the cost. **Both agree**: two representations, and only the cheap one stays resident on the
CPU. Neither keeps a bounding box ALONE -- a contact needs a surface, and a world whose physics is
box-only is not an interactive physics simulation.

## Measured 2026-09-02, OldTown -- the SMALLEST of the nine places, 349 766 triangles

`shots --measures OldTown`:

| what | bytes |
|---|---|
| **buildings** | **48 628 368** |
| land classes | 33 467 027 |
| OSM features, raw | 5 247 164 |
| streets | 328 336 |
| water | 52 880 |
| **the world's fields, together** | **87 723 775** |
| the ceiling they stand under | 536 870 912 |
| `subjects, device bytes` (GPU) | 189 862 252 |

**Fifty-five per cent of the world's CPU memory is building geometry that has already been handed
to the GPU.** `BuildingField::Built_` is a `Raised` -- `WallCorners`, `RoofCorners` and two index
runs, all of it float vertex data -- and it is the RENDER representation, kept after upload.
Shibuya draws 2 732 059 triangles, nearly eight times OldTown's, and the same accounting applies.

**And the collision body already exists beside it, for free.** `BuildingField::Prints_` holds one
`Footprint` per building: a ring of points plus `BaseM`, `HeightM`, `SeatM`. That IS RAGE's
extruded prism. It costs a fraction of `Raised` and it is what a contact needs.

## Why the render data cannot simply be dropped today

`Picturing.cpp` reads `Footprints().Built()` on every world compose and hands the whole array to
the renderer as ONE buffer for the whole world. A tile that lands appends to that array and the
world is composed again -- so the CPU copy is the source the next upload is made from, and dropping
it would mean re-meshing every building whenever one arrives.

That is the actual defect, and it is the one both references solved the same way: **residency is
per TILE, not per world.** A tile is meshed, uploaded, and its CPU geometry released; the renderer
draws N tile buffers; a tile that leaves releases its buffer. Nothing is re-uploaded because a
neighbour arrived. The owner's words for it: "ein Stream je Tile der bearbeitet wird und sparsam
abgelegt werden muss. wir behalten nicht mehr als der renderer braucht."

## What will be true

- [ ] `world: the buildings` reads under 5 MB at OldTown -- footprints and their rings, no wall or
      roof corners
- [ ] Building geometry is uploaded per TILE and the CPU copy released as soon as the upload has
      landed; a tile arriving does not re-upload its neighbours
- [ ] The physics reaches a building through its footprint prism, and a case drives a subject into
      a wall and reports the contact -- a bounding box alone would pass a weaker test, so the
      case is written to fail against one
- [ ] Elevation stays CPU-side with its GPU copy, unchanged: `sampleHeight` is a door function and
      the ground query is on the simulation's side of the snapshot
- [ ] `world: the bytes its fields hold` is quoted for all nine places before and after, and the
      ceiling that may only fall is recorded from the after
- [ ] Negative control: keep one place's `Raised` alive after upload and require the recorded
      ceiling to go RED

## What is NOT in this item

The land classes at 33.5 MB are a second half and a different shape -- a raster the ground shader
samples, not geometry. Whether that belongs on the CPU at all is its own question and is not
answered here.
