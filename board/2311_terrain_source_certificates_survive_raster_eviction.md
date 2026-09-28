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

## Active step: cache accepted source identity at its owner

Move StructureSourceKey.h and fixtures to world/ground; preserve algorithm/key bytes.
PendingAcceptance canonicalizes Sources and computes AcceptedInput::SourceKey once from
its OWN immutable Vector/Sources/Bake data, only when Qualified. QualifiedSourceKey reads
this scalar. Commit/replacement/snapshot/reset carry or recompute it with its owner.
Certificate-only renewal preserves it. Test canonicalization, mutations, snapshots and
unqualified acceptance. Protect an interior source-string page in a child and invoke the
real queue getter: lookup must succeed; restored rehash must fail the parent check safely.
Acceptance: format; native StructureSourceKey/BuildingField/queue suites; full lint/tidy/API.
Existing frame consumers stop hashing sources repeatedly; street scanning remains.

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
Format: 1204 files, zero errors. Official focused/full cache gate pending; no frame claim.

## Active step: retain exact height requests

HeightField owns Request {Zoom, ordered TileSpots, Fallback}, captured from Blocks without
sources/rasters. BuildingField::AcceptedInput owns this recipe; PendingAcceptance transfers
it at preparation, snapshots copy it, HeapBytes counts it. StructureBuildQueue records the
captured task recipe at whole-tile landing. Preserve order: RasterDigest folds field zoom,
then block addresses and samples in order. Sources are resolved ancestors; certificate
seam dependencies are not original requests. Neither can reconstruct the recipe.
Test different requested children sharing one ancestor, order/zoom, producer destruction,
snapshot/reset and heap accounting. Legacy/manual acceptances have no recipe; inspection
still uses certificates, explicit resident validation remains available.

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
Inspection scans tile way points; bound/cache street work under actual owners/generations
before claiming bounded frame inspection.
StructureCellsReady only reads plan.Active: integrate validation/replacement for active
fronts too; checking inactive activation alone cannot reject an expired fine front.

Acceptance: make format; revision/terrain/HeightField/BuildingField/StructureBuildQueue/
GroundPublication suites; full lint including clang-tidy/API guards. Render Hockenheim
static/paced with outshine-client, open PNGs, measure cold/warm/moving work, p50/p95/p99
and CPU/GPU bytes. CPU-only evidence does not close the Runtime/visual integration.
