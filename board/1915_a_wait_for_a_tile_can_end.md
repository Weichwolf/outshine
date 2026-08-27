Type: bug
State: active
Parent: 1890
Area: world
Tags: measured, threading, tiles

# The waiter the drive actually uses is proven by a case, not by inspection

**Benchmark** — Unreal: automation proves behaviour, not inspection. RAGE: the same. **Both agree** — what a drive actually waits on is a measurement, not a reading of the source.

**Two of this item's three defects are repaid at HEAD and the third is not.** 873f8f65 announced
this item closed and never deleted the file; that divergence is board:1938's subject, and what
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

The thread control is the item, and **half of it now exists**.
`outshine/geo/ScoreWhenAWaitForATileEnds` builds a `TilePool` on a `Holding` transport the case
drives, runs `MeshAwaited` on its own thread, and measures with a deadline:

    with the transport holding, the call has returned: no -- it is waiting
    after release, the wait ended: yes after 0.013 s (budget 5.0 s)
    the transport was asked 242 time(s)

**And the negative control says what it does NOT reach.** Deleting the repaired half of the
predicate -- `Posted_.find(key) == Posted_.end()` -- leaves the case GREEN. With the transport
answering `Working` the worker POLLS rather than giving up, so it never takes the Pending branch
at `TilePool.cpp:455-458` that erases a posted job and notifies with `Done_` empty. After release
the tile lands through `Done_` like anything else.

So the instrument stands and the residual is narrower and named: a transport that makes the
WORKER give up, not one that makes it wait. That is a different fake -- one whose `Collect`
returns `Unreachable` on a schedule the case sets -- and it is what is left of this item.

## What will be true

- [ ] A case stands a real `TilePool` and awaits a mesh whose bytes the case controls, and the
      wait ends with the reason, inside a declared bound.
- [ ] Negative control: the notify removed from the `Pending` path, and the case fails on its
      bound rather than hanging.
