Type: debt
State: open
Area: world, generators, engine, base
Tags: measured, memory, performance, owner
Supersedes: 2100, 2102, 2099
Depends: 2122

# A place costs what its geometry is worth, and a tile that leaves gives its memory back

**Benchmark** -- RAGE streams a heightfield and per-cell props into pools sized once; a cell that
leaves gives its memory back; the render mesh goes to the GPU and the CPU keeps a `phBound`, and
`sysMemAllocator` keeps the block's size in a header so returning it is a subtraction. Unreal:
World Partition loads and UNLOADS cells; `bAllowCPUAccess` is off so the render mesh is not
addressable from the CPU; `FMalloc::GetAllocationSizeUntyped` exists because the engine refuses
to ask the platform allocator on the hot path; `FMemStack` is a bump pointer reset per frame.
**Both agree**: the resident cost is the RING, the transient cost of a tile is that TILE, and
the CPU keeps the cheap representation only.

## Where it stands, measured 2026-09-04

```
  CentralPark   fields 281 MB + frame copies 168 MB = 449 MB of a 933 MB peak   (c3daf1cc)
  Shibuya       856.7 MB with the ceiling lifted; 668.9 MB of it buildings         (2122)
  the arithmetic for OldTown's ring                                       ~45 MB
```

What holds it:

| what | where | the defect |
|---|---|---|
| render geometry kept on the CPU after upload | `BuildingField::Built_`, one `Raised` for every building of every tile | board:2122 makes it a worker's scratch |
| a second copy the renderer reads | `Surrounds::WallPlaces/WallFacing/RoofPlaces/RoofFacing` | goes with the arena in board:2122 |
| nothing is ever released | `OsmField::Tiles_/Features_/Rings_/Points_`, `Footprints_`, `Built_` have no removal path; `Settled_` is a `vector<uint64_t>` searched linearly per ring tile per stand | a tile that leaves the ring is erased, and `Settled_` is a set keyed by `TileId` |
| coordinates stored as doubles two at a time | `OsmField::Points()` is `span<const double>`, read as `pts[i*2+1]` at twelve sites | `span<const LongitudeLatitude>`, per tile |
| the heap's own measure doubles the allocation | `Heap::Take` -> `malloc_size` on every allocation and again in `Returned` (`base/io/Heap.cpp:63-83`) | the size rides in a header before the block, RAGE's way; `Returned` subtracts and asks nothing |
| the ceiling cannot see a quarter of the spend | `GroundStack::HeapBytes()` sums five fields and not the frame copies | one accounting, the tagged heap, is the ceiling's source |
| the churn | one rebuild allocates a thousand times what it produces (`ground-yield` 3.5 GB) | board:2115 removes the passes; what remains writes into buffers sized once |

The physics half of board:2100 -- a body reaches a building through its footprint prism and never
its wall triangles -- is board:2127's.

## The solution, in the owner's three stages

> raw is TRANSIENT -> the generator builds per tile into a buffer sized before it is filled ->
> the engine holds what the renderer and the physics need

1. board:2122 first: per-tile bake, whole-piece handover, the CPU copy released on upload
2. then removal: `OsmField` keyed per tile with an erase; `BuildingField` and `StreetField` the
   same; a ring recentring by one tile erases one and bakes one
3. then the heap: size header, no `malloc_size`; the ceiling reads the tagged heap
4. the point type at the store: `Points()` per tile as `span<const LongitudeLatitude>`, which
   deletes twelve `[i*2+1]` readers and the last swappable pair

## What will be true

- [ ] `heap: bytes LIVE right now` is under 80 MB with a place standing, at all nine places
- [ ] `maximum resident set size` for one place is under 250 MB
- [ ] Moving the eye across ten tiles and back leaves `heap: bytes LIVE` where it started; a
      case lands one tile into a standing world and measures work and churn proportional to
      the tile
- [ ] The churn of one rebuild is under 4x the geometry it produces, per tag
- [ ] `Heap::Take` and `Returned` call `malloc_size` nowhere; `NoFramePathCallReachesABlock`'s
      physics seed no longer reaches it
- [ ] `world: the buildings` reads under 5 MB at OldTown -- footprints and rings, no corners
- [ ] Negative control: hold one released tile and the live-bytes ceiling goes RED

## Ruled out, measured

- an open-addressed map in `Refine`/`Cut` halved both passes and moved the byte churn by nothing
- reserving the mesh vectors to the announced count made the churn WORSE (3 572 -> 3 804 MB)
- `Drape::At` as the bottleneck: 4.4 ns per face, a quarter of a second, and it is a BVH now
- hoisting the junction map out of the levelling loop: one per cent
