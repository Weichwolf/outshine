Type: defect
State: active
Architecture: ready
Parent: 2234
Depends:
Priority: P0
Area: world, ground, engine, streaming
Tags: vector-tiles, roads, realtime, determinism

# Vector street corridor graph construction must not stall a moving frame

## Proven defect

After WI 2133 keyed network reuse by source revision, Wien's normal Refined
candidate runs `Corridors::MapOf` for 529.673/591.290 ms in one Engine frame.
Those runs' worst simulation frames were 530.97/592.67 ms. The graph has
47,101 ways, 155,084 nodes and 369,981 edges. This stage prevents continuous
camera movement even though road corridor slices are now about 10–12 ms.
The original `Corridors::MapOf` also built a snapped network from vector-tile
streets inside the road mesher. That network now has a paced candidate builder.
It is a **render-corridor input**, not the OSM-ID logical map in 2133: the
VersaTiles feature IDs have no proven mapping to source Way IDs and a 2 m XY
snap can connect unrelated levels.

## Decision

Keep the vector-street corridor graph under `world/ground`; the road generator
consumes a read-only graph snapshot. `VectorStreetGraph` and its build job
name this derived product, distinct from `TransportTopology`. A candidate-owned, move-only
builder pins one `OsmField` generation, `StreetField`, DEM source and region.
Build in deterministic source order: ingest ways/points, create topology,
classify crossings, then attach elevation and corridor metadata. Expose a
bounded `Advance(work budget)` and cancellation, not a synchronous full build
in `AdvanceGroundStreetGraph`. No intermediate graph can replace the published one.
Vector-tile revisions govern this derived product, never OSM logical identity.
A same-count changed vector revision rebuilds it. Failures retain the old
published corridor product; do not publish null or partial state. The source-ID
graph in `TransportTopology` stays independent of this job and of render LOD.

Keep `BuildOneShot` as an independent comparison oracle while pacing the same
vector-street product. Use a provisional 4-ms CPU slice target inside a
16.67-ms frame; measure whole frames before claiming the target. Bound peak
memory and pending jobs. Cancellation must preserve the published product.

## Current evidence

The one-shot oracle measured 543 ms (Weave 431, Crossings 26, Elevate 84).
The paced `VectorStreetGraphBuildJob` owns move-only weave, crossing and elevation jobs, pins
candidate DEM sheets and publishes only complete graphs. Wien has 47,101 ways,
155,084 nodes and 369,981 edges. Two recent Wien runs retained the same
native product digest; worst network slices were 3.35–4.03 ms, p99 frames
9.79–9.92 ms, with seven frames over 16.67 ms. This does not prove the
4-ms bound or whole-frame 60 fps. Visual differences are handled by 2256.

Hockenheim's warm offline 20 s lap starts only Playable and Refined candidates,
yet all 1200 motion frames are unrefined. The Refined MVT graph has 12,462
ways, 43,698 nodes and 102,370 edges. Its work takes about 625 ms CPU but
1716 paced advances before corridor construction. The candidate then reaches
the corridor phase while the camera is already 408 m along the route. The
semantic OSM alignment does not use this MVT graph, but corridor rendering does.
No candidate-restart hypothesis explains the measured delay.

Next move the existing deterministic graph job to one candidate-owned worker.
`Begin` snapshots way/point data into its own graph; pin an immutable copy of
candidate DEM fields for height queries, with no mutable GroundStack borrow.
The worker runs bounded job slices back-to-back and publishes only an owned
complete result; the engine thread polls without waiting. A canceled candidate
requests stop between slices and joins before releasing its height snapshot.
Keep the source revision and current published graph until atomic candidate
publication. Reject stale completion. Compare topology, heights, digest, frame
p99 and refined-frame fraction against the paced baseline; do not spend more
main-thread frame budget to reduce latency.

## Acceptance

- Analytic line, closed loop, crossings and changed vector revision produce
  identical corridor graphs and heights in one-shot and sliced builds across
  work sizes and changed arrival order. Same way count with changed geometry
  must fail the stale-cache negative control.
- Interrupted build, late DEM refusal and cancellation preserve the prior
  published corridor graph. Render-LOD and mesh eviction do not alter its ID.
- Wien and Malcesine: record per-phase total and worst slice, p50/p95/p99
  frames, peak memory and source counts. No network slice exceeds its declared
  budget; full 720p60 remains a separate acceptance. Inspect PNGs and compare
  native road products before/after; no test-specific route or Hockenheim path.
- The product is explicitly named as a vector-street corridor input; no caller
  treats its snapped nodes or provider IDs as source-ID navigation authority.
  Hockenheim WI 2260 must use the semantic graph in 2133, not this product.
