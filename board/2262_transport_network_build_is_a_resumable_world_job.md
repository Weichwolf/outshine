Type: defect
State: active
Architecture: ready
Parent: 2133
Depends:
Priority: P0
Area: world, navigation, engine, streaming
Tags: osm, routing, realtime, determinism

# Transport network construction must not stall a moving frame

## Proven defect

After WI 2133 keyed network reuse by source revision, Wien's normal Refined
candidate runs `Corridors::MapOf` for 529.673/591.290 ms in one Engine frame.
Those runs' worst simulation frames were 530.97/592.67 ms. The graph has
47,101 ways, 155,084 nodes and 369,981 edges. This stage prevents continuous
camera movement even though road corridor slices are now about 10–12 ms.
The original `Corridors::MapOf` also owned logical map construction inside the
road mesher; it has been moved to `world/navigation/TransportNetwork::BuildOneShot`.

## Decision

Move native transport graph construction into `world/navigation`; the road
generator consumes a read-only graph snapshot. A candidate-owned, move-only
builder pins one `OsmField` generation, `StreetField`, DEM source and region.
Build in deterministic source order: ingest ways/points, create topology,
classify crossings, then attach elevation and navigation metadata. Expose a
bounded `Advance(work budget)` and cancellation, not a synchronous full build
in `BeginsGroundModels`. No intermediate graph can replace the published one.
OSM IDs/topology govern logical identity; height refinement changes 3D
alignment without silently creating or removing turns. A same-count OSM
revision must rebuild. Failures retain the old graph and return a source-ID
diagnostic; do not publish null or partial routing state.

Keep the current one-shot `MapOf` as an independent comparison oracle during
migration, then remove its generator ownership. First measure its Lay, Weave,
Crossings and Elevate costs separately; split every measured over-budget
phase. Use a provisional 4-ms CPU slice target, leaving room in 16.67 ms for
simulation/render; revise only from measured whole-frame budgets. Bound CPU
peak memory and pending job count. Source revision/cancellation may not leave
borrowed spans or mutate the published graph.

## Current evidence

The one-shot oracle measured 542.714 ms: Lay 2.316, Weave 430.871,
Crossings 25.604 and Elevate 83.923 ms. A second Weave probe measured sort
5.352, snap 111.007, edges 13.738, tie 374.715 and pack 2.477 ms.

A move-only `Path::NetworkWeaveJob` now owns the mutable graph and temporary
indices; `NetworkElevationJob` owns height sampling and profiles.
`world/navigation/TransportNetworkBuildJob` drives both as candidate stage
`NeedsNetwork`; only its completed snapshot reaches `Corridors`. One-shot
paths remain comparison oracles. Analytic directed routes, shared-node samples,
profiles and diagnostics match at work sizes 1, 2 and 8.

Wien keeps 47,101 ways, 155,084 nodes, 369,981 edges and digest `49440d93`.
Worst simulation frame fell from about 556 ms to 109–133 ms; p99 fell from
about 23 ms to 9.56–13.18 ms. The alternate `0257fdae` digest differs in
290/921600 pixels at rows 487–509 and is tracked as shot nondeterminism in
WI 2105.

At 1,024 graph items/frame and 4,096 profile points/frame, latest worst slices
are: snap 2.285, edge creation 0.613, edge index 9.714, adjacency 5.947, tie
2.896, weave publication 2.941, crossings 25.636, node sampling 19.937, point
writes 0.013, stations 0.379, slopes 0.288 and grade statistics 0.058 ms.
Profile work previously cost 14.078 ms. Destroying 72.376 ms of weave
temporaries caused the unexplained 48.958-ms transition; explicit staged
release now limits that work to 2.594 ms and the longest build slice is the
crossing pass. Its measured parts are point span 0.217, segment list 0.717,
grid 0.194, cell filing 4.784, pair tests 19.420 and cache 0.037 ms.
A native crossing job now owns the graph, preserves one-shot crossing identity
at pair budgets 1, 2 and 8, and records the full candidate count. Wien has
2,822,153 candidate pairs. A phase-specific 2,097,152-pair slice is the
smallest measured budget that keeps the current 6,144-frame shot horizon
green: digest `49440d93` at 6,071 frames, crossing worst 19.387 ms. Budgets
1,048,576 and below finish the network but miss the later Refined deadline;
the candidate scheduler amplifies one extra network tick and needs correction.

Global budgets 128, 256 and 512 failed to reach Refined within the 6,144-frame
shot horizon; 1,024 completes in about 5,860–6,072 frames. Counts therefore
remain phase-specific. Next remove the candidate-scheduler amplification,
reduce crossing setup/pair slices below 4 ms, and move DEM sampling off the
frame path or into an independently bounded worker.

The elevation job now owns the candidate's pinned `HeightSheets` sampler
instead of consulting mutable `GroundStream` and synchronously building tiles.
Wien sampled every node, reduced the worst elevation slice from 21.630 to
0.804 ms and changed 3,297/921,600 pixels in the road/shore band because roads
now use the rendered candidate terrain. Digest is `84df505c`; the build needed
6,206 frames, so the unchanged 6,144-frame shot horizon remains red.

## Acceptance

- Analytic line, closed loop, legal junction, grade-separated crossing and
  source-revision replacement produce identical graph IDs, directed edges,
  route legality, elevations and diagnostics in one-shot and sliced builds
  across work sizes and changed arrival order. Same way count with changed
  topology must fail the stale-cache negative control.
- Interrupted build, late DEM refusal and cancellation preserve the prior
  published graph. Render-LOD and mesh eviction leave routes unchanged.
- Wien and Malcesine: record per-phase total and worst slice, p50/p95/p99
  frames, peak memory and source counts. No network slice exceeds its declared
  budget; full 720p60 remains a separate acceptance. Inspect PNGs and compare
  native road products before/after; no test-specific route or Hockenheim path.
- Hockenheim WI 2260 uses this stable graph to identify its OSM raceway loop;
  graph readiness and route identity survive streaming revisions.
