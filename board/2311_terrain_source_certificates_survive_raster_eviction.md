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

Accepted readiness formerly re-resolved DEM after raster eviction. Larger caches/pins
hid the ownership defect and increased memory. TilePool now owns TerrainRevisionIndex
independently of raster residency. TerrainCertificate records owner, shape scope and
requested-address stamps. TerrainField/HeightField preserve stitch dependencies through
copy/share/resampling; BuildingField::AcceptedInput owns accepted source certificates.

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

## Active step: pure building-source inspection

- Owners: StructureBuildQueue.h/.cpp and HeightSource::InspectCertificate callback.
  InspectCellSource returns its own five-state CellSourceState. It reads vector/street/
  span/source-key metadata, known scope and certificate without callback copies or a
  resolver. No fallback to blocking CertificateCurrent, even on misses/unknown/pending.
- A shared AcceptedSourceMetadataCurrent comparison prevents post/landing drift.
  Missing acceptance/inspection/scope is Unknown; known input differences are Stale or
  ScopeChanged. Terrain callback statuses remain distinct. No certificate callback can
  override known source differences or unknown accepted scope.
- Former CellSourceCurrent is ValidateResidentCellSource; the allocating internal operation
  is ValidateCellSource. Preserve their resident-resolution behavior and existing fixtures.
  Frame activation still uses this explicit validator until revalidation is integrated.
- Test HIT/MISS/Unknown/Pending/Stale/Scope and missing inspector for zero allocations and
  zero Sample/CopyField/ResidentField/CertificateCurrent calls. Restore the resolver on the
  pure path, ignore unknown scope or bypass metadata checks: each must produce actual FAIL.

## Evidence

987af742a: 13 official focused tests, full lint 256/256 tidy units without findings,
32/32 guards PASS. Actual-delivery/mutex fixture: 26 checks; independently blocking each
of Queue/Cache/Index gives actual FAIL. Zero allocations for all inspection states.
Index: 4096*32-byte entries + 1-byte domain = 131073 bytes before allocator/owner overhead;
raster budget 16 MiB. Logs: /tmp/outshine-terrain-inspection-{focused,full-lint}.log.
Pure source inspection: 33 direct checks PASS; restored resolver, ignored unknown scope
and disabled metadata comparison each give actual FAIL. cf58d54a1 passed 4 focused tests,
but tidy found two absent engine inspector initializers. 4871add5a wires both: 4 focused
PASS, full lint 256/256 units without findings, 32/32 guards, API docs PASS; process exit 0.
Logs: /tmp/outshine-source-inspector-wiring-{focused,full-lint}.log and gate-result.txt.
d91dc0cdb implements request capture/acceptance/heap accounting and three owner fixtures;
format 1203 files, zero errors. 46bbf8c48: HeightField 5, BuildingField 5, queue 4 PASS.
Ancestor substitution, reversed order and block zoom instead of field zoom give actual FAIL.
Logs: /tmp/outshine-height-request-{heightfield,buildingfield,queue,controls}.log.
Full lint still running; no frame-cost claim.

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
without rebaking. Changed input revokes activation and requests existing replacement flow.
Before replacing the frame validator, specify queue/byte caps, polling/backoff and error
states; prove no retry storm, starvation or borrowed mutable reader. Runtime cost/visual
acceptance remains open; standalone inspectors do not establish a bounded frame path.
Inspection hashes tile way points and accepted sources each time; bound/cache this work
under the actual owners/generations before claiming bounded frame inspection.
StructureCellsReady only reads plan.Active: integrate validation/replacement for active
fronts too; checking inactive activation alone cannot reject an expired fine front.

Acceptance: make format; revision/terrain/HeightField/BuildingField/StructureBuildQueue/
GroundPublication suites; full lint including clang-tidy/API guards. Render Hockenheim
static/paced with outshine-client, open PNGs, measure cold/warm/moving work, p50/p95/p99
and CPU/GPU bytes. CPU-only evidence does not close the Runtime/visual integration.
