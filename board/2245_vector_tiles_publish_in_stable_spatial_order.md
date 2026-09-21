Type: defect
State: active
Architecture: ready
Parent: 2154
Depends:
Priority: P0
Area: world, ground, streaming
Tags: osm, determinism, identity

# Vector tiles retain spatial identity across download schedules

## Defect

`OsmField::Build` scans a fixed 7x7 request window but `Accept` appends each ready
tile, its features, rings and points immediately. Different download schedules give
the same tile set different native indices. The 2026-09-21 preload/advance oracle
proves different `OsmField::Tiles()` order while terrain inputs and counts match.
Footprint order, seating, pressed terrain, building pieces and renderer digest then
diverge. Sorting footprints by transient tile index did not fix this.

Current focused repair stages decoded tiles independently, publishes a spatially
ordered contact snapshot, then a complete requested-ring snapshot. Preload and
advance now request the same first tier. `GroundCandidatePacingReachesReadiness`
passes normal and NDEBUG variants; Floor/Lattice and six OsmField tests pass.
Source-revision replacement, refused/missing recovery, assembly cost and bounded
memory still require proof before this WI closes.

## Decision

Fetch all requested tiles concurrently; never serialize IO behind the first pending
tile. Decode each tile into an owned, independently indexed product keyed by stable
`(zoom, x, y)` and source revision. Publish a canonical vector snapshot by spatial
key and deterministic layer/feature order. Derive contiguous runtime indices for
features, rings, points, tags and tile ranges in one bounded publication step; no
consumer may treat arrival index as identity. Retain the previous snapshot until the
new one is complete. Stable source IDs and tile keys carry cross-snapshot references.

Keep near-contact readiness independent of distant downloads: publish the complete
required near subset first, then a refined snapshot with all requested tiles. Missing,
refused and later-retried tiles have explicit state, not silent gaps or permanent
head-of-line blocking. Generator input and candidate revision pin one snapshot;
outdated worker results cannot commit into it. Measure decode, merge, memory peak and
frame slices; if canonical assembly exceeds budget, resume by tile without exposing
partial arrays. Reuse existing native `OsmField` contracts during migration; do not
add an engine-facing glTF/OSM data model.

## Implementation

1. Add an isolated `OsmField` test with two tile payloads arriving A-B and B-A.
   Assert identical tile keys, feature/ring/point contents and all index relations;
   use a missing/retried tile and a contact tile as negative/control cases.
2. Split `Accept` into per-tile validated decode and canonical snapshot assembly.
   Remap local offsets and interned keys/values explicitly. Reject malformed input
   atomically. Make `Tiles()`, `Features()`, `OfTile()`, `Points()` and generation refer
   to one immutable publication; preserve API ownership and span lifetime.
3. Move `TileWatermark`, BuildingField, StreetField, WaterField, classing and piece
   identity to stable spatial keys or verified snapshot-local indices. Cancel/rebase
   work on snapshot revision change. Remove transient-index digest dependencies.
4. Restore WI 2234's paced/preload oracle. Compare source manifest, native products,
   contact and image under reversed IO order; retain a deliberate early-publication
   negative control. Record p50/p95/p99 and peak bytes in WI 2233/2244 budgets.

## Acceptance

- Same provider bytes and request produce identical native snapshot and products
  under reversed completion order, preload and frame pacing.
- Contact publishes within its existing 15 s limit while distant tiles are delayed;
  source requests remain concurrent and queue/memory limits hold.
- Invalid, missing, refused and retried tiles preserve prior publication and recover
  without mixed offsets, stale commits or permanent blocking.
- Focused OsmField/TileWatermark/generator tests, GroundCandidatePacingReachesReadiness,
  Floor/Lattice integrations, `make format` and `make lint` pass.
