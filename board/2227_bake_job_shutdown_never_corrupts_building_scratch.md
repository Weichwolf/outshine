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

- `ScoreAFootprintStandsOnALevelFloor` reaches the legitimate 15 s ingestion failure and exits as
  `UNPREPARED`, never as a 120 s timeout or signal.
- A completed landing and repeated preload-timeout destruction preserve scratch exclusivity under
  ASan/UBSan.
- A deliberately premature scratch release is detected by the sanitizer test; normal shutdown has
  no allocator finding, trap or leak.

## Priority

The ownership repair is independent of renderer-world replacement: workers own only their job
storage, and `Clear` joins them before that storage is released. It therefore must not block
WI 2231. Keep this WI active only until the focused cancellation test is present; it is a
correctness check, not a prerequisite for bounded range scheduling.
