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
`Corridors` also owns construction of the logical map, although road geometry
should consume a published graph rather than build navigation inside a mesher.

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

First implementation slice: `NetworkWeaveJob` now resumes edge indexing,
physical adjacency and loose-end splicing, preserving one-shot route behavior
in the analytic directed-splice case at work sizes 1, 2 and 8. It is not yet
used by the Engine. `Begin` still performs sorting, point snapping and edge
creation synchronously; publication also remains one-shot. Split those phases,
then integrate a candidate-owned world/navigation builder before claiming a
frame-budget improvement.

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
