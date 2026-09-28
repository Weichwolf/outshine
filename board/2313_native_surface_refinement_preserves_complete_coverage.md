Type: feature
State: active
Architecture: ready
Parent: 2312
Depends:
Priority: P0
Area: base, generators
Tags: geometry, proof, lod, bounded

# Native surface refinement preserves complete coverage

## Problem and existing capability

2312 supplies outward point/triangle distances, enclosed triangle regions and a
resumable native coarse envelope. Vertex-only error misses the filled opening:
its Hausdorff distance is 0.5 m despite coincident source vertices. Coarse envelopes
alone are too loose. Adaptive refinement must cover every wall/roof interior,
remain bounded and resumable, and preserve a complete certificate on exhaustion.

## Ownership and dataflow

- generators/building/StructureSurfaceRefinement.h/.cpp owns a worker-side task
  composed with StructureSurfaceErrorTask. Preserve the existing coarse contract.
- Inputs: immutable scoped Raised pair, pinned source key, common frame. Views never
  outlive the pinned meshes. Validate all native streams before publishing any bound.
- Two deterministic bounded heaps own source regions, one per direction. Each region
  holds three PointEnclosures, interior sample, radius, upper bound and serial tie.
- Largest-upper regions split into four enclosed children. Retain the parent's
  certificate until all four children are complete; publish replacement atomically.
  Children may inherit parent upper bounds. Every exposed result covers both surfaces.
- Limits: 65536 triangles/input, 131072 distance queries, 4096 live regions,
  target interval width 0.02 m [SET]. WorkUnits bound validation, queries and matching.
  Reusable scratch, capacities, query count and splits are measured. No frame consumer.
- Insufficient initialization capacity or zero query budget retains coarse coverage.
  Exhaustion retains the last complete upper/lower interval; nonfinite arithmetic fails
  closed. Source changes/cancellation revoke results. Copy is independent; moves revoke
  the source, preserve partial state, and allow reset. Self-Move preserves progress.

## Conservative bounds and bounded target work

- Each sample lower distance requires minima over ALL target triangles, minus its
  enclosure deviation rounded down. Samples alone never certify interior coverage.
- Sample upper plus region radius, rounded outward, is one valid whole-region upper.
- Another upper uses ONE fixed convex target: max of the three enclosed corner-distance
  uppers, including outward-added corner radii. For p=sum(lambda_i*p_i), choose q_i in
  that target; q=sum(lambda_i*q_i) stays inside it, so distance(p,q) <= max_i U_i.
  Minimize these maxima across targets and combine with sample/parent bounds.
  Per-corner minima from DIFFERENT targets are unsafe: the filled opening disproves them.
- EvaluatePoint counts the interior and three corner queries separately. Target changes
  reset the maximum. Moves transfer the partial cursor/maximum. Query caps stay unchanged.
- Once the partial corner maximum reaches the current region upper, remaining corners
  cannot improve the computed maximum; skip them. Keep the target's interior sample for
  complete lower minima. Never publish a smaller partial maximum as whole-region proof.
- Seed checks the same-index target as a hint, then the regular scan checks other targets.
  Zero enclosure radii plus full equality of ALL source corners with corners of ONE target
  prove convex containment and directed error zero, including degenerate triangles.
  Complete that region with lower zero. Indices/hashes alone never prove correspondence.
  Matching is bounded work, not a distance query. Nonmatching inputs use the regular proof.

## Falsifiable acceptance

- Filled opening contains analytic 0.5 m at every certificate and resolves width <=0.02 m.
  Reverse directions/order; differently tessellated, parallel, sloped, collapsed and
  translated surfaces preserve safety. Native flat/pitched self pairs remain useful
  under caps even after triangle-order reversal. No place/vertex-only oracle.
- One-unit/arbitrary slices, exhausted seeds/partial children, cancellation, stale keys,
  copy/move/reset and invalid input cannot expose partial or obsolete coverage.
- Wrong corner min, false coordinate correspondence, wrong pruning, omitted radius,
  midpoint deviation, all-target lower minimum or source guard must cause actual FAIL.
  Analytical/rational controls supplement regressions and native utility measurements.
- make format; make suite SUITE=outshine/src/generators/building/StructureSurfaceRefinement;
  existing TriangleDistance/TriangleRegion/StructureSurfaceError suites; full make lint.
  Measure query counts, time and actual scratch before choosing runtime caps.

## Evidence and remaining work

9bde2cd88: seven official focused tests and full lint PASS, 256/256 tidy-Units without
findings, 32/32 guards. Logs: /tmp/outshine-convex-bound-{focused,full-lint}.log;
gate-result.txt records the checked commit. ef78343aa previously proved explicit moves
against the reproduced SEGV, implicit-move mutant, ASan/UBSan and full gates.

Current optimization: expanded native test fails against 9bde2cd88 and passes afterward:
16 checks cover flat/pitched and original/reversed orders. Existing adaptive 55, move
3003 and analytical convex 456 checks pass ASan/UBSan. False match (X only), corner min
and inverted pruning each cause actual FAIL, not BUILD. Logs:
/tmp/outshine-corner-coverage-{controls,sanitized,pruning-control}.log and per-case logs.
Format: 1201 files, zero errors. Official focused/full gates for this change remain pending.

Local 20x30 m native probe: flat Fine/Shell uses 16052 queries for
[1.899999979,1.911157473] m; pitched Fine/Shell 47541 for [3.447908072,3.465112777] m.
Their widths are respectively 0.011157494/0.017204705 m, below 0.02 m. Exact self pairs
use zero distance queries after complete value matching. Six single runs take 0.27–98 ms;
no p99/device claim. Log: /tmp/outshine-corner-coverage-probe.log.
Scratch remains two retained 4096-slot heaps plus five Regions, each 152 bytes:
(8192+5)*152 = 1245944 bytes/task despite the 4096 LIVE cap. Runtime utility still needs
scratch/concurrency budgets, broader native profiles and independent image acceptance.

Independent Fraction oracle: 7200 rational interior probes cover arbitrary/sloped,
degenerate/self targets, scaling and large translations. Fixed-target inequality holds;
per-corner target switching gives upper zero versus opening squared distance 1/4.
Log: /tmp/outshine-review-convex-oracle.log. This alone does not prove C++ rounding/runtime.
2312 owns StructureBuildTask/product integration. This CPU work grants no smaller runtime
LOD error; adaptive publication, source lifetime and independent visual acceptance remain open.
