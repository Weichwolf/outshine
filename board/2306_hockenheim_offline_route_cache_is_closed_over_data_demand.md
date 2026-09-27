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
2. Make route-data demand a deterministic closure of the declared path and
   capture quality. Preparation runs the same planner as capture. A cache
   complete for that closure must replay offline regardless of IO timing.
3. If confirmed 404s occur, add a typed bounded absence record with source
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
