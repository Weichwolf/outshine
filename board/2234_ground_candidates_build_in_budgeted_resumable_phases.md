Type: defect
State: active
Architecture: ready
Parent: 2105
Depends: 2245, 2246
Priority: P0
Area: engine, world, rendering
Tags: streaming, realtime, ownership

# Ground candidates build in budgeted resumable phases

## Verified state and immediate defect

Candidate ownership, phase scheduling and atomic publication exist. Floor-contact,
Lattice and paced-readiness fixtures passed at bd8693885. These establish readiness
and local contracts, not equal native products under different pacing or frame budgets.
Current client run 590696be3, Malcesine without vegetation, through refinement:
p50 2.08, p95 4.25, p99 598.35 ms; 36/2408 frames exceed 16.67 ms.
Simulation p99 597.81/worst 671.65 ms; draw p99 2.12/worst 9.98 ms; peak heap 852 MB.
Latest candidate: 2887 haloed/rendered sheets; longest earthwork slice 11.671 ms,
initial/native mesh slices 10.677/11.127 ms, Floors slice 1.204 ms.
Earthworks total 727.445 ms and haloing total 205.550 ms are accumulated phase costs,
not per-frame maxima. These samples do not attribute the remaining ~672 ms stall.
Instrument remaining synchronous ingestion/rebuild/commit phases before splitting
FloorsOf merely because it is still unsliced. Preserve the full refinement window.
Static candidate ownership, sliced mesh/residency and reject-before-write already
work; retain them and identify the first measured unit exceeding its tick budget.
Historical timing sequences remain in Git, not as current acceptance evidence.

The Refined oracle now passes for preload, paced advance and a repeated paced run,
also with NDEBUG. The defect was a combination of arrival-ordered `OsmField` indices,
completion-ordered footprints and unequal first vector requests: preload asked for
contact, advance for the full ring. WI 2245 owns source snapshot identity and its
remaining missing/retry and budget proofs. Equality is proven for this fixture;
other inputs and per-unit frame bounds remain open.

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
The client now measures 120 streaming frames before pinning capture and requires
Refined readiness for its shot. Capturing immediately after Playable preload had
frozen a nearly empty Malcesine frame. The corrected shot has 110345 triangles;
visual inspection and pixel comparison against the previous completed shot show
0.8864% changed pixels. This restores the image oracle, not visual acceptance.
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
   Start with earthworks, terrain mesh and corridors. Attribute each sampled frame
   to its active substep and retained candidate bytes; prove total frame work, not
   merely the time of a named phase after moving it elsewhere.
3. Continue the longest over-budget unit by tile/row/batch, preserving topology and
   stable reduction order. Whole named phases are not automatically bounded units.
   Test cancellation, stale completion, submission failure and retry; publication once.
   Review through 21342822f: `PressPointsJob` now completes global rejection before
   applying any height changes, retaining cursors and decisions. Preserve that rule.
   Remaining unsliced work: bucket construction in its constructor and both
   `FloorsOf` passes in `TerrainPressJob::Advance(Floors)`. Also measure
   `OsmField::PublishParsed`, which rebuilds the entire resident vector snapshot.
   Do not call these bounded because surrounding loops yield. Malcesine-762c673c
   measured sim p99 597.81 ms and draw p99 2.12 ms over the refinement run;
   those aggregate values do not identify the responsible phase. Measure first,
   split the dominant unit, preserve global decisions and completed native products.

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
