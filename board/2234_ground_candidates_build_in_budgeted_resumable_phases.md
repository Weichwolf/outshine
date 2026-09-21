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
On 2026-09-21 Malcesine's 120 measured frames after Playable preload gave
p50 3.03, p95 694.92 and p99 711.90 ms, with 41/120 above 16.67 ms and
564 MB peak heap. Advance dominates (p99 711.43 ms); render p99 is 4.90 ms.
These are observed client timings, not accepted budgets. Find and slice the
responsible native build/publication units without moving work out of the
measurement window.
The repeated `shots --no-vegetation --measures Malcesine` run gave p95 640.81 ms
and 39/120 over budget. Its last candidate reported earthworks 395.085 ms,
terrain mesh 183.774 ms and corridors 72.491 ms. These phase samples identify
where to instrument next; they are neither per-frame maxima nor distributions.
The follow-up split measured 47.472 ms in sheet handoff and 140.658 ms in
native meshing across 2887 sheets. Both mesh passes now advance 96 sheets per
frame. Stitching took 0.856 ms; residency took 46.336 ms before slicing.
Residency now prepares the complete key set, stages 128 pages per frame in the
isolated candidate renderer and publishes terrain tiles only after the last page.
Malcesine reached Refined after 154 measured frames: longest residency slice
7.745 ms, p95 668.08 ms, 37/154 over budget. Earthworks and corridors remain
unbounded. The image is visually unchanged, but 0.0601% of pixels differed
near one shore building in an earlier run; diagnose input readiness versus
geometry. Shot timing continues after 120 frames only until Refined, capped at
240 measured frames. The direct staged/one-shot digest and late GPU failure
tests pass; candidate isolation still needs cancellation and memory-peak proof.

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
   `PressPoints` in `GroundYield.cpp` has two ordered full-node passes: the first
   globally marks stamps whose requested cut/fill exceeds the earthwork bound;
   the second excludes those stamps and changes heights. A resumable press must
   finish the first pass for all nodes before changing any height. Keep the bucket
   index, rejection flags, positions, original heights and second-pass cursor in
   candidate-owned state; then convert changed nodes back to geodetic heights and
   calculate floors in deterministic source order. Do not split by stamp or publish
   partly pressed sheets. Prove byte-identical results against the current one-shot
   algorithm, including overlapping stamps and a late rejected stamp.

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
