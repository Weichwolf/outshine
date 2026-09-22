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
Source-revision replacement, missing recovery and bounded memory still require
proof before this WI closes. A temporary timed probe on Wien found the complete
49-tile publication rebuilding 108870 features and 2143040 point scalars in
34.271 ms on the Engine thread; the slowest tile took 1.487 ms. Koerbersee's
largest publication took 4.560 ms. The Wien step alone exceeds 16.67 ms, so
bounded resumable assembly is required, with unchanged atomic publication.
The provider-backed refusal/backoff/arrival case now proves that the old native
snapshot survives refusal and the recovered centre replaces it without mixing
tile indices or losing source identity. Missing-tile recovery and changed source
revisions remain unproven.

2026-09-22: `GroundCandidatePacingReachesReadiness` was red on the committed
baseline and with bounded assembly because a zero-sheet terrain candidate reached
corridor draping. WI 2243's contact-mesh gate restored both pacing variants without
changing the oracle.

2026-09-22 Wien probe after staged assembly: the 49-tile snapshot completed in
13 four-tile slices; the longest observed slice was 2.513 ms. Removing the
probe restored the prior shot digest `49440d93`. This bounds the measured case,
not every tile or total frame. WI 2256 owns the now-measured 637.622-ms road
corridor call; WI 2234 still owns whole-frame stalls and budgets.

The current producer retains parsed tiles and settled keys from every visited
window, then merges all of them into later snapshots. Movement therefore grows
memory and publishes out-of-window geometry. Retain only the active requested
window's decoded products; keep the previous native publication intact until the
new contact/full snapshot commits. Revisit must decode or reuse cache and produce
the same canonical product. Test disjoint windows and return travel.

`GroundRevision` currently compares ingested street/water tile counts but omits
the vector publication generation. Replacing one native OSM snapshot with another
at equal counts can therefore leave a candidate or published world marked current.
Include the vector generation in revision matching and rebuild eligibility; a
candidate must be discarded before it reads changed vectors. `SourceSet` is sealed
for the declared scenario, so in-session provider registration/revision mutation
is outside this contract; redeclaration resets the source set and publication.

The paced product oracle read `the geometry the world built` from `World.Pieces`
inside `Focuses`, before the next candidate was built. Once source generation
restarts candidates at the correct revision, that value can describe different
previous publications under different pacing. Move the diagnostic to successful
publication; retain the same digest equality check against the final native world.

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
