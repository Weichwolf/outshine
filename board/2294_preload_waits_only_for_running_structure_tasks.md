Type: performance
State: active
Architecture: ready
Parent: 2285
Depends:
Priority: P0
Area: engine, streaming
Tags: startup, structure, scheduler

# Preload waits only for running structure tasks

## Evidence

A fresh offline Hockenheim still at 74.850 s reached Playable with 538 store
deliveries, zero provider starts and all 16 terrain/49 vector tiles arrived.
Its Refined preload then timed out at 120 s: 116.417 s in 2245 structure waits,
zero structure wake signals, 25/47 bakes landed, 18 discarded, four queued.
The measured bake mean was 7.98 ms. `StructureBuildQueue::AwaitSlice` waits
whenever `Queue_` is nonempty, including entries whose worker task finished
but whose landing is still pending. This is a candidate explanation for the
missed work; the no-signal ledger must fall after the change.

## Contract and ownership

`StructureBuildQueue` owns the distinction between queued candidates and
running worker tasks. `AwaitSlice` waits only when at least one queue entry has
an active task. Finished entries and tasks awaiting publication return
immediately so `PumpPreload` can advance publication. Do not increase
concurrency or lengthen the 50 ms wait slice to hide the problem. Preserve
bounded candidate admission, ordered landing, cancellation and revision checks.

## Falsifiable acceptance

- A queue with completed but unlanded candidates does not sleep for the full
  requested slice; a running task still wakes on completion. A scene with
  incomplete ground times out cleanly, without spin or unbounded work.
- Repeat offline Hockenheim at 74.850 s with `--probe-pixel`: Refined succeeds
  within the 120 s diagnostic limit or report the remaining wait class and
  open a separate WI. Compare store/provenance and image state to the paced
  route; no source change or place-specific fast path.
- Focused scheduler/route tests, `make format`, `LINT_JOBS=2 make lint` pass.
