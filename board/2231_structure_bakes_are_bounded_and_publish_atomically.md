Type: defect
State: active
Architecture: ready
Parent: 2105
Depends:
Priority: P0
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

## Implementation order

The production scheduler already carries `StructureBakeProgress`, takes 64 structures per range
and runs at most four ranges per task. Prove the existing contract before changing its constants.

1. **P0-A:** Add a deterministic public-Engine fixture with world A and 257 structures for B.
   Hold worker completion at an internal deterministic test barrier; observe the public
   state after each consumed completion (up to four ranges share one task). Read A's
   footprint set, pieces, revision and GPU output; all remain A until complete B commits.
   Do not expose worker-owned intermediate ranges or add a production public stepping API.
2. **P0-B:** Run the same input as an uninterrupted one-shot generator control. Source ordering,
   accepted structures and final native products must match exactly.
3. **P0-C:** Expose range count, maximum structures/range, maximum range CPU time and final
   clustering time through the bounded preload diagnostic. It reads completion snapshots only.
4. **P0-D:** If clustering violates the range bound, make only clustering resumable and preserve
   source order. Do not split a tile's publication or relax the 15-second Place limit.

## Acceptance

- A fixture with more than two ranges has no published footprint or geometry before its final range;
  completion publishes the identical ordered tile as the one-shot control.
- Cancelling between ranges or during a range releases no scratch early, publishes nothing and
  terminates boundedly.
- Per-range structure and elapsed-work limits are measured. The unchanged floor-contact Place
  becomes resident within 15 s without reducing geometry or extending its timeout.
- Small, empty and rejected structures preserve existing output/error contracts; lint passes.

## Files and gates

src/engine/streaming/StructureBuildTask.cpp, StructureBuildQueue.cpp, GroundWorldCandidate.h and
StructureTilePublication.h own private progress and publication. Reuse these owners.
Tests belong under test/outshine/include/Outshine/; analytical generator controls under
src/generators/building/StructureBake. 64 structures bounds count, not elapsed time:
one complex structure and final clustering need separate maximum/percentile measurements.
Commands: make format; make suite SUITE=outshine/src/generators/building/StructureBake;
make suite SUITE=outshine/integration/places/ScoreAFootprintStandsOnALevelFloor;
run the added public candidate case through make suite; make lint.

## Measurement

`StructureBakePropagatesMeshFailure` prüft 257 Strukturen gegen einen One-Shot: vier
vollständige 64er-Bereiche bleiben unvollständig, erst der fünfte schließt das Produkt,
und beide Ergebnisse sind identisch. Ein entfernter Tile bündelt seine zwei Gebäude in
genau einem `Massed`-Mesh; null Meshes war ein falscher, korrigierter Testvertrag.
Der Test beweist die Generatoraggregation, nicht die öffentliche Kandidatenpublikation.

2026-09-18: `ScoreAFootprintStandsOnALevelFloor` is checked in and exercises the public Engine
path. Its current run completed preload in 2,894.869 ms against 15 s, reached 4,415 building pads
and left at most 0.000015 m above a stamped floor or corridor. It proves the current Place budget
and contact contract; it does not yet expose an intermediate 257-structure candidate to prove
that no partial aggregate publishes.

2026-09-17: the prior Graz bake-timeout diagnosis was stale. With the candidate path, all
structure landings complete and the no-vegetation client capture passes: 1,940,223 building
triangles, 120 frames, p99 5.07 ms, zero bare tiles. The opened `Graz-c725aa8e.png` visibly
contains the city. Its row-neighbour variation is 0.6689 rather than the old, falsely required
1.0; the blank-frame control is 0.0. The test now rejects only pictures indistinguishable from
that measured control. Worker-task time is reported separately from range time for the next
actual dense-tile regression.
