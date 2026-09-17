Type: defect
State: active
Parent: 2105
Depends: 2227
Area: engine, generators, world
Tags: streaming, realtime, ownership

# Structure bakes are bounded and publish atomically

## Problem

A `RawTile` holds every building in one OSM tile. The focused floor-contact Place reached 32/36
bakes in 15 s while four queued jobs held 918 structures. Completed bakes averaged 1.37 ms, so the
remaining tile-sized work units, not normal meshing, violate bounded streaming work. A partial tile
must never publish: footprints, geometry and revision advance together.

## Decision

A tile bake owns one mutable aggregate and advances through bounded structure ranges. Each worker
invocation owns an exclusive scratch and completes one range, then reposts the same tile state until
its final clustering phase. Only a complete aggregate becomes a `Landing`; cancellation retains all
owners until the final worker returns. Range boundaries preserve source order and produce the same
accepted tile as one uninterrupted bake. Final clustering also has a declared bound or continuation;
no hidden unbounded tail remains.

Only the main thread reads range progress, after consuming the worker completion. Timeout diagnostics
therefore report a stable upper bound of remaining structures and the maximum completed slice time;
they never read mutable generator state while a worker owns it.

The initial preload phase resumes completed ranges before a ground snapshot is publishable. It may
advance private aggregates, but cannot hand off a partial tile; publication remains in the landing
phase.

One worker task owns at most four 64-structure ranges. It records the slowest individual range,
then yields the aggregate to the main thread. This bounds worker work while avoiding one full
engine-pump delay after every 64 structures.

## Acceptance

- A fixture with more than two ranges has no published footprint or geometry before its final range;
  completion publishes the identical ordered tile as the one-shot control.
- Cancelling between ranges or during a range releases no scratch early, publishes nothing and
  terminates boundedly.
- Per-range structure and elapsed-work limits are measured. The unchanged floor-contact Place
  becomes resident within 15 s without reducing geometry or extending its timeout.
- Small, empty and rejected structures preserve existing output/error contracts; lint passes.

## Measurement

2026-09-17: completed ranges now resume during initial preload. The floor-contact executable still
missed its 15 s bound at 15.47 s (23/27 tiles, four jobs, estimated 629 structures remaining,
maximum completed slice 4.65 ms). This disproves a costly single-range tail but does not yet prove
the full residency budget; scheduler throughput and tile admission remain open.
