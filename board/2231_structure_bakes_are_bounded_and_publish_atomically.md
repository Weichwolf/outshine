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

Preload creates an initial ground candidate to establish material resources, then defers its next
ground rebuild until every structure bake has landed. Intermediate footprint revisions remain
private; a frame never observes partially rebuilt terrain or a partial building tile.

## Acceptance

- A fixture with more than two ranges has no published footprint or geometry before its final range;
  completion publishes the identical ordered tile as the one-shot control.
- Cancelling between ranges or during a range releases no scratch early, publishes nothing and
  terminates boundedly.
- Per-range structure and elapsed-work limits are measured. The unchanged floor-contact Place
  becomes resident within 15 s without reducing geometry or extending its timeout.
- Small, empty and rejected structures preserve existing output/error contracts; lint passes.

## Measurement

2026-09-17: candidate-owned footprints and pieces remove the active-world handoff from the
preload critical path. `ScoreAFootprintStandsOnALevelFloor` passed its unchanged public
floor/road checks in 3.55 s. This proves the small contact case, not throughput for dense tiles.

2026-09-17: Graz still misses its 15 s residency limit with 6,255 structures: four queued bake
jobs complete 15 of 19 landings, mean 26.81 ms and maximum 126.05 ms. The work unit is therefore
still too coarse for dense OSM coverage. Measure range distribution and final clustering separately
before changing `kStructuresPerRange` or worker-task grouping; a smaller constant without a
throughput measurement is not a solution.
