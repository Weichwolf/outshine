Type: bug
State: open
Parent: 1890
Area: world
Tags: measured, threading, tiles, door

# A wait for a tile ENDS, and an implementation that cannot wait says so

`TilePool::MeshAwaited` waits on a predicate that a legal outcome never satisfies, and the
interface lets a second implementation inherit the polling it was written to replace.

## 1. THE WAIT CANNOT END WHEN THE MESH ANSWERS `Pending`

    src/world/ground/TilePool.cpp:513   const uint64_t key = MeshKey(z, x, y);
    src/world/ground/TilePool.cpp:516   Landed_.wait(lock, [&] { return Done_.find(key) != Done_.end(); });

Unbounded, no deadline, and the only thing that ever satisfies it is a `Done_` entry. The worker
writes one on every result EXCEPT the one that matters:

    src/world/ground/TilePool.cpp:455   if (result.State == Reply::Pending) Posted_.erase(job.Key);

`Pending` erases the posting, writes NO `Done_` entry and calls no `notify`. The caller sleeps
until the process is killed. The path is reachable and named at every step:

    FetchInto  gives up after kPollAttempts = 30000 x 1 ms and returns Pending   (TilePool.cpp:190, 224)
    PoolTerrain::Take turns that into TerrainBytes::Waiting()                    (TilePool.cpp:284)
    RunMesh    turns Miss::Wait into Reply::Pending                              (TilePool.cpp:359)
    Work       erases Posted_ and notifies nobody                                (TilePool.cpp:455)

So a tile whose bytes take longer than 30 s hangs the caller FOREVER instead of coming back as
one pending tile of nine. That is the failure mode the item this one is parented to was fixed to
avoid, arriving through the fix. A failure here is meant to be LOUD.

## 2. THE DEFAULT IS A TRAP

    src/world/ground/TileMeshes.h:29   virtual Reply MeshAwaited(...) { return Mesh(...); }

`Around::Awaited = true` means "wait". With this default it means "wait if the implementation
happens to have implemented waiting" -- a different contract, entered silently. The next
`TileMeshes` gets board:1914's one-tile-of-nine back with nothing refusing. CLAUDE.md puts the
type system ahead of a checker: `MeshAwaited` is `= 0` and each implementation states its answer,
which costs the two that exist one line each.

## 3. WHAT GUARDS IT TODAY GUARDS OUR SHAPE, NOT THE CODE

`test/harness/outshine/geo/ScoreWhatAPatchworkReads.cpp:86` hands `LayPatchwork` a `Twice` whose
own `MeshAwaited` calls its own `Mesh` twice. The case proves that `LayPatchwork` calls
`MeshAwaited` when the flag is set -- an assertion about our call graph, not about a tile
arriving. `TilePool::MeshAwaited`, the code board:1914 changed, is not executed by any case. The
stride half of that same file is a proper oracle and stands; this half is a mock.

## What will be true

- [ ] A wait ends: either the tile lands, or the wait returns the reason it did not, with a bound
      the caller can read. The `Pending` result reaches the waiter rather than vanishing.
- [ ] `MeshAwaited` is pure virtual; no implementation inherits a poll while a caller reads a wait.
- [ ] Proving case: a `TileMeshes` whose worker answers `Pending` forever, awaited -- the call
      RETURNS with a reason inside a declared bound. Negative control: the unbounded predicate
      restored, and the case hangs rather than failing, which is why the bound is part of the case.
