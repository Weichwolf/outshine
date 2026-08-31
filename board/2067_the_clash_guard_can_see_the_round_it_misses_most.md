Type: bug
State: open
Area: engine
Tags: measured, gate

# The clash guard sees the round where nothing clashes and misses the one where everything does

**Benchmark** — Unreal: `FScopedDurationTimer` and the stats system attribute every counter to a
named frame, and a stat written outside a frame is attributed to no frame rather than silently to
the wrong one. RAGE: the telemetry channel opens and closes with the frame it describes.
**Both agree**: a counter belongs to a bounded interval that is opened and closed around the work,
never one that ends before the work starts.

## Measured

`Ledger::Opens()` bumps the round AND clears `Clashed_`. `Engine::advance()` calls it, then
`Updates()` -- which reaches `Grounds()` -- then `Tells()`, which publishes
`measures published twice in one round`. Inside one advance the guard therefore works.

**`preload()` never calls `Opens()`.** Its whole loop -- `Engine.cpp:368-390`, which calls
`Grounds(true)` on every iteration that the world looks ready -- runs in ONE round, and the first
`advance()` afterwards clears `Clashed_` before any `Tells()` has read it. So every clash raised
during preload is discarded unseen.

That round is exactly where `Grounds()` runs many times, and the ledger keeps the FIRST value per
round. Chasing board:2066 this cost hours: `class field: it published a structure` read 0 and
`the version the colours used` read -1 while a probe logging past the ledger showed the same
rebuild holding version 3 and naming 253 515 vertices. The guard said `measures published twice in
one round: 0`, and that zero was taken as evidence that the round was not the explanation. It was
the explanation, and the control could not see it.

**This is the trap CLAUDE.md names as costing most: a negative control that passes.** The guard
was built to catch a measure written twice in one round and is blind in the only round where that
happens.

## What will be true

- [x] The preload loop opens and closes rounds like `advance()` does, so a measure published in it
      belongs to a bounded interval. Then the ledger's first-wins rule stops silently freezing the
      first touch of the world into every number a picture reports. Done: `Engine.cpp` calls
      `Published.Opens()` at the top of each preload iteration, and `Opens()` no longer CLEARS the
      clash list -- a clash is a defect that happened, not a per-round counter. Exemplar, 2026-08-31:
      `streets: features it walked at all` read 0 while a probe past the ledger showed the same run
      walking 2; after the fix it reads 2. The frozen zero had already been believed once, in the
      commit that said a declared field lays nothing.
- [ ] Negative control that goes RED: publish one name twice with different values inside preload
      and require the guard to name it. Today that control passes green, which is why this item
      exists.
- [ ] Every measure `Grounds()` publishes is checked against this once the rounds are right --
      board:2063's bare tile count and board:2066's class field measures both read the first
      rebuild and both were believed.
