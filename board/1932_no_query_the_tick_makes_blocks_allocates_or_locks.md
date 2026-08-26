Type: bug
State: open
Area: sim, world
Tags: measured, frame-path, performance
Regresses: 1924

# No query the physics tick makes blocks, allocates or locks

CLAUDE.md bounds the frame path: *no alloc/lock/disk/unbounded block*. The seam board:1924 built
puts all four on the physics tick, and board:1924's own ledger ticks the box that says otherwise.

## The chain, read at HEAD

    src/sim/DriveTick.cpp:160,167,168   beneath.At(...) -- up to THREE per off-made wheel
    src/sim/GroundUnderfoot.cpp:7       Stack_.Ground().At(lat, lon)
    src/world/ground/TerrainLoader.cpp:208   TileAt(hx, hy)
    src/world/ground/TerrainLoader.cpp:169   held.Stitched->StitchedGrid(...)   on a cache miss
    src/world/ground/TerrainLoader.cpp:102   Held_.Pool.BytesBlocking(request, &landing)
    src/world/ground/TilePool.cpp:190,204    for (attempt < 30000) ... sleep_for(1 ms)

`kPollAttempts = 30000` and `kPollMs = 1` (`TilePool.cpp:25-26`), so one wheel one tick may sleep
**30 seconds** waiting on an HTTP fetch. At four mounts and three queries each the worst case is
twelve such waits in one step. The frame budget is 16.7 ms.

Three more terms on the same path, each smaller and each still forbidden:

- **A LOCK.** `ClassField::Read()` takes `std::lock_guard<std::mutex>` (`ClassField.h:28-29`) and
  copies a `shared_ptr`; `ClassAt` calls it (`:41`) on every query.
- **AN ALLOCATION.** `TileAt` on a miss reaches `FillNodeHeights(..., &victim->H)`
  (`TerrainLoader.cpp:195`), which fills a `std::vector<float>`, and `FetchInto` does
  `out->Bytes.assign(...)` (`TilePool.cpp:196`).
- **A THIRD QUERY THAT BUYS A NORMAL THE FIELD ALREADY KNOWS.** `GroundSample`
  (`src/world/ground/GroundSample.h:6`) carries a height and a state and nothing else, so
  `DriveTick.cpp:165-180` reconstructs the normal by finite difference from two EXTRA `At()`
  calls at the post spacing. The tile the answer came from holds its own normals -- the
  patchwork reads them (`GroundPatchwork.cpp`, commit aeabda4e) -- and the sample throws them
  away. One query should answer height, normal and friction; today it costs three, and each of
  the three can block.

## A DEAD TERM IN THE SAME LOOP

`src/sim/DriveTick.cpp:158`

    const double atLat = way.FrameLat + (worldM[0] * 0.0 + armNorthM + northM) / way.PerLatM;

`armNorthM + northM` is `-worldM[2]` by construction (`:145`) and `armEastM + eastM` is
`worldM[0]` (`:144`), so `:158-159` re-derive two values already in hand, and `worldM[0] * 0.0`
is a multiply by zero standing in source that carries no comments.

## What will be true

- [ ] `Underfoot::At` answers height, NORMAL and friction in ONE call, from the field's own
      normals, never from a finite difference the caller assembles.
- [ ] A query on the tick that would have to fetch answers `Known = false` at once. Blocking
      belongs to the streaming path that opens the world, never to the step that integrates it.
- [ ] No lock and no allocation between `DriveTick` entry and exit, held by a case that counts
      them.
- [ ] Proving case: a drive whose wheels leave the made surface into a region whose tiles are
      NOT resident, with a declared per-step ceiling in milliseconds. Negative control: the
      blocking path restored, and the step overruns the ceiling by three orders of magnitude.
