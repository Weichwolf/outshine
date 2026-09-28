Type: feature
State: active
Architecture: ready
Parent: 2298
Depends:
Priority: P0
Area: world, streaming
Tags: terrain, ownership, revisions

# Terrain source certificates survive raster eviction

## Problem and existing capability

Accepted readiness formerly re-resolved DEM after raster eviction. TilePool owns revisions
independently of residency; TerrainField/HeightField carry stitch dependencies and
BuildingField::AcceptedInput owns accepted certificates.

## Binding ownership and validity

- TerrainRevisionIndex owns address/registration/delivery metadata, no bytes/strings.
  Capacity 4096 [SET], configurable 1..65536. Evict oldest delivery; reads never touch LRU.
  Fresh delivery/absence revokes old stamps under raw-publication locks. Cache hits may
  restore metadata without IO. Raster eviction alone preserves it; domains never mix.
- TerrainDelivery stamps identify actual delivered bytes and retain refusal context.
  Registration differs from delivery identity. Unknown differs from known stale.
- TerrainTiles records centre/edge/corner and shared-ancestor requests, separately from
  resolved source identity. Missing/corrupt/deferred seams cannot certify. Decoded and
  stitched hits validate stamps; shape scope follows 2310. Partial/fallback/empty input
  cannot certify. Candidates/corridor/publication restart after actual source changes,
  not after residency-only changes. Owners account certificates/fields in HeapBytes.
- Posting/landing validate vector/street/span/source key, generation, scope and stamps.
  A landing may re-resolve its complete key and transfer a newly valid certificate into
  acceptance. Release only owned reservations; stale fronts cannot activate.

## Nonwaiting metadata inspection

- TerrainCertificate::Validation distinguishes Current/Unknown/Stale/ScopeChanged/Pending.
  TerrainRevisionIndex::TryInspectStamps returns optional existing stamp status;
  nullopt means contention. Blocking/try inspection share the same locked classification.
- TilePool::InspectCertificate tries Queue-, Cache- and Index-Mutex once each. Queue protects
  scope, Cache protects index access. Busy returns Pending; no allocation, IO, LRU touch or
  optimistic Current. Complete known scope and correct owner domain are required.
- Private test peers hold real mutexes for deterministic writer fixtures. Inspector must
  return Pending BEFORE writer release. Watchdogs protect test liveness, never prove p99.
  Full blocking validation remains separate at preparation/publication boundaries.

## Pure building-source inspection

InspectCellSource separates five states without resolution, callback copies or blocking
fallback. Shared checks cover vector/street/span/key/scope; fixtures require zero allocation
and resolver calls. Restored resolution or ignored scope/metadata must give actual FAIL.
Frame still uses ValidateResidentCellSource until bounded revalidation is integrated.

## Active step: own street digests at ingestion

StreetField owns immutable Way digests and OSM origin/generation. Reuse the existing
OriginToken byte owner, never pin coordinates. Snapshots retain the same context; foreign
domains or later generations cannot hit even at equal counters. Ingest resets old derived
data on context changes and computes the former engine digest (count/width/coordinate bits).
SourceDigest(field,tile) returns optional scalar; unknown context never triggers lazy rehash.
Empty unbound fields/untouched tiles retain the old empty digest. Settle/HeapBytes include
cached words. Queue inspection/validation/post/landing consume them; unknown context defers
before posting/resolution and cannot call Current. Test independent oracle, repeat ingest,
snapshot, width/coordinate/domain/generation changes, unknown context and protected points.
Restored scan must give actual FAIL. Acceptance: format; StreetField/OsmField/GroundStack/
queue suites; full lint/tidy/API. Existing accepted source-key cache remains unchanged.

## Evidence

987af742a: 13 focused, full lint 256/256 without findings, 32 guards PASS; 26 delivery/mutex
checks and three blocking mutants FAIL. Pure source inspection: 33 direct checks, three
resolver/scope/metadata mutants FAIL; 4871add5a full gate PASS. Logs retained under /tmp.
46bbf8c48: HeightField 5, BuildingField 5, queue 4, full lint 256/256 without findings,
32 guards/API docs PASS. Request ancestor/order/zoom mutants FAIL. Logs:
/tmp/outshine-height-request-{heightfield,buildingfield,queue,controls,full-lint}.log.
Accepted-key cache: 9 direct checks PASS; restored rehash gives actual FAIL through a
protected payload page, never watchdog/SIGNAL/BUILD. Independent little-endian FNV oracle
matches 0x1ae933c37f74e20a; derivation /tmp/outshine-accepted-source-key-oracle.log.
Logs /tmp/outshine-accepted-source-key-{controls,correct,restored-rehash}.log.
41e159998: native key 1, BuildingField 5, queue 5 and full lint PASS; 256/256 tidy units,
zero findings, 32 guards/API docs; process exit 0.
Logs /tmp/outshine-accepted-source-key-{native,buildingfield,queue,full-lint}.log.
Street digest: 25 owner and 35 queue checks PASS; scan/domain/generation mutants FAIL.
Unknown street context invokes no terrain callbacks/resolution and allocates nothing.
Independent 48-byte count/width/f64 oracle: /tmp/outshine-street-digest-oracle.log.
Logs /tmp/outshine-street-digest-{controls,correct,queue-correct}.log.
Format 1205 files, zero errors; official street gate pending; no frame-time claim.

## Exact height requests

HeightField owns Request {Zoom, ordered TileSpots, Fallback}; accepted BuildingField inputs
capture it without rasters, snapshots copy it and HeapBytes counts it. Digest order matters;
resolved ancestors/seam dependencies cannot reconstruct original requests. Legacy/manual
acceptances have no invented recipe and retain explicit resident validation.

## Runtime integration contract

Use synchronized TilePool::Field with worker-local HeightField; never copy GroundStream or
Engine/Stack closures. TilePool consumes Field completions (Holds=false); TerrainTiles
revalidates retained stitched/decoded stamps. No additional persistent field cache needed.
StructureBuildQueue owns coalesced preparation and immutable source metadata. Clear joins
its jobs before Surrounds destroys Tasks or GroundStack; no reader survives its owner.
Unknown/Pending retain coarse geometry and defer fine activation. Completion rechecks live
vector/street/span/key/generation/scope and stamps; identical source refreshes acceptance
without rebaking. Changed DEM content must request GroundWorldCandidate: terrain,
footprints and pieces publish together. Fine-only replacement on unchanged terrain is
forbidden. Delivery changes need revision invalidation distinct from shaped TerrainScope.
Before replacing the frame validator, specify queue/byte caps, polling/backoff and error
states; prove no retry storm, starvation or borrowed mutable reader. Runtime cost/visual
acceptance remains open; standalone inspectors do not establish a bounded frame path.
Cached source/street reads remove repeated hashing; remaining vector identity comparisons
and preparation/task scheduling still need actual frame-cost acceptance.
StructureCellsReady only reads plan.Active: integrate validation/replacement for active
fronts too; checking inactive activation alone cannot reject an expired fine front.

Acceptance: make format; revision/terrain/HeightField/BuildingField/StructureBuildQueue/
GroundPublication suites; full lint including clang-tidy/API guards. Render Hockenheim
static/paced with outshine-client, open PNGs, measure cold/warm/moving work, p50/p95/p99
and CPU/GPU bytes. CPU-only evidence does not close the Runtime/visual integration.
