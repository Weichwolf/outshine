Type: bug
State: open
Parent: 1890
Area: world
Tags: measured, threading, tiles

# The waiter the drive actually uses is proven by a case, not by inspection

**Two of this item's three defects are repaid at HEAD and the third is not.** 873f8f65 announced
this item closed and never deleted the file; that divergence is board:1933's subject, and what
follows is the residual only.

REPAID. `TilePool::MeshAwaited`'s predicate is now *the tile landed OR the job is no longer
posted* (`src/world/ground/TilePool.cpp:516-518`), so the `Pending` path that erased `Posted_`
and notified nobody no longer leaves a caller asleep. `TileMeshes::MeshAwaited` is pure virtual
(`src/world/ground/TileMeshes.h:29-30`), so no implementation inherits a poll while a caller
reads a wait.

STANDING. **Nothing executes `TilePool::MeshAwaited`.** Every hit in `test/` is a fake:
`ScoreWhatAPatchworkReads.cpp:81`, `:114` and `:239` are three hand-written `TileMeshes` whose
`MeshAwaited` answers what the case wants. Those arms prove `LayPatchwork`'s call graph and a
ring whose tiles never land -- real work -- but the condition variable, the worker's notify on
the `Pending` path, and the `Posted_`/`Done_` race are held by reading alone. The closing commit
said so: *"a case that provokes the exact worker race needs thread control the harness does not
have, and I did not build one."*

The thread control is the item. A `TilePool` fed by a transport the case drives -- one that holds
a request until the case releases it -- makes the worker's timing an INPUT rather than a hope,
and the same instrument proves board:1932's ceiling.

## What will be true

- [ ] A case stands a real `TilePool` and awaits a mesh whose bytes the case controls, and the
      wait ends with the reason, inside a declared bound.
- [ ] Negative control: the notify removed from the `Pending` path, and the case fails on its
      bound rather than hanging.
