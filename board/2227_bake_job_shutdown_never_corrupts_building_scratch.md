Type: bug
State: active
Area: engine, generators, world
Parent: 2105
Depends:
Tags: ownership, streaming, shutdown

# Bake-job shutdown never corrupts building scratch

## Problem

A structure bake owns `Raw`, `Output` and `Scratch` until its task has landed. The owners must
outlive every worker. `Tasks::Done` consumes its completion token. `StructureBakes::NextLandings`
previously consumed a current job's token while probing staleness, then consumed it again while
landing. The second probe failed and shutdown blocked forever in `Wait` on the spent token.

## Decision

`StructureBakes` owns queued bake storage and calls `Clear` before its task pool is destroyed.
`Clear` requests cooperative cancellation, then waits before releasing any job storage.
`NextLandings` tests revision before consuming a completion token; a current completed job is
therefore consumed exactly once, by its landing path. A stale job consumes its token only when it
is ready to discard. Do not make `Tasks::Done` non-consuming: one explicit consumer per result is
its ownership contract.

## Proof

- Source audit: `Clear` sets every stop flag, waits for each unconsumed handle and only then destroys
  queue, raw input, output and scratch. `NextLandings` consumes a current completion only on its
  landing path; stale work consumes it only when discarded.
- No checked-in Floor-Contact or scratch-lifetime sanitizer oracle exists. Add a focused task fixture:
  hold a worker in its scratch, call `Clear`, release the worker and prove destruction returns;
  a deliberately early release must fail under ASan/UBSan. Until then this repair is implemented but
  not accepted.

## Next implementation

Do not add a friend or test-only insertion path to `StructureBakes`. Extract its private `Job` into
an internal move-only `StructureBakeTask`: it owns raw tile, heights, output, scratch, progress,
stop flag and completion handle. `Start`, `Resume`, `RequestStop` and `Join` express the actual
worker lifetime; only `Join` releases payload ownership. `StructureBakes` keeps admission, ordering
and landing. The focused test constructs this real task with a blocking mesher, verifies `Clear`
waits for its completion, then runs the deliberate premature-release process under sanitizers.

## Priority

The ownership repair is independent of renderer-world replacement: workers own only their job
storage, and `Clear` joins them before that storage is released. It therefore must not block
WI 2231. Keep this WI active only until the focused cancellation test is present; it is a
correctness check, not a prerequisite for bounded range scheduling.
