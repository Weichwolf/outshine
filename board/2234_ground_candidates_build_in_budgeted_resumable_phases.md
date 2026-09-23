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

Candidate ownership, phase scheduling and atomic publication exist. Floor-contact and
paced-readiness passed at bd8693885, proving local contracts but not pacing equivalence.
Current client run 12ed7a1d7, Malcesine without vegetation, through refinement:
p50 2.15, p95 4.25, p99 34.84 ms; 36/2405 frames exceed 16.67 ms.
Simulation p99 34.08/worst 265.85 ms; draw p99 2.23/worst 11.17 ms; peak heap 852 MB.
Native macOS sample (`sample <client-pid> 20 1 -file <tmp-file>`) attributed
11676/15041 main-thread samples to repeated atmosphere integration inside structure
publication. RuntimeScene candidates now inherit the exact atmosphere cache before
Build; unchanged air/sun no longer integrate. Focused tests and full lint pass;
cache-reset mutation fails 18 checks. PNG opened; digest remains 8dd84aa7.
Unprofiled baseline was simulation p99 597.81/worst 671.65 ms. Profiling samples
attribute work but are not independent frame-time measurements.
WI 2253 removed the confirmed `FieldAwaited` worker wait from refinement/halos.
Fields are now prepared and pinned in bounded polls before either phase. Delayed,
boundary, cancellation, absence and refusal cases plus full lint pass at 86eee1580.
Malcesine is pixel-identical (0/921600); p50/p95/p99 2.08/4.10/33.53 ms, 36/2468
over 16.67 ms, sim p99/worst 32.84/192.46 ms, draw p99/worst 2.71/11.47 ms,
peak heap 848 MB. WI 2254 then fixed nested fetch admission at cap 1/2:
parked field jobs stop consuming active slots, while their fetches inherit the
parent admission. Delayed, shutdown and pool tests plus full lint pass at
02463e0c0. Malcesine remains pixel-identical; p50/p95/p99 2.06/4.30/33.73 ms,
36/2456 over budget, sim worst 188.71 ms, peak heap 848 MB. Rosenheim initially
measured 18.72 ms in `RuntimeScene::SetGeometry`: 11.33 ms shape preparation,
including 9.00 ms cluster cooking. Resumable cooks now preserve digest 8e6642f9;
the synchronous wrapper measures 16.19/9.08/7.12 ms for set/shape/clusters.
Preserve detail and the full measured refinement window.

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
   `ClusterCookJob` and `ShapeCookJob` preserve exact products under varied budgets;
   the candidate now drives them at 262144 items/advance. Rosenheim keeps digest
   8e6642f9; shape completion reshapes for 0.000 ms and the longest geometry slice is
   the remaining 11.94 ms final material/GPU bind. Split that without partial publish.
   Review through 21342822f: `PressPointsJob` now completes global rejection before
   applying any height changes, retaining cursors and decisions. Preserve that rule.
   `HaloBuildJob` replaces Rosenheim's 130.19 ms whole-set halo pass. Budgets 1/2/257
   reproduce exact output; production measures 3.09 ms. Shared immutable heights cut
   structure resolution from 39.57 to 2.00 ms; atomic tile swaps cut live transfer
   from 52.62 to 2.38 ms. Refined candidates omit pieces every tile rebakes. A
   source-paced `TerrainRefinementJob` preserves exact patches at budgets 1/2/7;
   production budget 16 cuts refinement from 69.02 to 11.46 ms, Ground worst from
   69.05 to 29.63 ms and over-budget frames from 10 to 8. Rosenheim stays 8e6642f9.
   Next split candidate begin at 29.55 ms, then geometry's observed 19.26 ms maximum.
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
