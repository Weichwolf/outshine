Type: feature
State: active
Architecture: ready
Parent: 2298
Depends:
Priority: P0
Area: world, streaming
Tags: terrain, ownership, revisions

# Terrain source certificates survive raster eviction

## Problem and implementation

Accepted building readiness used full DEM resolution after raster eviction, creating
work on a query path. Bigger caches/pins raised memory and did not fix ownership.
TilePool now owns a bounded TerrainRevisionIndex independent of raster residency.
TerrainCertificate carries owner domain, shape scope and requested-address stamps.
TerrainField/HeightField retain every stitch dependency through copy/share/resampling.
AcceptedInput retains the accepted certificate; partial/fallback/empty input cannot certify.

## Binding ownership and validity

- TerrainRevisionIndex stores address/registration/delivery fields, no bytes or strings.
  Capacity 4096 [SET], configurable 1..65536. Evict oldest delivery; reads do not touch LRU.
  Fresh delivery or explicit absence revokes previous stamps under raw publication locks.
  Raw-cache hits can restore metadata without IO; raster eviction alone preserves it.
  Another pool domain cannot validate old certificates. No reuse of live delivery stamps.
- TerrainDelivery carries the stamp belonging to actual delivered bytes and owned refusal
  context. Registration and delivery identity differ. Unknown is distinct from known stale.
- TerrainTiles records centre/edge/corner requests, including shared ancestor requests;
  source identity is separate. Missing/corrupt/deferred seams do not qualify. Decoded and
  stitched cache hits validate stamps. Shape scope follows the implemented 2310 contract.
- HeightSource exposes TerrainScope and a read-only CertificateCurrent callback separately
  from producer revision. BuildingField owns accepted certificates. StructureBuildQueue
  certified readiness reads metadata only: no CopyField, decode, fetch or allocation.
- Posting/landing validate vector/street/span/source identity, generation, scope and stamps.
  Unknown/revoked acceptance resolves during bounded preparation, never during readiness.
  A whole-tile landing may re-resolve and compare its complete key; move the newly validated
  certificate into acceptance. Release only owned reservations; stale fronts cannot activate.
- GroundRevision owns candidate comparison/difference masks and includes terrain scope.
  Candidate/publication/corridor preparation restart after change; residency-only changes
  do not restart the world. Certificates/sampled fields participate in owner HeapBytes.

## Evidence and remaining work

At 5de351d40: affected suites belong to 45 focused PASS; full lint passes 255/255 tidy
units, zero findings. Fixtures cover delivery/absence, metadata/raster eviction, domain,
late neighbours/ancestors, copy/share/resample, scope and zero-allocation/copy readiness.
Historical independent mutations cover publication, decode, domain, stale scope, landing
and refreshed acceptance. Larger raster caches are explicitly rejected as a workaround.

Index payload: 4096 * 32-byte entries + 1-byte domain = 131073 bytes, excluding allocator
and shared-owner overhead. Historical warm -O3 microbenchmark (concurrent local lint):
9 dependencies p50/p95/p99 0.1423/0.1430/0.1483 us, all queries allocate zero. This is
neither moving/contended cost nor an A18 frame promise. The raster budget stays 16 MiB.
Review gap: CellSourceCurrent's miss path still calls CurrentCellSource/ResolveHeights
with resident-only callbacks, building vectors/shared HeightField allocations. The
zero-allocation oracle covers certified HITS, not these misses. This violates the
intended pure readiness boundary; do not infer general zero-work readiness.
Next slice: separate metadata inspection from bounded source revalidation. On a miss,
report NeedsValidation and queue bounded preparation; source-matching completion may
refresh acceptance without rebaking identical geometry. Keep uninstrumented sources
supported through explicit preparation. Rename the allocating validation operation;
its existing resident-validation fixtures retain their behavior. Add zero-allocation
MISS/unknown/stale fixtures and a control restoring the resolver on the inspection path.
Also open: fresh Hockenheim static/paced PNGs and cold/warm/moving cost evidence.

## Acceptance

- Valid raster-free accepted inputs require zero copy/decode/fetch jobs and allocations.
  Changed edge/corner/provider/shape/vector/street inputs revoke affected products only.
- Uninstrumented sources remain supported through explicit resolution, without optimistic
  certificate hits. Disabling scope/stamp validation or the zero-work path must FAIL.
- Owners: TerrainRevisionIndex/TerrainDelivery/TilePool/TerrainTiles/TerrainLoader,
  HeightField/BuildingField, StructureBuildQueue and Advancing's HeightSource adapter.
- make format; focused revision/terrain/HeightField/BuildingField/StructureBuildQueue/
  GroundPublication suites; full lint including clang-tidy/API guards.
- Render Hockenheim static/paced through outshine-client, open PNGs and measure work,
  p50/p95/p99, CPU/GPU bytes. CPU evidence alone does not close visual integration.

## Readiness-Contention und Zustand

TilePool::CertificateCurrent hält QueueMutex_ und CacheMutex_, darunter sperrt
TerrainRevisionIndex::AreCurrent seinen Mutex. Der Hit-Beleg ist unkontendiert.
Ein try-lock nur im Index genügt deshalb nicht: alle drei Frame-Locks müssen nicht wartend sein.
Null Allokationen beweisen keine begrenzte Wartezeit im Frame. Die reine Inspektion
braucht zusätzlich einen nicht wartenden Versuch; bei Contention kein Current erfinden,
sondern ValidationPending an begrenzte, pro Tile/Quelle zusammengefasste Vorbereitung
übergeben. Blockierende vollständige Validierung bleibt für Worker-/Publikationsgrenzen.
Unknown/fehlende Metadaten, bekannte Stale-Quelle und abweichender Scope müssen getrennt
prüfbar bleiben. Kein per-Frame Retry-Sturm und kein sofortiger Geometrieverlust.
Kontrolle: Writer deterministisch halten, Frame-Inspektion liefert Pending ohne Wait;
danach Release und gleiche Quelle revalidieren. Sleeps sind kein Synchronisationsbeweis.

## Metadaten-Foundation

TerrainCertificate::Validation trennt Current/Unknown/Stale/ScopeChanged/Pending.
TryInspectStamps und blockierende Index-Inspektion teilen den Locked-Kern.
TilePool::InspectCertificate versucht Queue-, Cache- und Index-Lock jeweils einmal;
Scope unter Queue-Lock, Indexzugriff unter Cache-Lock. Busy liefert Pending.
Kein IO/Cache-Touch/Optimistic-Current. Private Test-Peers halten nur echte Mutexes.
26 direkte Lieferungs-/Mutex-Checks PASS; drei unabhängig blockierende Lock-Mutanten
liefern tatsächliches FAIL. Watchdogs beweisen keine Framezeiten. Format 1202 Dateien.
Logs: /tmp/outshine-terrain-inspection-verification.log und Case-Logs.
987af742a: offizielle fokussierte/full Gates laufen; kein Frame-Consumer dieses Slices.

## Aktiver Schritt: reine Building-Source-Inspektion

StructureBuildQueue::HeightSource bekommt einen nicht wartenden InspectCertificate-
Callback; InspectCellSource liefert eigenen CellSourceState mit denselben fünf Zuständen.
Prüfe Vektor-/Street-/Span-/Source-Key-Metadaten, Scope und Zertifikat ohne Resolver,
Callback-Kopien oder Allokation. Kein Fallback auf blockierendes CertificateCurrent.
Gemeinsamer Metadatenvergleich verhindert Drift der Post-/Landing-Validierung.
Bestehendes CellSourceCurrent heißt danach ValidateResidentCellSource; Verhalten und
resident-validation Fixtures bleiben erhalten. Frame verwendet noch diesen Validator.
HIT/MISS/Unknown/Pending/Stale/Scope und fehlender Callback: null Allokationen, null
Sample/CopyField/ResidentField/CertificateCurrent-Aufrufe. Resolver-Mutant muss FAIL.
Format, StructureBuildQueue-Suite, full lint. Queue-Consumer folgt erst mit Revalidierung;
Worker dürfen keine ungeprüften Engine-/Stack-Callbacks aus dem Frame übernehmen.
