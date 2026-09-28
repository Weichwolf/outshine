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

## Street digest ownership

StreetField owns immutable Way digests and OSM origin/generation. Reuse the existing
OriginToken byte owner, never pin coordinates. Snapshots retain the same context; foreign
domains or later generations cannot hit even at equal counters. Ingest resets old derived
data on context changes and computes the former engine digest (count/width/coordinate bits).
SourceDigest(field,tile) returns optional scalar; unknown context never triggers lazy rehash.
Empty unbound fields/untouched tiles retain the old empty digest. Settle/HeapBytes include
cached words. Queue consumers defer unknown context without resolution. Test independent oracle,
snapshot, width/coordinate/domain/generation changes, unknown context and protected points.
Restored scan must give actual FAIL. Acceptance: format; StreetField/OsmField/GroundStack/
queue suites; full lint/tidy/API. Existing accepted source-key cache remains unchanged.

## Evidence

63908c160: 19 focused PASS; full lint FAIL on three tidy findings. 94581ed9b fixes
include/complexity; full rerun pending.
StagesBakes must retain source rasters through publication/retirement for canonical live LOD.
HeightSheets owns these references and accounts their bytes; prove move lifetime and missing IO.

## Active repair: canonical resident terrain for building detail

fef5b6197 retains canonical rasters; focused PASS, paced publication still FAIL at bakes.
SelectRefinement advances before landing; discarded replacements must rewind their owned tile.
Repair owner: BuildingField retry cursor and StructureBuildQueue::DiscardFront. Foreign domains
never rewind successors. Test revoked replacement delivery, successful retry and no double take.
After ground publication, use the
published HeightSheets source representation for whole/detail preparation and validation.
Add optional CopyResidentField; legacy callers retain ResidentField fallback. Missing fields
must defer without IO; preserve scope/source checks and conservative LOD bounds. Before
publication retain the existing streaming path. Changed DEM still needs a joint candidate.
Test resident-copy selection against a poisoned resolver and changed/missing raster; format,
focused/source-field/Places suites, all ten no-vegetation renders, full lint/tidy/API.

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
