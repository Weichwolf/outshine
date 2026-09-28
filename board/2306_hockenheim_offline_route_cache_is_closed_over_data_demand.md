Type: defect
State: active
Architecture: ready
Parent: 2260
Depends:
Priority: P0
Area: world, data, client, streaming
Tags: hockenheim, offline, cache, route, reproducibility

# Hockenheim offline route cache is closed over its data demand

## Evidence and implemented capability

Fast-forward/playable preload fetched 65 vector and 129 elevation tiles; a
paced run fetched the same vectors and 473 elevation tiles. Immediate offline
replay of the first cache missed required data. Independent patchwork, source
and sampler coverage now feed bounded path preparation before client capture.

Starting with that incomplete 194-entry cache, online preparation/capture at
91.7 s fetched exactly the missing 344 elevation tiles. Immediate offline replay
had 538 cache hits, zero misses/network starts, and pixel-identical output to
both the online frame and the earlier paced-cache reference. PNG inspected:
road/building/material quality remains schematic; this is a data correctness fix.

Deleting `elevation/15/17165/11203` from a separate cache makes preparation fail
before playback, with one cache miss, no PNG/network. Owned failure propagation
now reports raw requested/served `15/17165/11203`, `terrarium.s3`, its source key
and offline-miss cause; stitched `15/17164/11202` remains separate context.
The complete-cache replay still has 538 hits, no misses/network, identical pixels.
Terrarium now uses the shared 404-only absence rule. Scripted 404/403/401 and
subsequent recovery pass; restoring the former 403 rule fails the same test.
Full-lap/alternate-pacing proof and typed absence/error propagation remain open.

## View preparation contract

- `Engine::prepareViewData(durationS, patienceS)` prepares the active static/route
  view through ceil(duration/step) future ticks. It does not advance simulation,
  change the active view, draw or upload. Groundless scenes succeed; unsupported
  dynamic views fail. Geographic camera height resolves under the same deadline.
- Route pose, geographic conversion and sight-to-level calculation are shared
  with runtime streaming. Capture invokes preparation after bootstrap/view
  selection, before advancing, for both motion and static output.
- `engine/streaming/TerrainPathPreparation` owns canonical unions and vector-then-
  height settlement: at most 216001 points and 8192 unique addresses per kind.
  Admit bounded batches, refuse oversized plans before oversized IO. OSM windows
  share `OsmField::SourceWindow`; classifier windows come from `ClassField`.
- `PlanPatchworkTiles` covers every possible mesh block independent of residency.
  `PlanTerrainSourceTiles` supplies halos, parents, vector/building and route
  fields to both candidates and path preparation. `GroundStream::SamplingCoverage`
  owns normal/coarse mapping; every mesh block includes its coarse parents,
  including seam crossings. Native grid maximum belongs to `Data::TileId`.
- CPU/IO preparation is synchronous on the Engine/video thread with one global
  deadline; bounded work units may overrun it. Partial cached progress survives
  failure. No fake heights, readiness weakening, or rendering-driven requests.

## Error and absence contract

- Owners: `world/data/{Delivery,SourceSet,ContentStore}`, `ground/TilePool`,
  `ground/TerrainLoader` and its terrain byte/stitch adapter; preparation consumes
  their typed outcome. Preserve status/progress contracts of current callers.
- Owned `Data::FetchFailure` carries kind, requested address, optional
  served address, source ID/revision and reason (unknown offline cache, provider
  refusal, timeout/cancellation, corrupt payload or capacity refusal). Sources
  identify the actual attempted provider; do not infer one from an empty result.
  Carry it with refused delivery/job results through field decoding and stitching.
  Keep the failed raw address distinct from the aggregated field address.
  Success/pending paths allocate no diagnostic strings. No global last-error slot.
  Store the failure in Delivery, TilePool Landing/Result/refusal CacheEntry,
  TerrainBytes and TerrainGrid; move it through worker publication and stitching.
  Bytes/Field/PollStitchedField expose an optional owned failure alongside status;
  refusal cache hits reproduce the original failure. Decoding creates corrupt-
  payload failures with the delivered source identity/address. Wire/Fetched must
  explicitly distinguish offline miss from provider refusal, not infer it from
  Ticket::None. Deadline/admission failures keep their own cause and context.
- Exact source identity and served address must survive ancestor mapping and
  provider fallback. Preparation formats this owned failure; retain the generic
  field address as context, never as a substitute for the offending request.
- ContentStore distinguishes bytes, confirmed absence and unknown cache miss.
  Persist absence only for authoritative 404 under the same source key/revision,
  with bounded freshness for unpinned data and bounded metadata storage. Never
  cache 403, timeout, cancellation or corrupt bytes as absence. Terrarium's 403
  handling must be corrected by removing its special absence override; the shared
  WebTileSource 404-only rule applies. A TerrariumDem test uses scripted HTTP
  replies through SourceSet: 404 alone hands over, 403 refuses without fallback,
  and a subsequent 200 remains fetchable. Existing raw cached bytes remain readable; any
  new record format is versioned. Do not pretend unavailable height is zero.

Missing/cached-refusal, provider fallback and ancestor identity are implemented.
Independent edge/diagonal stitch tests pass; removing raw-failure propagation fails
them. Native timeout/cancel mapping, corrupt/capacity failures, persistent confirmed
absence and full-lap/alternate-pacing proof remain open.

## Acceptance

- Analytical path rectangles, fallback seams, permutation/duplicate invariance,
  invalid/budget input, no-IO planning, groundless API and capture mutation tests.
  Removing the coarse-parent expansion must fail its independent oracle.
- Online prepare/capture then immediate offline capture in an isolated cache
  succeeds with zero required misses/network and identical pixels. Different
  worker/playback pacing requires the same address set. Complete Hockenheim lap
  uses this preparation and preserves WI 2260 route/contact invariants.
- Delete one required tile: fail before PNG/network with the actual source key,
  raw requested/served address and cause. Test ancestor mapping independently.
- Inject 404, 403, timeout/cancellation and corrupt payload independently: only
  404 may become confirmed absence; all other outcomes retain their cause.
- Focused SourceSet/ContentStore/HeightSheets/path/client cases, `make format`
  and `LINT_JOBS=2 make lint` pass. Compare via `test/scripts/pixels.py`.
