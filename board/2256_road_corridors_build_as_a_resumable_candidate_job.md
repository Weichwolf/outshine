Type: defect
State: active
Architecture: ready
Parent: 2234
Depends: 2245
Priority: P0
Area: generators, road, engine
Tags: realtime, roads, determinism, ownership

# Road corridors build as a resumable candidate job

## Defect

Wien's Refined shot `49440d93` reports simulation p99 81.30 ms. The native
`Corridors::Lay` call alone takes 637.622 ms on the Engine thread for 55,021
edges, 16,573 junctions and 215,279 paved stations. Its design-lane pass takes
94.165 ms, `ShapesJunctions` 174.848 ms and paving 275.692 ms, including
193.510 ms for yields. The renderer path separately takes 71.851 ms to hand
scene geometry over; neither cost is explained by the vector snapshot assembler.
A temporary Wien probe measured that assembler's 13 four-tile slices at at most
2.513 ms, versus its former 34.271 ms one-shot publication. These are samples,
not an enforced worst-case budget. Probe logging changed scheduling and the shot
digest, so production image comparisons must run without that instrumentation.

`Corridors::Lay` owns transient `Paved`, `RoadRaised`, shared-node counts, notes and
yields on its stack. The caller cannot pause, cancel or retain a partial result;
the entire 637-ms call happens inside one candidate frame. This violates the
16.67-ms frame target even when total work is otherwise acceptable.

## Decision

Make corridor construction a move-only, candidate-owned generator job. Keep
`Paved`, `RoadRaised`, shared-node counts, ordered notes, corridor yields and
phase cursors inside it. Pin the input `OsmField` publication, `StreetField`,
network, class structure, tangent frame and DEM field owner for the job's lifetime;
no borrowed span or callback may outlive its owner. Cancellation discards the
whole job. A revised source starts a new job; no partial road geometry is visible
to renderer, navigation or earthworks. Keep `Corridors::Lay` as an independent
one-shot oracle until exact output equivalence is proven.

Resume in source order: crossings/decks, designing lanes, splitting edges,
shaping junctions, paving lanes, raising junction bodies, handing geometry over.
Slice loops by bounded work units, first lanes and junctions; decompose the
remaining whole-phase operations after measurement. Preserve ordering of edges,
stations, junction legs, yields, geometry indices and diagnostic reductions.
`ShapesJunctions` needs an explicit stable node order rather than unordered-map
iteration. Retain the complete candidate's terrain and OSM snapshot while the
job runs. Publish only after the final geometry and yields pass validation.

## Acceptance

- One-shot and interrupted runs produce identical native geometry, ordered
  yields, bridge clearances, network contacts and diagnostics for small analytic
  roads, stacked crossings, bridges and Wien; vary lane/junction slice sizes.
- A revision change, cancellation or late DEM refusal leaves the prior published
  world intact. No stale job writes into the new candidate.
- Measure every phase slice and full frame p50/p95/p99 plus CPU/GPU peaks. No
  corridor unit exceeds the declared per-frame budget on Wien and Malcesine;
  720p60 target remains a separate whole-frame acceptance.
- Keep `GroundCandidatePacingReachesReadiness`, Floor/Lattice and place render
  contracts unchanged. Open resulting PNGs, compare against prior complete
  shots and run `make format`, focused suites and `make lint`.

## Current evidence and remaining work

The candidate now owns a resumable corridor job. Wien's 47,645 ways produce
582,932 street triangles; the interrupted run rendered the same `0257fdae`
shot as a same-tree one-shot run and also reproduced the earlier `49440d93`
shot. Both images have 2,920,352 total triangles. A diagnostic same-site
oracle exposed a missing lane-cursor reset after bridge raising: before the
fix, the job completed with zero street triangles and yields despite green
pacing tests. That transition is fixed; the PNGs were opened. Current
`ScoreAFootprintStandsOnALevelFloor` passes in normal and validated variants.
`GroundCandidatePacingReachesReadiness` exposed a corridor field miss near east
365.028 m, north -1096.280 m: the candidate asked at zoom 15 while source DEM
is zoom 14. `HeightSheets::PrepareFields` now also requests parent DEM tiles
and their halo. A subsequent normal and validated Pacing run and the focused
HeightSheets rim case pass; repeat under changed source arrival schedules before
calling this resolved. No fabricated drape height was introduced. `make lint`
passes with clang-tidy after this change.

Wien's prior longest measured corridor slice was 28.120 ms: geometry transfer
was the largest phase (29.576 ms in an earlier run); bridge-end raising reached
16.529 ms. Geometry transfer is now split into material/part, positions,
normals, colours, triangles and validation stages. Measure each on Wien and
bound any remaining over-budget stage, especially winding validation and
bridge-end raising. Prove one-shot/job equivalence for geometry and ordered
yields directly, then open a fresh Wien PNG. The prior shot's simulation p99 was 23.97 ms
and worst frame 542.29 ms, so neither the corridor nor the whole-frame budget
is accepted yet. Diagnose those costs separately; do not infer a frame bound
from the matching still image.
