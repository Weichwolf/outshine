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
