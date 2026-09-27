Type: defect
State: active
Architecture: ready
Parent: 2260
Depends:
Priority: P0
Area: world, data, client, streaming
Tags: hockenheim, offline, cache, route, reproducibility

# Hockenheim offline route cache is closed over its data demand

## Reproduced defect

On 2026-09-28, an online Hockenheim capture at 91.7 s (5,502 ticks)
succeeded, but an immediate identical offline capture failed during route
advance with unknown height tiles and zero remote starts. A missing tile
`elevation/15/17165/11203` was fetched separately as a 54,579-byte Terrarium
PNG: at least part of the gap is unsubmitted demand, not provider absence.
Playable preload at every fast-forward tick still left 42 offline misses.

A controlled fresh-cache run paced the same ticks at 60 Hz without intermediate
rendering. It fetched 538 tiles. The immediate offline static capture had 538
hits, zero misses and zero remote starts; all 921,600 pixels matched the online
static image. Thus rendering is not required for this observed demand closure;
fast-forward outruns asynchronous terrain candidate progress. Diagnostic code
was removed. The paced motion image itself is not the static image oracle.

Content-key reconstruction identified every entry in the isolated caches.
Both runs contain the same 65 vector tiles. Paced execution fetched 473
elevation tiles versus 129 with fast-forward/playable preload: 344 additional
elevation addresses at source zooms 11, 14 and 15. Required demand is submitted
by candidate sheet preparation, after residency-dependent patchwork selection.
`PlanPatchworkTiles` and `PlanTerrainSourceTiles` now permit independent planning,
but no route-path preparation consumes them yet.

`GroundStream::TileAt` also calls `KeepCoarse` after its normal grid resolves;
that sampler requests a field three zoom levels below its normal grid. The
route plan must include those sampling requests as well as render-sheet demand,
using the sampler's own coverage contract rather than a copied zoom constant.

## Contract and ownership

- Client route preparation owns the declared station/time path; `HeightSheets`
  and ground classification own their data requests. A capture-ready result
  means all data requests that the same deterministic path can issue through
  final publication are settled or explicitly absent. Worker scheduling and
  cache speed must not change the *required address set*.
- SourceSet/ContentStore distinguish bytes, confirmed absence and unknown
  cache miss. Persist an absence only for an authoritative provider response
  such as 404, under the same source key/revision; never turn 403, timeout,
  cancellation or corrupt bytes into absence. Unpinned absence needs bounded
  freshness. Offline must issue zero network starts and fail with source key,
  tile address and cause when required data is unknown.
- Keep the cache bounded, including metadata/negative entries. Do not fake
  terrain heights or broaden `settled()` to hide refused tiles. Existing raw
  cached bytes remain readable; any new record format is versioned.

## First falsification and implementation sequence

1. In an isolated cache, trace required source key/address/outcome for online
   success and immediate offline replay. Compare the sets, marking each miss
   as never requested, confirmed absent, or evicted. Put traces under system
   tmp and keep only aggregate counts in normal client output.
2. Trace the candidate phase that submits each extra address when asynchronous
   tile progress keeps up with route ticks. `lap` can be selected only after
   the route is published by initial preload, so retain that bootstrap, then
   prepare the selected route view explicitly. The initial preload alone
   targets the previous view.
3. Move required data-demand submission into world/view preparation, independent
   of GPU drawing and worker timing. A preparation API awaits the complete
   request closure under one bounded global deadline; playable `preload`
   alone returns too early. Factor a pure coverage-to-source-tile planner from
   `GroundPatchwork` and `HeightSheets`: enumerate route camera positions from
   the published alignment and speed profile, cover their possible 4x4 tile
   blocks at each level, then expand source neighbours and ground-zoom parents.
   Use bounded batches and an explicit tile-count refusal for oversized paths.
   The planned set must be independent of resident mesh/field state; replay
   must require that same set. Keep the planning contract useful for any
   georeferenced path, not tied to Hockenheim or the client.
   `src/engine/streaming/TerrainSourceCoverage` owns the shared source planner:
   valid native tile IDs, canonical unique output, 8192-field maximum (initial
   budget; observed path needs 473), explicit refusal before oversized work.
   Settle vectors/classification first, then include vector neighbours and
   building footprint heights through that planner, not a guessed radial pad.
   `PlanPatchworkTiles` plans potential mesh tiles without IO. The shared
   `PlanTerrainSourceTiles` now supplies halos, parents, vector/building and
   route tiles to `HeightSheets`, with independent grid/seam/budget controls.
   Path union and bounded path preparation are still to be integrated.
   `world/ground/TerrainSamplingCoverage` owns normal/coarse field mapping.
   `GroundStream::SamplingCoverage(TileId)` maps a source tile to the sampler's
   configured grid and its optional three-level fallback without IO. Resident
   queries and `KeepCoarse` consume this same mapping. Reject invalid grids or
   a source tile coarser than the configured sampling grid; never invent finer
   coordinates from an underspecified coarse tile.
   `Data::TileId::MaximumZoom` owns the existing native grid limit of 30.
   `HeightField` aliases it; sampling coverage must not include the field loader.
4. If confirmed 404s occur, add a typed bounded absence record with source
   revision/freshness rules; prove it differs from a cache miss. Do not cache
   403 as absent (Terrarium currently does), and retain retry/refusal policy.

## Acceptance

- Online prepare/capture followed by offline capture in an isolated cache at
  the same route/time succeeds with `remote_starts=0`, `store_misses=0` for
  required addresses and pixel-identical PNG. Repeat with different worker
  pacing; the demand set is equal. Hockenheim full-lap motion uses the same
  closed data plan and keeps contact/route invariants from WI 2260.
- Delete one required cached tile: offline fails with exact source/address and
  no PNG or network. Inject 404, 403, timeout and corrupt payload independently:
  only 404 may become an explicit absent record; no case invents elevation.
- Focused SourceSet/ContentStore/HeightSheets/client cases, `make format` and
  `LINT_JOBS=2 make lint` pass. Compare PNGs with `test/scripts/pixels.py`.
