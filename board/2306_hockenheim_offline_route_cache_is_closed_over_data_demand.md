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

On 2026-09-28, `outshine-client run --view lap --at-seconds 91.7` captured
Hockenheim after 167 remote starts. The immediate identical `--offline` run
failed in `HeightSheets` during `route advance`: 170 cache hits, 46 misses,
zero remote starts. A second online success needed 4 remote starts; another
offline run still failed with 49 misses. The missing addresses included
`elevation/15/17165/11203` and coarser tiles around the lap. Thus a successful
online image does not prove a closed cache for the same timed camera path.
`ContentStore` stores bytes only; `Meaning::Absent` is not persisted. The
observations do not yet distinguish newly demanded tiles from explicit 404s.
An isolated camera probe centred on missing `15/17165/11203` fetched and
cached a 54,579-byte Terrarium PNG; that address was a real, previously
unrequested byte tile. Offline replay then still failed on other addresses.
At least part of the gap is demand closure, not missing-value policy.
The client preloads before selecting `lap`, then fast-forwards 5,502 ticks
without intermediate preload and waits only at the final frame. A paced
online `--motion` run to 91.7 s fetched the route demand (244 remote starts,
0 missing-contact frames, p99 11.91 ms). The next static offline capture
had 538 hits, 0 misses, 0 remote starts and a PNG identical to the successful
online static image (0/921600 pixels changed). The motion frame itself was
unrefined and is not the static image oracle.
Fast-forward with playable preload every tick still left 42 offline misses;
playable readiness alone does not close route demand.
The controlled paced 60-Hz motion without intermediate renders requested 538
byte tiles into a fresh cache in 97.7 s. Its final screenshot failed because
no frame was drawn, as expected for this temporary diagnostic. An immediate
offline static capture from that cache succeeded with 538 hits, zero misses
and zero remote starts. Its PNG matched the earlier online static image at
all 921,600 pixels. The diagnostic code was removed. Thus wall-time pacing,
not GPU rendering, closes this observed demand gap; fast-forward outruns
asynchronous tile and candidate progress. It remains to identify which
candidate phase creates the extra addresses and make its route demand explicit.
Content-store key reconstruction mapped every file in the two isolated caches
to a tile address. Both contain the same 65 vector tiles. The paced cache has
473 elevation tiles versus 129 after fast-forward with playable preload every
tick: 344 elevation addresses occur only in the paced run. The gap includes
source zooms 11, 14 and 15; it is not an OSM vector-download difference.
`HeightSheets::SourceTilesOf` currently derives elevation requests from
resident `Patchwork::Sheets`, so a candidate that has not reached its sheet
phase cannot submit the full set. `GroundPatchwork` selects tiles from `Around`
but changes coverage with tile readiness. Demand planning must enumerate the
conservative geometric coverage independently of that readiness.

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
   `PlanPatchworkTiles(Around)` now provides canonical potential mesh tiles,
   independent of replies, without IO or waiting. Its independent-cell test
   covers ready/mixed/pending cascades, dateline and poles. Source-neighbour
   expansion, path union and bounded preparation are still to be integrated.
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
