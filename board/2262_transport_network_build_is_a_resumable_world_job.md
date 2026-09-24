Type: defect
State: active
Architecture: ready
Parent: 2234
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
The original `Corridors::MapOf` also built a snapped network from vector-tile
streets inside the road mesher. That network now has a paced candidate builder.
It is a **render-corridor input**, not the OSM-ID logical map in 2133: the
VersaTiles feature IDs have no proven mapping to source Way IDs and a 2 m XY
snap can connect unrelated levels.

## Decision

Keep the vector-street corridor graph under `world/ground`; the road generator
consumes a read-only graph snapshot. Rename its builder and product so neither
claims to be the authoritative transport topology. A candidate-owned, move-only
builder pins one `OsmField` generation, `StreetField`, DEM source and region.
Build in deterministic source order: ingest ways/points, create topology,
classify crossings, then attach elevation and navigation metadata. Expose a
bounded `Advance(work budget)` and cancellation, not a synchronous full build
in `BeginsGroundModels`. No intermediate graph can replace the published one.
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
The paced builder owns move-only weave, crossing and elevation jobs, pins
candidate DEM sheets and publishes only complete graphs. Wien has 47,101 ways,
155,084 nodes and 369,981 edges. Two recent Wien runs retained the same
native product digest; worst network slices were 3.35–4.03 ms, p99 frames
9.79–9.92 ms, with seven frames over 16.67 ms. This does not prove the
4-ms bound or whole-frame 60 fps. Visual differences are handled by 2256.

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
