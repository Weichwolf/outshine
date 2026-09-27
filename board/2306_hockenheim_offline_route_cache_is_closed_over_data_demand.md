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
Two discarded client experiments advanced without intermediate renders and
called playable `Engine::preload`: every simulated second caused 181 remote
starts and 43 offline misses; every tick caused 197 starts and 42 misses.
Both fast-forwarded in a few wall seconds. Their failure does not yet
separate real-time pacing from `renderer().render` as the missing trigger.

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
2. Isolate pacing from rendering: run identical route ticks at real-time pace
   without frame rendering and compare source address sets to `--motion`.
   Locate the phase that submits each additional request. Select `lap` after
   its route is published and before its own data preparation; initial preload
   currently targets the previous view.
3. Move required data-demand submission into world/view preparation, independent
   of GPU drawing and worker timing. A preparation API awaits the complete
   request closure under one bounded global deadline; playable `preload`
   alone returns too early. Replay must require the same address set.
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
