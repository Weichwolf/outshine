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
before playback, with one cache miss, no PNG/network. The error instead reports
stitched field `elevation/15/17164/11202`: the failing raw source address is lost.
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

## Remaining error and absence implementation

- Owners: `world/data/{Delivery,SourceSet,ContentStore}`, `ground/TilePool`,
  `ground/TerrainLoader` and its terrain byte/stitch adapter; preparation consumes
  their typed outcome. Preserve status/progress contracts of current callers.
- Introduce an owned `Data::FetchFailure`: kind, requested address, optional
  served address, source ID/revision and reason (unknown offline cache, provider
  refusal, timeout/cancellation, corrupt payload or capacity refusal). Sources
  identify the actual attempted provider; do not infer one from an empty result.
  Carry it with refused delivery/job results through field decoding and stitching.
  Keep the failed raw address distinct from the aggregated field address.
  Success/pending paths allocate no diagnostic strings. No global last-error slot.
- Exact source identity and served address must survive ancestor mapping and
  provider fallback. Preparation formats this owned failure; retain the generic
  field address as context, never as a substitute for the offending request.
- ContentStore distinguishes bytes, confirmed absence and unknown cache miss.
  Persist absence only for authoritative 404 under the same source key/revision,
  with bounded freshness for unpinned data and bounded metadata storage. Never
  cache 403, timeout, cancellation or corrupt bytes as absence. Terrarium's 403
  handling must be corrected. Existing raw cached bytes remain readable; any
  new record format is versioned. Do not pretend unavailable height is zero.

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
