Type: defect
State: proposed
Architecture: ready
Parent: 2230
Depends: 2248
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
WI 2248.

## Decision

Track accepted structure products by vector tile and exact height input
revision, including empty geometry. Playable fallback is publishable; Refined
requires a qualified pinned fine-height snapshot for each relevant tile.
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
Revision comparisons cover vector source, DEM source, quality, camera-dependent
detail inputs and candidate generation. Do not use a global reset or special
case for Malcesine/Graz.

2026-09-23 Rosenheim exposed an old-candidate worker releasing its tile in the
new candidate's `BuildingField`: equal vector generation was insufficient and
tripped `TileWatermark::Release`. Reservation release now also requires the
same candidate height revision, footprint parameters and eye. This prevents
cross-candidate mutation; it does not replace the global refined rebake.

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
4. Render Malcesine twice from cache-warm starts; open both PNGs and compare
   exact pixels on the same backend. Refined reports zero fallback structure
   tiles in its workset. Graz reaches Refined without a global structure
   rebuild. Report posted/replaced tile counts, p50/p95/p99, frames above
   16.67 ms and memory peak against the current baseline. `make format`,
   relevant suites and `make lint` pass.
