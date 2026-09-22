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
yields directly. The fresh Wien PNG `49440d93` was opened: road and city
placement persist, but broad flat roofs and weak material/light variation
remain visually below target. The shot has 2,920,352 triangles, simulation
p99 22.12 ms, worst frame 531.07 ms and 50/4435 frames over 16.67 ms.
Neither corridor nor whole-frame budget is accepted yet; a matching still
image does not prove a frame bound.

Wien `shots --measures` isolates the next two units: bridge-end raising
17.574 ms and the 12-pass ramp relaxer 16.541 ms; transfer phases peak at
5.754 ms. Keep the same ordered relaxation and cap, but resume one complete
pass per frame. Move bridge topology cleanup out of the final raise slice.
Recheck direct products, shot digest, slice maxima and whole-frame timings;
this only addresses those measured units, not the separate 502-ms worst frame.

After the split, Wien stays at `49440d93`; the PNG was opened. Longest measured
raise, cleanup and ramp slices are 0.202, 11.576 and 2.169 ms respectively;
the whole corridor maximum is 11.577 ms. The job takes 1057.702 ms total,
spread over frames. Shot simulation p99 is 21.98 ms with a 531.30-ms worst
frame, so WI 2234 still owns the larger stall. Pacing passes in both variants.
The HeightSheets dependency-admission test failed once on its posting bound
and then passed alone; WI 2244 tracks that unresolved scheduler observation.
Another Wien run measured bridge cleanup at 17.183 ms, above the frame budget;
erase topology nodes incrementally before releasing the remaining storage.
Three subsequent Wien runs kept the shot at `49440d93`; the PNG was opened.
Cleanup peaked at 1.581/1.647/1.508 ms, and the whole corridor slice at
10.819/11.618/11.475 ms. Pacing passed normal and validated. Whole-frame
simulation p99 still ranged 19.45–23.99 ms with 528–610-ms worst frames;
correctness and per-corridor pacing do not close the engine-wide frame claim.
