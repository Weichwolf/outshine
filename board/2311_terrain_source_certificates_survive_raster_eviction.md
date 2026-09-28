Type: feature
State: active
Architecture: ready
Parent: 2298
Depends:
Priority: P0
Area: world, streaming
Tags: terrain, certificates, measured

# Terrain source certificates survive raster eviction

## Problem and foundation

StructureBuildQueue::CurrentCellSource resolves and combines DEM again to check
source identity. Resident pointer equality helps only until stitched fields evict.
Producer revision and terrain scope are distinct. SourceSet is sealed per TilePool,
but byte eviction and later deliveries require per-address metadata.
Partial boundaries cannot certify. 2310 contracts pass tests and full lint; its remaining
GPU transition render is visual integration evidence, not a certificate-code blocker.
No permanent raster pin, global terrain epoch or larger raster-cache workaround.

## Current implementation

Bounded delivery metadata survives raster eviction; fresh bytes/absence revoke prior
stamps. Cached bytes restore missing metadata without IO; registration and delivery
identities remain distinct. InspectStamps differentiates unknown from known stale.
TerrainField carries every requested stitch dependency, including shared ancestors;
HeightField preserves certificates through copies/shares/resampling and ground views.
Partial/fallback/empty inputs cannot certify. Certificate/sampled-ground heap is counted.
AcceptedInput owns its certificate; readiness reads metadata/resident fields only.
Valid raster-free readiness performs zero copies and heap allocations. Whole-tile
posting/landing validate again and move refreshed certificates into acceptance.
Nine focused tests and code/API gates pass; WI size was corrected by lint-docs.
Effective mutations cover domain, publication, decode, unknown/stale, transfer,
readiness, late landing and refreshed acceptance. Hockenheim PNG/performance is open.
Shaped fields now retain scope; candidate/publication and every structure path check it.
Ten focused tests pass; the old cell-post path fails its added test before the fix.
Disabling ScopeCurrent fails seven checks. Full scope gate passes with zero tidy/API findings.
Warm index microbenchmark (local host, concurrent lint, not a frame/A18 bound):
4096 entries, 99 batches; 9 dependencies: p50/p95/p99 0.1423/0.1430/0.1483 us.
4096 dependencies: 124.750/146.791/210.167 us. All measured queries allocate zero.
Payload: 4096 * 32-byte entries + 1-byte domain = 131073 bytes, excluding allocator
and shared-owner overhead. Compile -O3 -fno-exceptions; benchmark artefacts are in
/tmp/outshine-terrain-index-benchmark.cpp and .json. Moving/contended costs are unproven.
Candidate comparison/diagnostic mask now live in GroundRevision; Laying delegates.
Preserve restart fields and ignore residency-only changes; directly test scope,
quality, coverage and reasons pass; removing scope fails two CPU checks. Full gate passes.
Naming follows 2139; no compatibility aliases.

## Accepted-input implementation contract

TerrainCertificate captures scope for provider and shaped fields. Merge rejects
conflicting known scopes even without stamps. HeightSource has separate TerrainScope;
producer Revision stays independent. Guard posting/readiness/landing against old scope.
GroundRevision captures pool scope and restarts candidate/publication/corridor
preparation after changes. Hold producer revision/content fixed while varying scope;
the test must fail if scope checks are removed.
TilePool validates scope and stamps while holding queue/cache publication locks.
HeightSource exposes a read-only CertificateCurrent callback. AcceptedInput owns
its certificate. Current vector/street identities, tile span and qualified source
key guard the certificate fast path. Readiness may only read resident fields on a
miss. NextLandings receives HeightSource, not just its producer revision. Validate
street digest plus captured certificate before activation; otherwise resolve again
and compare the whole-tile source key. Re-resolved instrumented inputs must certify.
Posting rejects revoked instrumented inputs before reserving work. Retire stale
finished fronts and release only owned reservations; preserve valid batch pointers.
Move the newly validated certificate into acceptance after same-key re-resolution.

## Ownership and data flow

- world/ground/TerrainRevisionIndex owns bounded metadata for requested DEM TileId.
  TilePool owns the index; initial cap 4096 entries [SET], configurable 1..65536.
  Entries have fixed-size address/stamp fields, no bytes or source strings.
  Sorted lookup; evict the oldest delivery revision. Reads never change eviction order.
  Report metadata heap separately; include certificate vectors in owner HeapBytes.
- A fresh provider delivery or explicit absence revokes the previous address stamp.
  Observe under the same synchronization as raw cache publication. The returned
  landing carries the stamp for its actual bytes, not a later independently read stamp.
- Raw-cache hits reuse valid stamps. Metadata eviction revokes old certificates;
  serving retained bytes can issue a new stamp without IO. Raw-byte/raster eviction
  alone preserves metadata. No stamp reuse while old products can reference it;
  a new TilePool domain cannot validate an old pool's certificate.
- TerrainSource/TerrainBytes expose optional internal stamps. Uninstrumented
  sources remain supported, but cannot certify a raster-free cache hit.
- TerrainTiles records every requested dependency used for centre, edges and
  corners, even when several requests serve the same ancestor. Delivered source
  identity remains separate. Missing/corrupt/deferred dependencies cannot certify.
  Raw decoded-cache hits also validate stamps; stale decode cannot mask new data.
- An immutable TerrainCertificate contains owner domain, shape scope and bounded
  requested-address/stamp dependencies (maximum 4096 [SET]). TerrainField/HeightField propagate it.
  Copy/share/resample preserve dependency ownership; no raster handles in certificates.
- GroundStream validates certificates without fetch, decode, allocation or queued
  work. Shape scope follows 2310. Validation observes a coherent metadata snapshot.
- BuildingField AcceptedInput stores the accepted certificate. StructureBuildQueue
  first checks vector/street identity, tile span, product source key and certificate.
  A valid hit avoids ResolveHeights. Unknown/revoked certificates fully resolve;
  posting and activation validate again. Readiness never prepares terrain.
- Files: TerrainRevisionIndex.h/.cpp; TilePool and TerrainDelivery; TerrainTiles,
  TerrainLoader, HeightField, BuildingField; StructureBuildQueue and the HeightSource
  adapter in Advancing.cpp. No public API extension, importer types or global state.

## Independent acceptance

- Revision-index tests: same cached bytes, fresh delivery, explicit absence,
  eviction/re-entry, domain replacement, bounded count and stale-token refusal.
- Fully stitched provider/ancestor fixtures: raster eviction retains a valid
  certificate; a changed edge/corner request invalidates affected cells only.
  Another region's delivery leaves this certificate valid. Partial seams never certify.
- Instrument CopyField/terrain requests: valid accepted certificate after raster
  eviction needs zero copy/decode/fetch jobs. Removing validation admits deliberately
  stale input; disabling the fast path violates the zero-work oracle.
- Revoked index entry or changed shape/vector/street/span takes full resolution;
  absent certificate never becomes an optimistic hit. Late/stale landing is rejected.
- Measure cold/warm/moving work and metadata bytes; no new raster retention.
  Hockenheim static/paced captures converge without historical giant silhouette changes;
  open PNGs and report p50/p95/p99, work, CPU/GPU bytes against WI2298 baseline.
- make format; focused revision/terrain/HeightField/BuildingField/StructureBuildQueue
  suites; SDKROOT="$(xcrun --show-sdk-path)" LINT_JOBS=2 make lint.
