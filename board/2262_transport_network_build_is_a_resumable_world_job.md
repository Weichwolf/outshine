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

First Wien phase probe: 542.714 ms total, comprising Lay 2.316 ms, Weave
430.871 ms, Crossings 25.604 ms and Elevate 83.923 ms. The largest work is
inside `Network::Weave`: sort ways, snap points, create edges, tie loose ends,
then flatten adjacency. Measure those subphases before changing their order;
stage `Weave` and `Elevate` first. Shot digest stayed `49440d93`.

Second Wien probe: `Weave` 510.662 ms: sort 5.352, snap 111.007, edges
13.738, tie 374.715, pack 2.477 ms. Prioritize a resumable `TieLooseEnds`
with owned edge-cell index, adjacency and ascending loose-node cursor;
`SpliceInto` must update both structures atomically before yielding. Stage
point snapping next. Keep a single frozen candidate graph until all phases
complete. The probe shot stayed `49440d93`.

Implementation boundary: a move-only `Path::NetworkWeaveJob` owns one private
mutable graph and all temporary point/node, outgoing-edge, edge-cell and
adjacency products. Its `Advance` resumes by source-order cursors and returns
Pending/Done or a typed failure; only `Take` on Done exposes the graph. The
existing `Network::Weave` remains the one-shot oracle. A later
`world/navigation` transport builder owns this graph job and handles OSM/DEM
source revision, crossings and elevation; `Corridors` only consumes its frozen
result. First prove analytic graph/route equality across slice sizes before
replacing the Engine's synchronous `MapOf` call.

Implementation so far: `NetworkWeaveJob` resumes point snapping, edge creation,
edge indexing, physical adjacency and loose-end splicing. The analytic directed-splice case
matches the one-shot route at work sizes 1, 2 and 8; all 14 Wayfinding cases
and lint pass. It is not yet used by the Engine. `Begin` still sorts ways
synchronously; publication is one-shot. Split or bound those operations and
measure worst individual edge-index and splice cost, then integrate a candidate-owned
world/navigation builder before claiming a frame-budget improvement.

`NetworkElevationJob` now resumes DEM sampling by distinct graph node and
height assignment by source point. The shared-node/profile oracle matches
one-shot `Elevate` at work sizes 1, 2 and 8, including refused samples and
grade diagnostics; 15 Wayfinding cases and lint pass. Station/grade profile
construction remains one-shot. The job is not yet wired into the Engine.

After moving one-shot construction to `world/navigation`, Wien repeated the
old `49440d93` still digest on the same source tree; another run gave
`0257fdae` with 290/921600 pixels differing only at rows 487–509. This is
existing shot nondeterminism tracked by WI 2105, not evidence of network
equivalence by itself. Wien still has a 556-ms worst simulation frame.

The candidate now owns `TransportNetworkBuildJob` as a separate `NeedsNetwork`
stage and publishes only after weave, crossings and elevation complete. Wien
keeps 47,101 ways, 155,084 nodes, 369,981 edges and the prior `49440d93`
digest. Worst simulation frame fell from about 556 ms to 109–133 ms; p99 fell
from about 23 ms to 9.56–13.18 ms. Remaining worst work at 1,024 items/frame:
snap 2.285, edge creation 0.613, edge index 9.714, adjacency 5.947, tie 2.896,
weave publication 2.941, crossings 24.613, node sampling 22.096, point writes
0.030 and profile construction 14.078 ms. A separate unclassified builder
slice reached 50.370 ms. Reduce the item budget from measurements, classify
that transfer/start slice, then split crossings and profile construction.

Global budgets of 128, 256 and 512 items were rejected: Wien did not reach
Refined within the 6,144-frame shot horizon; 1,024 completed in roughly
5,860–6,009 frames. A single count cannot represent cheap adjacency work and
expensive DEM samples. Keep 1,024 until phase-specific budgets or compute
execution preserve both completion latency and the per-frame bound.

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
