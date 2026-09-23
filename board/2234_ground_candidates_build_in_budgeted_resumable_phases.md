Type: defect
State: active
Architecture: ready
Parent: 2105
Depends: 2246, 2256
Priority: P0
Area: engine, world, rendering
Tags: streaming, realtime, ownership

# Ground candidates build in budgeted resumable phases

## Verified state and immediate defect

Candidate ownership, phase scheduling, async field preparation, resumable cooks
and atomic publication exist. Rosenheim remains 8e6642f9; with worker fields and
smaller terrain batches its p99 is 5.04 ms, 0/3821 frames exceed 16.67 ms.
Malcesine previously rendered 36/2456 frames over budget. At 0f5b955a6 and
pre-worker 7ed2bdb17 it instead misses the 15 s preload deadline in
`structure-bakes`: posted=1, landed=0, queue=0. The one task is discarded when
its height-source revision changes; vector, focal length, tile span and eye
still match. `GroundWorldCandidate` copies `BuildingField` including a pending
reservation, but the old task belongs to another height-source revision. The
candidate then has a taken tile without a task; `Next` cannot schedule it.

The Refined oracle now passes for preload, paced advance and a repeated paced run,
also with NDEBUG. Canonical `OsmField` publication, bounded active windows, source
identity and stale-result rejection are proven under reordered, missing and retried
inputs. Equality is proven for this fixture; other inputs and per-unit frame bounds
remain open.

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

An in-flight building-tile reservation belongs only to its posting field.
`BuildingField::SnapshotAccepted` must copy accepted products and watermark
state but remove unaccepted reservations from the copy; it must leave the source
field untouched. `GroundWorldCandidate` takes that snapshot. Skipped tiles and
accepted out-of-order tiles remain marked. Test a reservation copied before
completion: source stays reserved, candidate can request the tile, and no
duplicate/partial product appears. Re-run Malcesine preload and paced capture;
the repaired run completes: 07ca3a25, p99 4.22 ms, 0/3422 frames over budget.
The historical image digest came from different engine inputs and is no current
pixel oracle; the new PNG still exposes implausibly vertical shore terrain.

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
Use internal contracts for equivalence and public API end to end. Add no second client.

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
   Existing cook/refinement/halo jobs preserve exact products under varied budgets.
   Shared height fields and atomic tile swaps bound structure work. Driven/generated
   ownership is explicit; shadow casting is independent. Height-page CPU payloads are
   shared and GPU restore advances 64 pages/frame. The supposed 17.83 ms publication
   cost did not reproduce: its substeps total 0.29 ms. Instrumentation instead found
   120.72 MB retained through publication, including a completed 110 MB `TerrainPressJob`.
   Release that scratch at its last use. Retire the remaining 10.52 MB/2122 Patchwork
   sheets at 64 per frame; measured retirement is at most 0.088 ms. Rosenheim remains
   8e6642f9. Refinement attribution: 100 sources, 12.904 ms for a 16-source slice,
   0.917 ms for one source, 0.207 ms for deduplication plus replacement. Eight sources
   per advance cut the longest selection to 6.346 ms and the whole phase to 7.114 ms;
   `ceil(100/8)-ceil(100/16)=6` extra advances. WI 2124 moved the remaining
   synchronous water-height stitching to workers. Residency 128->64 cut its
   longest batch 11.0->5.86 ms; native mesh 96->48 cut 14.93->10.32 ms.
   Rosenheim stays 8e6642f9: p99 5.04 ms, 0/3821 frames over 16.67 ms,
   65 extra advances against the 128/96 worker baseline.
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
- make format; focused schedule and paced-publication cases; Floor/Lattice integrations; make lint.
