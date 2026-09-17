Type: defect
State: active
Parent: 2105
Depends: 2231
Area: engine, world, streaming
Tags: scheduling, osm, realtime

# Vector decoding and structure admission share a compute budget

## Problem

The floor-contact Place receives all 48 terrain and 49 vector tiles with no outstanding IO, then
misses its 15 s residency bound with four structure bakes remaining. Forcing ingestion while vector
siblings are pending made the same Place worse: 18/22 bakes landed, mean bake cost 5.06 ms, versus
27/31 and 2.48 ms after vector settlement. Unbounded overlap steals CPU from the critical path.

## Decision

Model vector decode, field ingestion and structure ranges as explicit bounded work classes under one
streaming budget. Admit the next class from measured ready work and cost; retain deterministic tile
priority and cancellation. Do not infer readiness from zero network requests. Report queue depth,
CPU time and oldest-ready age without frame-path allocation or periodic logs.

## Acceptance

- A controlled fixture proves that bounded overlap neither starves vector settlement nor completed
  structure ranges; changing arrival order preserves accepted native-world data.
- The floor-contact Place becomes resident within 15 s with unchanged input, geometry and timeout.
- CPU time, queue bounds and admission decisions are measurable; focused tests and lint pass.
