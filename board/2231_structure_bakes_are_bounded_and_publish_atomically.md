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

## Acceptance

- A fixture with more than two ranges has no published footprint or geometry before its final range;
  completion publishes the identical ordered tile as the one-shot control.
- Cancelling between ranges or during a range releases no scratch early, publishes nothing and
  terminates boundedly.
- Per-range structure and elapsed-work limits are measured. The unchanged floor-contact Place
  becomes resident within 15 s without reducing geometry or extending its timeout.
- Small, empty and rejected structures preserve existing output/error contracts; lint passes.
