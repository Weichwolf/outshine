Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: generators, road, engine
Tags: realtime, roads, determinism, ownership

# Road corridors build as a resumable candidate job

## Defect and evidence

The old `Corridors::Lay` consumed 637 ms in one Wien candidate frame. The
candidate-owned `Corridors::Job` now paces topology, bridge treatment, lane
and junction construction, paving and geometry transfer. It pins explicit OSM,
DEM, network and class inputs; changed vector revisions are rejected. The
crossing/bridge oracle proves one-shot equality at two interruption schedules.
The current Wien shot retains `2fc0aec4`, with 47,645 ways and 582,932 street
triangles. The PNG was opened; geometry is stable but materials and lighting
remain visibly below target.

The current full corridor phase peaks at 21.60 ms although its generator
advance peaks at 13.01 ms. Job admission is below 0.001 ms, product inventory
0.001 ms and DEM field release 0.39 ms. `GroundBuildState::FinishesCorridors`
destroys the finished job synchronously and took 14.85 ms. Earlier telemetry
sampled before this destruction and understated the corridor maximum. Six of
4466 full Wien frames exceed 16.67 ms; other phases also need attribution
under WI 2234. A matching image alone does not prove a frame bound.
The completed-job path now drains nested scratch in candidate-owned advances.
Wien remains `2fc0aec4`: retirement peaks at 0.17 ms, the full corridor slice
at 11.54 ms, p99 at 9.81 ms, six of 4659 frames late and heap peak 1096 MB.
Malcesine remains `07ca3a25`: retirement 0.03 ms, corridor slice 3.31 ms.
Both PNGs were opened. A changed revision now moves the old candidate into
the single retirement slot, which blocks new admission until scratch retires.
The canceled-candidate path and destruction of its other products still need
an independent latency test.

## Contract and ownership

`GroundBuildState` owns the move-only job through construction and retirement.
The job owns `Paved`, `RoadRaised`, bridge topology, ordered yields and phase
cursors. No callback or span outlives its candidate input. The completed
geometry and ordered yields move to candidate products exactly once; retirement
may then release job scratch without touching those products. Cancellation or a
stale source sends private scratch through the same bounded retirement path;
no partial road geometry reaches renderer, navigation or earthworks. The engine
retains at most one retired candidate and applies backpressure.

Keep `Corridors::Lay` as an independent one-shot oracle. Preserve edge,
station, junction-leg, yield, index and diagnostic order across arbitrary
advances. Do not move the 14.85-ms destructor to publication or another frame.
Expose a bounded `Job::RetireStep` that releases nested vectors/maps in measured
chunks, then destroys the empty job. Hold the candidate in `NeedsCorridors`
until retirement completes. Canceled candidates use the engine-owned single
retirement slot before destruction. The transition must be explicit and replayable;
retirement timing cannot alter the native geometry or road network.

## Implementation and acceptance

1. In `src/generators/road/Corridors.*`, inventory job-owned allocations after
   `Done`. Retire nested `Paved` station/yield/junction arrays, maps, topology
   and stream scratch with a cursor and bounded count/time per advance. Record
   the longest release unit and retained bytes. No unbounded container clear,
   map destruction or large destructor on the frame path.
2. In `src/engine/Laying.cpp`, move final ordered notes/yields once, then enter
   the retirement phase. Advance it at most once per frame before completing
   `NeedsCorridors`. The single retirement slot handles canceled jobs; verify
   that subsequent candidate admission waits and teardown stays within budget.
3. Prove one-shot/interrupted equality of geometry, ordered yields, contacts
   and diagnostics on small roads, stacked crossings, bridges and Wien. Reject
   a changed vector revision and a DEM field miss without fabricating heights.
   `GroundCandidatePacingReachesReadiness` must pass normally and with NDEBUG.
4. Render Wien and Malcesine through outshine-client, open PNGs, preserve
   `2fc0aec4` and `07ca3a25`, and report corridor retirement max, whole
   corridor max, full-frame p50/p95/p99 and heap peak separately. Run
   `make format`, focused suites and `LINT_JOBS=2 make lint` on the commit.
