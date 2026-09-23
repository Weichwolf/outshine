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

The one-shot oracle measured 543 ms (Weave 431, Crossings 26, Elevate 84).
`TransportNetworkBuildJob` now owns move-only weave, crossing and elevation
jobs. It publishes only a complete graph; `Corridors` reads that snapshot.
One-shot paths remain independent comparison oracles. Small directed routes,
crossings, elevation profiles and diagnostics agree at work sizes 1, 2 and 8.
Equal-geometry ways now sort by all navigation properties and source tag;
reversing their arrival order preserves the native way order in the focused test.
The elevation job pins candidate `HeightSheets`, avoiding synchronous DEM
builds and aligning roads with the rendered terrain.

Wien holds 47,101 ways, 155,084 nodes, 369,981 edges and 2,822,153 candidate
crossing pairs. Its current shot digest is `2fc0aec4`. The earlier 8,192-frame
diagnostic needed 7,433–7,456 advances; a bounded four-advance, 8-ms
same-phase candidate scheduler now reaches Refined in 3,141 advances, with
the same digest. At 262,144 crossing pairs per slice, Wien needs 3,118
advances, p99 10.78 ms, and still has 10 frames over 16.67 ms. Longest
crossing pair slice is 2.852 ms. A long edge previously indexed thousands of
cells in one advance; a cell cursor now bounds that work. Measured single-cell
hashmap rehash cost 14.70 ms and full edge-set clearing 15.01 ms. The cell
index now has 256 fixed shards. Physical edges transfer to adjacency without
destruction or reinsertion; degree accumulation is sliced. Wien remains
`2fc0aec4`; its measured edge-index slice is 3.5–5.0 ms across runs.
Crossing setup now counts, prefixes, allocates and fills cells in separate
bounded phases, including a cursor within a long segment. At 131,072 pairs
per pair slice, Wien reaches Refined in 4,196 advances, p99 10.42 ms, peak
heap 1,102 MB; setup stays below 3 ms, pair worst 3.633 ms, adjacency worst
0.114 ms. The longest network step is 5.273 ms in weave startup; 10 whole
frames exceed 16.67 ms. Malcesine remains `07ca3a25`, p99 10.59 ms and zero
late frames. The PNGs
were opened: graph work does not change roads, river or building masses;
their low-detail look is tracked by visual WIs.

Next bound weave startup/index rehash and identify the remaining simulation spikes
before claiming 720p60; draw spikes are separate. Graph source-revision and
cancellation tests remain part of this WI's acceptance.

`NetworkWeaveJob::Begin` currently calls the one-shot
`Network::SortWaysIntoDeclaredOrder`, including full sort and point/way copies,
before any `Advance` budget applies. Move ordering into bounded job phases:
sort fixed-size runs with the same total way comparator as the one-shot oracle,
merge them incrementally, then copy point/way arrays with a point cursor.
Prepare and publish each array only at complete phase boundaries. Check exact
node/edge/route equality for duplicate ways and reversed input order; measure
each phase on Wien separately before claiming the startup spike is gone.

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
