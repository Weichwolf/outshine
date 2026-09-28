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

Prepared 220-s offline lap on `b6bf3b0e1`: 13,200 frames, 4575.927 m,
538 cache hits, zero misses/provider/network starts; fast-forward capture has
identical source counts. Trace: zero station regressions/contact gaps, maximum
position step 0.504150 m, first/last-frame separation 0.000417 m. Frame p50/p95/
p99: 2.720/8.922/12.780 ms; 39 over-budget frames; 885.9 MiB peak heap.
All twelve motion PNGs and the fast still opened. Marks 2–4 have huge torn
polygons; every frame is unrefined. Final still/motion differ at 7348/921600
pixels. WI 2295 owns visual convergence; input closure is separately evidenced.

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
  Admit bounded batches, refuse oversized plans before IO. OSM windows share `OsmField::SourceWindow`; classifier windows come from `ClassField`.
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
  their typed outcome.
- Owned `Data::FetchFailure` carries kind, requested address, optional
  served address, source ID/revision and reason (unknown offline cache, provider
  refusal, timeout/cancellation, corrupt payload or capacity refusal). Sources
  identify the actual attempted provider; do not infer one from an empty result.
  Carry it with refused delivery/job results through field decoding and stitching.
  Keep the failed raw address distinct from the aggregated field address.
  No global last-error slot.
  Store the failure in Delivery, TilePool Landing/Result/refusal CacheEntry,
  TerrainBytes and TerrainGrid; move it through worker publication and stitching.
  Bytes/Field/PollStitchedField expose an optional owned failure alongside status;
  refusal cache hits reproduce the original failure. Recovery replaces expired
  empty refusals before admission, sharing cache-index removal with eviction.
  Recovered requests deliver resident bytes without refetching. AwaitLanding
  checks retained results under QueueMutex before waiting; pre-landed work cannot
  lose its wakeup. Decoder failures retain delivered identity/address/key. Delivery/TilePool
  cache retain the key; TerrainBytes carries it to RawGrid. Invalid PNG, crop or
  served ancestry refuses the raw field; corrupt neighbours prevent publication.
  host/Fetching maps curl timeout/body limits to TimedOut/CapacityRefused. Cancelled ticket IDs use
  a FIFO bounded by MaxRequests; transfers/payloads are freed, Collect consumes
  the cause once. HTTP 408 retries as timeout; cancel/body limits are terminal.
  Transport/Source::Begin returns FetchStart = expected<Ticket, FetchFailureReason>.
  SourceSet::StartCurrent shares admission/context/phase changes with retry.
  Local sources may return successful Ticket::None;
  Native/offline refusal is unexpected, never inferred from None or last-error.
  Fetching uses Unavailable/Ready/Stopping/Failed states under Mutex; worker failure
  prevents new starts. Capacity/ticket exhaustion => CapacityRefused; empty/NUL
  URLs => InvalidRequest; offline => OfflineMiss; unavailable worker => Unavailable.
  Deadline failures keep their own cause. Refused starts are terminal, perform no
  Collect/Cancel or retry and cannot persist absence. Admission after cancellation
  succeeds; independent native-capacity/URL, source/context and retry fixtures
  prove this, including successful local None and source fallback/ancestor.
- Exact source identity and served address must survive ancestor mapping and
  provider fallback. Preparation formats this owned failure; retain the generic
  field address as context, never as a substitute for the offending request.
- ContentStore distinguishes bytes, confirmed absence and unknown cache miss.
  Persist absence only for authoritative 404 under the same source key/revision,
  with bounded freshness for unpinned data and bounded metadata storage. Never
  cache 403, timeout, cancellation or corrupt bytes as absence. Terrarium's 403
  handling must be corrected by removing its special absence override; the shared
  WebTileSource 404-only rule applies. 403 refuses without fallback and later 200
  remains fetchable. Existing raw cached bytes remain readable; never pretend
  unavailable height is zero.
  ContentStore::Lookup returns Bytes/Absent/Unknown; Read remains byte-compatible.
  ContentStoreAbsence.cpp owns regular-file-only `.outshine-absence-v1` sidecars,
  bounded expiry index (default 4096 entries, configurable, maximum 65536), atomic
  publication and eviction. Config injects UTC seconds for expiry tests. TTL is
  24 h without revision, seven days with a pin. Bytes win and invalidate absence.
  Fetched carries explicit HTTP-not-found evidence; generic Absent cannot persist.
  SourceSet consumes cached absence through the same Fail/Continue policy as 404;
  cached Fail reports ConfirmedAbsent, never OfflineMiss or a fabricated height.

Owned transport/decoder failures, bounded absence, recovery and state-based
landing waits have independent fixtures and effective negative controls.
Admission context survives first/fallback/retry starts. TerrainDelivery.h maps
both adapters retain owned CorruptPayload context: five focused tests and full lint pass.
Exact alternate-pacing address-set and image convergence remain open.

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
