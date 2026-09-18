Type: defect
State: active
Parent: 2105
Depends: 2231
Priority: P0
Area: engine, world, streaming
Tags: scheduling, osm, realtime

# Vector decoding and structure admission share a compute budget

## Problem

The floor-contact Place receives all 48 terrain and 49 vector tiles with no outstanding IO, then
misses its 15 s residency bound with four structure bakes remaining. Forcing ingestion while vector
siblings are pending made the same Place worse: 18/22 bakes landed, mean bake cost 5.06 ms, versus
27/31 and 2.48 ms after vector settlement. Unbounded overlap steals CPU from the critical path.

## Architecture decision

One engine-thread `StreamingAdmission` owns bounded ready queues for vector decode, field
ingestion and completed structure ranges. Each item has source identity/revision, deterministic
tile priority, measured previous CPU cost and a cancellation token. The admission decision takes
one fixed per-frame work budget and selects the highest-priority ready item whose estimated cost
fits; starvation prevention promotes the oldest-ready item only at deterministic boundaries.
Workers never mutate the native world. They return a private result; the admission owner validates
revision and publishes it through the existing candidate boundary. Queue telemetry is a snapshot
(depth, oldest ready age, admitted count, measured CPU time), sampled by tests or explicit
diagnostics; it has no periodic frame log or frame-path allocation.

## Implementation order

1. **P0, after 2231:** Introduce the admission record and a deterministic scheduler fixture with
   reversed arrival order. Accepted native data must remain identical.
2. Admit vector decode, field ingestion and one completed structure range through the same budget;
   retain the former world when a revision becomes stale or a task is cancelled.
3. Establish budgets from recorded slice measurements, then enforce the unchanged 15-second
   floor-contact and 240-km Lattice preload limits. No arbitrary sleep, worker-count change or
   timeout increase is a fix.

## Acceptance

- A controlled fixture proves that bounded overlap neither starves vector settlement nor completed
  structure ranges; changing arrival order preserves accepted native-world data.
- The floor-contact Place becomes resident within 15 s with unchanged input, geometry and timeout.
- CPU time, queue bounds and admission decisions are measurable; focused tests and lint pass.

## Current evidence

2026-09-18: `ScoreTheLatticeMeetsItselfAtALevelBoundary` exhausted its unchanged 15 s preload
window with `world ingestion pending, terrain classification pending`. It is currently reported as
unprepared, not accepted. The case has 240 km sight and is the controlled regression to use for
admission timing after the atomic multi-range candidate fixture in WI 2231 exists.
