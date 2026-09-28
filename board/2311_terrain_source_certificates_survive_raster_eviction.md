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

5de351d40: 45 focused PASS, full lint 255/255 tidy-Units, zero findings. Historical fixtures
cover delivery/absence, metadata/raster eviction, domain, seams/ancestors, scope,
copy/share/resample, publication/decode and refreshed landing acceptance.
Index payload: 4096*32-byte entries + 1-byte domain = 131073 bytes, excluding allocator/
shared-owner overhead. Raster budget remains 16 MiB. No contended/A18 frame claim.

987af742a: 13 official focused tests, full lint 256/256 tidy-Units without findings,
32/32 guards PASS. Direct actual-delivery/mutex fixture: 26 checks. Three independent
blocking Queue/Cache/Index mutants give actual FAIL, not BUILD/TIMEOUT. Zero allocations
cover Current/Unknown/Stale/Domain/Scope/Pending. Format: 1202 files, zero errors.
Logs: /tmp/outshine-terrain-inspection-{verification,focused,full-lint}.log and case logs;
gate-result.txt identifies the checked commit. No Frame-consumer acceptance implied.

Pure source inspection: 33 direct checks PASS. Restored resolver, ignored unknown scope
and disabled metadata comparison each give actual FAIL. Existing resolver assertions stay.
Logs: /tmp/outshine-source-inspection-verification.log and case logs; format 1202 files.
Official StructureBuildQueue suite/full lint for this change are pending.

## Runtime integration still open

The current frame validator still allocates on misses and may wait. Replace it only with
bounded coalesced preparation per tile/source; Unknown/Pending must not cause a retry storm,
permanent activation starvation or immediate geometry loss. Source-matching completion
refreshes acceptance without rebaking identical geometry; changed inputs revoke activation.
Uninstrumented sources remain supported through explicit preparation, never optimistic hits.
Worker inputs need owned immutable metadata and a proved source-reader lifetime/thread
contract. Do not copy borrowed Engine-/Stack-capturing callbacks into a worker unexamined.

Acceptance: make format; revision/terrain/HeightField/BuildingField/StructureBuildQueue/
GroundPublication suites; full lint including clang-tidy/API guards. Render Hockenheim
static/paced with outshine-client, open PNGs, measure cold/warm/moving work, p50/p95/p99
and CPU/GPU bytes. CPU-only evidence does not close the Runtime/visual integration.
