Type: defect
State: active
Architecture: ready
Parent: 2230
Depends:
Priority: P1
Area: engine, ground, generators
Tags: structures, determinism, realtime

# Refined structure tiles replace stale bakes in bounded work

## Defect and counterexample

Playable may bake a structure tile from fallback heights. Refined's FineOnly
queue gate rejects new/waiting fallback tasks, but `GroundWorldCandidate`
copies accepted `BuildingField` and its advanced `TileWatermark`; those tiles
are never offered again. Malcesine Refined therefore still showed fallback
tile 24 and arrival-dependent pixels. A full candidate reset/rebake made two
Malcesine captures pixel-identical with zero fallback tiles, but Graz failed
to reach Refined before capture after over 65,000 buildings had been baked.
That candidate-wide path was rejected. Source and product revisions come from
WI 2248. Its accepted-input identity contract is implemented; remaining
source-lifecycle tests there do not block the per-tile product store.

Basel Badischer at 47.568 N, 7.607 E reproduced the same scale failure on
2026-09-23: after the native structure material fix, vegetation disabled, and
cached input, the client processed more than 98,000 building candidates, then
started another candidate and still missed Refined after 6,144 measured frames.
Changing `sightM` from 4,000 to 500 did not change that outcome. Diagnose the
candidate/revision trace before attributing every rebake to height quality.

## Decision

Track accepted structure products by vector tile and exact height input
revision, including empty geometry. Playable fallback is publishable; Refined
requires a qualified pinned fine-height snapshot for each relevant tile.
Require both complete source provenance and sufficient effective DEM sample
spacing for the tile's declared contact/detail tolerance; an identified
ancestor DEM is not automatically fine. Derive the threshold from the actor
contact and visible terrain error budget, not a hardcoded provider zoom.
When its source changes, enqueue only the affected tile for replacement.
Separate replacement admission from first-time `TileWatermark` ingestion so
already accepted tiles are not silently skipped or counted twice. Bound
in-flight tasks, posts, merges and GPU uploads per frame. Cancel stale work
without joining workers on the frame thread; reject late older generations.

`TilePieces::Hands` can swap a tile's render handles after successful upload.
`BuildingField` must replace that tile's footprint range, seat/across samples,
counts and range offsets atomically after reserving capacity. Keep tile IDs
unique and the published world intact until the candidate commits. Any failed
upload or bake discards the private candidate product; no half-updated world.
The current Refined constructor chooses `SceneResources::PieceSources::Omit`,
and `GroundWorldCandidate::Prepare` clears the copied `TilePieces`. That policy
forces a full GPU structure upload even if CPU bakes are selective. Refined
must instead copy stable piece sources and tile ownership from Playable,
replace only stale tiles after successful uploads, and retain unchanged
handles through publication. Prove that copied GPU resources remain valid
until candidate commit or abandonment; do not merely remove `ResetDerived`.
First replace `BuildingField`'s sparse footprint range plus append-only
measurements with one sorted accepted-tile product record. That record owns
ranges for all three arrays and the tile's counters. Replacement prepares its
owned input and reserves growth before the non-throwing commit; it never
advances first-ingestion watermark or accepted-tile count. Verify replacement
independently before connecting worker admission.
Revision comparisons cover vector source, DEM source, quality, camera-dependent
detail inputs and candidate generation. Do not use a global reset or special
case for Malcesine/Graz.

2026-09-23 Rosenheim exposed an old-candidate worker releasing its tile in the
new candidate's `BuildingField`: equal vector generation was insufficient and
tripped `TileWatermark::Release`. Reservation release now also requires the
same candidate height revision, footprint parameters and eye. This prevents
cross-candidate mutation; it does not replace the global refined rebake.

The `GroundCandidatePacingReachesReadiness` integration suite is red on both
`13e3b6cfd` and the later sliced-network branch: all OSM/DEM tiles arrive,
outstanding IO is zero, but three consecutive builds each remain at candidate
progress 10 (`NeedsBakes`) after the 30-second Refined deadline. Normal and
validated variants agree. Network weaving finishes before this phase; do not
attribute the timeout to network slicing or weaken the deadline. Diagnose
which structure bake/replacement job or publication gate holds progress 10.
Wien independently misses its 6,144-frame Refined shot after more than
103,000 structure candidates are baked and another candidate starts; the
client cannot produce a fresh PNG or publish network diagnostics meanwhile.
The exact bypass is `BeginsGroundBuild` calling `ResetDerived()` for every
Refined candidate. `BuildingField::ReplaceAcceptance` already replaces one
tile atomically, but `StructureBuildQueue::Posts` admits only
`BuildingField::Next` first-ingestion work. `Complete` checks only the queue
and watermark, so it cannot certify qualified replacement. Connect admission,
commit and readiness to accepted source revisions before removing the reset.

## Implementation and acceptance

1. Add a per-tile acceptance record with source/product revisions and ranges
   for footprints and both measurement arrays. First acceptance and replacement
   preserve sorted tile order, counters and `Ingested` semantics. Empty output
   still records its accepted revision.
2. Schedule only stale tiles from the copied candidate. A fallback tile must
   be replaced when fine input arrives; a fine tile with identical revision
   must not rebake. `Complete` and Refined-readiness wait for all required
   replacements, not merely an empty worker queue.
3. Test fallback → fine, two reversed worker completions, same-revision no-op,
   empty tile, smaller/larger replacement, cancellation, failed upload and
   source eviction/reload. Compare full footprints, samples, counts, handles
   and publication revision; published Playable remains usable on failure.
   Assert unchanged tile handles and upload count/bytes are retained across
   Refined publication, not rebuilt under `PieceSources::Omit`.
4. Render Malcesine twice from cache-warm starts; open both PNGs and compare
   exact pixels on the same backend. Refined reports zero fallback structure
   tiles in its workset. Graz reaches Refined without a global structure
   rebuild. Basel Badischer also reaches Refined from the same cached input
   without a candidate-wide rebake. Report posted/replaced tile counts, p50/p95/p99, frames above
   16.67 ms and memory peak against the current baseline. `make format`,
   relevant suites and `make lint` pass.
