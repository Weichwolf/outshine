Type: defect
State: active
Parent: 2105
Depends:
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

## Next implementation

Priority P0. The production scheduler already carries `StructureBakeProgress`, takes 64
structures per range and runs at most four ranges per task. Prove that contract before changing
its constants: add a deterministic fixture with more than 256 structures and an observable
candidate world A. It must show that every intermediate completion leaves A's footprints,
pieces, revision and readback intact; only the final range yields B. Then make the per-range
structure cap and the measured task/slice maxima explicit diagnostics of the public preload
failure. If clustering exceeds the same bound, split only clustering into resumable slices with
the source order preserved. Do not split the published tile or relax the 15-second Place limit.

## Acceptance

- A fixture with more than two ranges has no published footprint or geometry before its final range;
  completion publishes the identical ordered tile as the one-shot control.
- Cancelling between ranges or during a range releases no scratch early, publishes nothing and
  terminates boundedly.
- Per-range structure and elapsed-work limits are measured. The unchanged floor-contact Place
  becomes resident within 15 s without reducing geometry or extending its timeout.
- Small, empty and rejected structures preserve existing output/error contracts; lint passes.

## Measurement

`StructureBakePropagatesMeshFailure` prüft 257 Strukturen gegen einen One-Shot: vier
vollständige 64er-Bereiche bleiben unvollständig, erst der fünfte schließt das Produkt,
und beide Ergebnisse sind identisch. Ein entfernter Tile bündelt seine zwei Gebäude in
genau einem `Massed`-Mesh; null Meshes war ein falscher, korrigierter Testvertrag.
Der Test beweist die Generatoraggregation, nicht die öffentliche Kandidatenpublikation.

2026-09-17: candidate-owned footprints and pieces remove the active-world handoff from the
preload critical path. The cited public Floor-Contact executable is not checked in; its former
3.55-second claim is historical, not current acceptance evidence.

2026-09-17: the prior Graz bake-timeout diagnosis was stale. With the candidate path, all
structure landings complete and the no-vegetation client capture passes: 1,940,223 building
triangles, 120 frames, p99 5.07 ms, zero bare tiles. The opened `Graz-c725aa8e.png` visibly
contains the city. Its row-neighbour variation is 0.6689 rather than the old, falsely required
1.0; the blank-frame control is 0.0. The test now rejects only pictures indistinguishable from
that measured control. Worker-task time is reported separately from range time for the next
actual dense-tile regression.
