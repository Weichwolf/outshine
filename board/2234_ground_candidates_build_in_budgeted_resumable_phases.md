Type: defect
State: active
Architecture: ready
Parent: 2105
Depends: 2245
Priority: P0
Area: engine, world, rendering
Tags: streaming, realtime, ownership

# Ground candidates build in budgeted resumable phases

## Verified state and immediate defect

Candidate ownership, phase scheduling and atomic publication exist. Floor-contact,
Lattice and paced-readiness fixtures passed at bd8693885. These establish readiness
and local contracts, not equal native products under different pacing or frame budgets.
Historical single-run timings are in Git; no p95/p99 claim follows from them.

The Refined oracle reaches readiness in both paths but native products differ.
Reversing execution order excludes cache warmth alone. Terrain source, refined and
haloed nodes are byte-identical; tile IDs and product counts match. Footprint order
and heights then diverge, followed by pressed terrain and renderer geometry. The
first differing input is `OsmField::Tiles()` order: `Build` appends successful vector
downloads in arrival order, so tile/feature/point indices change with pacing. Waiting
for all terrain meshes does not repair this; the probe was removed. WI 2245 repairs
the vector identity boundary before product equivalence can pass.

The release regression is reproduced: with NDEBUG, the paced engine tries to start
another candidate while its renderer still owns the first. The three mutating schedule
calls in Laying.cpp now execute outside assert. Place integrations run the same tests
with NDEBUG in the actual engine, not only in their test translation units. The object
cache keys include validation/sanitizer defines to prevent stale variant reuse.

## Decision

Use the existing GroundWorldCandidate owner and GroundBuildSchedule. No parallel
transaction, second publication state machine or public test-only stepping API.
Evaluate each transition unconditionally; assert only the stored result. Keep release
builds warning-clean and prove actual engine progress with NDEBUG. Audit analogous
side-effectful assertions in the candidate path while fixing the three confirmed sites.

Each candidate owns a coherent input revision and its intermediate products. Retain
input owners through worker completion. Cancel obsolete candidates; never splice
new revision data into completed phases. Only complete validated products replace the
active world. Failure and cancellation preserve the published world.

Engine::advance and preload must orchestrate the same production operations. Their
first publications now both use Playable quality; advance had incorrectly labelled its
contact-only source request Refined. Both then continue to Refined visual coverage.
Freeze input revision, coverage, quality and simulation time before comparing results.
Do not call different-quality image equality a valid oracle.
Use existing internal contracts for controlled equivalence tests and public API for
end-to-end progress. Do not add a second rendering client.

## Ordered implementation

1. Preserve the NDEBUG regression and immediate advance-error diagnostics in
   GroundCandidatePacingReachesReadiness. Readiness alone is not equivalence. Compare
   native geometry, contact data and revision under identical inputs with different
   interruption schedules. Deliberately early publication must fail the oracle.
2. Measure bounded units in Laying.cpp, GroundWorldCandidate and TerrainTileUpload.
   Record maximum input sizes, p50/p95/p99 and CPU/GPU memory separately. Existing
   OwnedHeapBytes covers selected direct CPU products, not total engine residency.
3. Continue the longest over-budget unit by tile/row/batch, preserving topology and
   stable reduction order. Whole named phases are not automatically bounded units.
   Test cancellation, stale completion, submission failure and retry; publication once.

Memory accounting belongs to WI 2228/2244; admission integration to WI 2233.
These do not block the release-state fix or controlled product-equivalence tests.
Expected image: unchanged completed world, no partial terrain/contact revision;
streaming preparation must stop monopolizing frames under measured workload.

## Acceptance

- Actual engine progresses and publishes with and without NDEBUG.
- Interrupted and uninterrupted equivalent-input builds produce identical native products.
- Active world remains intact during preparation, rejection and cancellation.
- Per-unit distributions, maximum workload and candidate peaks support stated budgets.
- Existing Floor and Lattice 15 s/contact contracts remain intact.
- make format; focused schedule and paced-publication cases; Floor/Lattice integrations;
  make lint. Keep logs in system temp; report only failures and final counts.
