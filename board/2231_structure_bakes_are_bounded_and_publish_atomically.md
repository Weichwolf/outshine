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

1. **P0-A [done]:** `StructureBakeProgress` owns its mutable aggregate. No accessor exposes it;
   only successful finalization moves a `BakedTile` into the task's optional completed product.
   The 257-structure scheduler case has no product after four ranges and one after the fifth.
   `TileChangesPublishAtomically` separately proves complete B replaces A only at candidate commit.
   Do not add a public stepping API or a timing-dependent whole-Engine range observer.
2. **P0-B [done]:** The same 257 inputs run uninterrupted and in five ranges. Ordered footprints,
   contact arrays, counters, geometry digest and native runs match exactly.
3. **P0-C [done]:** Expose range count, maximum structures/range, maximum range CPU time and final
   clustering time through the bounded preload diagnostic. It reads completion snapshots only.
4. **P0-D [measured, no split]:** Finalization is 3.736 ms against a 4.317 ms maximum worker task
   in the cold Place run. It does not cause the timeout. Reconsider resumable clustering only when
   a measured finalization exceeds the worker budget; preserve source order and atomic publication.

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

Range traversal and finalization are separate generator phases. The scheduler reports completed
ranges, 64 structures/range, maximum range time and maximum finalization time from consumed worker
snapshots. Its 257-structure test observes four ranges, then one short range and finalization.

2026-09-21: `ScoreAFootprintStandsOnALevelFloor` reached 4,415 pads and 0.000015 m contact error.
An immediate warm run preloaded in 2,973 ms. A genuinely empty-cache run timed out at 15,163 ms:
all seven bakes landed, maximum range/finalization/task were 0.940/3.736/4.317 ms, while 76 tile
requests remained and height admission had deferred structures 889 times. Ground first completed
at 7.359 s. Cold tile residency is the blocker; neither run exposes an intermediate public candidate.

2026-09-17: the prior Graz bake-timeout diagnosis was stale. With the candidate path, all
structure landings complete and the no-vegetation client capture passes: 1,940,223 building
triangles, 120 frames, p99 5.07 ms, zero bare tiles. The opened `Graz-c725aa8e.png` visibly
contains the city. Its row-neighbour variation is 0.6689 rather than the old, falsely required
1.0; the blank-frame control is 0.0. The test now rejects only pictures indistinguishable from
that measured control. Worker-task time is reported separately from range time for the next
actual dense-tile regression.
