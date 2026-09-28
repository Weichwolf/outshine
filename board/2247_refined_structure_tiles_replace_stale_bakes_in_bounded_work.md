Type: defect
State: active
Architecture: ready
Parent: 2230
Depends:
Priority: P0
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

Reservation release belongs to the BuildingField domain, even after producer revisions
change (8d094066c). A foreign domain never releases or lands the current owner’s slot.

Current capability: accepted CPU tile products and immutable piece payloads survive
candidate copying; restoration is paced. Queue/BuildingField/HeightField and both
publication pacing variants pass at 1b1086740. At 497d94109, Refined captures succeed
for 7/10 Places; Wien remains in earthworks, Graz/Olympiaturm time out. True unchanged
GPU-handle reuse and complete cancellation/eviction acceptance remain open.
Olympiaturm sample: repeated Retable/UploadTables and Metal waits, 9.4 GB peak footprint.
Measure retained/cycled capacities before calling this a leak.

## Next executable repair: share each piece's cluster spheres

AppendPieceBatches currently duplicates the same 12 sphere/error floats per cluster for
every instance row. SubjectDraw.cpp owns the table; transforms and jobs remain per instance.
Append immutable spheres once per piece, then reference their base plus cluster index from
each instance job. No deduplication across pieces; preserve batch/model/material/index rows,
source errors, instance transforms and draw coverage. Empty piece rows append no spheres.
Owner/test: render/stages/SubjectDraw.cpp and its suite. Use real GPU table readback with
multiple differently transformed instances and two distinct pieces; check exact sphere
bytes, job references and retained transforms. Restored duplication must actually FAIL.
Gate: make format; SubjectDraw/SceneResources suites; complete Places and opened PNGs;
full lint/tidy/API. Measure actual byte savings and all Place frame costs; smaller tables
alone neither prove an Engine residency ceiling nor close unchanged GPU-handle reuse.

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
