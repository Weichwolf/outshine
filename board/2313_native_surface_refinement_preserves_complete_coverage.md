Type: feature
State: active
Architecture: ready
Parent: 2312
Depends:
Priority: P0
Area: base, generators
Tags: geometry, proof, lod, bounded

# Native surface refinement preserves complete coverage

## Problem and measured failure

Native vertex distances miss a filled opening: a 3x3 m cap over a 1x1 m opening
has zero corner error but Hausdorff distance 0.5 m. Complete interiors need proof.
At 6e88154b5, Graz Fine/Shell pairs have 4884/4486, 13207/12293 and 19083/17829
triangles. Flat initialization exceeds 4096 regions, performs zero queries and retains
310.998/157.921/231.442 m coarse bounds. Single-building utility did not generalize.
Log: /tmp/outshine-native-pair-6e88154b5-Graz.log. Do not increase caps to hide this.

## Owners and immutable inputs

- generators/building/StructureSurfaceRefinement owns resumable comparison composed
  with StructureSurfaceError. Inputs are immutable scoped Raised views in one frame,
  from one pinned source. Validate every native stream before publishing any bound.
- StructureSurfaceIndex owns incremental hierarchy and matching scratch over original
  indexed wall/roof vertices. Existing ray TriangleBvh reconstructs float edges and
  builds synchronously; that contract cannot certify original native coordinates.
- Keep 65536 triangles/input, 131072 queries, 4096 live regions and 0.02 m target width.
  Count construction/traversal/matching in WorkUnits and retained capacities in bytes.
  Every Step is resumable; cancellation/source changes revoke every certificate.
- Copy preserves independent progress. Moves preserve all cursors/heaps/views and revoke
  the source; reset and self-move remain valid. Inputs outlive every task copy.

## Complete hierarchy and target search

- Build deterministic balanced topology incrementally in original triangle order;
  fold exact stored-vertex AABBs outward. Each source node covers every descendant.
- Evaluate every original leaf before dense adaptive refinement; fold leaf upper
  certificates upward. Until both roots finish, publish the existing coarse envelope.
  This prevents one detailed facade consuming the budget before other roofs are seen.
- Two largest-upper heaps cover both directions. Split nodes into two descendants,
  triangle regions into four enclosed children. Keep the parent until ALL children
  finish; replace atomically. Fully settled regions retain their maximum certificate.
- Best-first target search orders nodes by outward point/AABB lower distance. Prune
  only when no descendant can lower the complete sample minimum or improve its witness.
  Node traversal, hash probes and each triangle query consume bounded work units.
- Canonical coordinate hashes are hints. Full equality of ALL source corners with
  corners of ONE target and zero enclosure radii proves convex containment/zero error.
  Cap each match/insertion at 64 probes; collisions fall back to geometric proof.
- Complete sample minimum minus sample deviation rounded down is a lower bound.
  Partial minima are never lower evidence. Sample distance plus region radius rounded
  outward is a whole-region upper. Early settled uppers publish sample lower zero.
- Pick ONE target witness from the nearest-sample search. Max of its three enclosed
  corner-distance uppers is another whole-region upper by convexity. Never switch
  targets per corner. Stop corners when their partial max cannot improve the prior upper.
- Exhaustion retains complete coverage, including during seeding/partial children.
  Nonfinite arithmetic fails closed. No runtime LOD grant follows from CPU proof alone.

## Falsifiable acceptance

- Opening, planes, slopes, translations, collapsed triangles, reverse directions/order
  and native flat/pitched self pairs contain independent analytic truth at every bound.
- 48x48 grids exceed flat seed capacity. Distances 0/2 m and isolated 5 m peak remain
  enclosed and reach <=0.02 m width under unchanged caps in either triangle order.
- One-unit/arbitrary slices, cap exhaustion, cancellation, stale keys, copy/move/reset
  cannot expose partial or obsolete coverage. Zero target width completes exact self.
- Forced hash collisions must remain correct. False coordinate match, omitted descendant,
  inward box, nearer-target pruning and live moved-from task must actually FAIL.
- make format; StructureSurfaceIndex/Refinement/Error and TriangleDistance/Region suites;
  full make lint including tidy/API; all Places via client and personally opened PNGs.

## Current measured implementation

Dense hierarchy, bounded coordinate matching and nearest-witness queries implemented.
Three Graz cells (35/67/120 structures), same raw source/height field/anchor, normal caps:

| Pair | Upper m | Width m | Queries | WorkUnits | Scratch bytes |
| --- | --- | --- | --- | --- | --- |
| Fine/Shell 35 | 5.39355547 | 0.01041373 | 5425 | 273511 | 2813376 |
| Fine/Shell 67 | 4.19171936 | 0.01342433 | 10115 | 675793 | 5019312 |
| Fine/Shell 120 | 4.09557385 | 0.01240434 | 11447 | 1070814 | 7257456 |
| Fine/Massed 35 | 64.3713712 | 0.0181950 | 7593 | 190853 | 2127232 |
| Fine/Massed 67 | 66.0309054 | 0.0169132 | 15973 | 465656 | 3320400 |
| Fine/Massed 120 | 63.0093669 | 0.0133916 | 22713 | 812135 | 4512912 |

Widths = upper minus lower. Temporary probes use frozen 6e runtime plus explicit new
proof sources; they are not production integration or a green Place gate. Single host
proof durations: Shell 36.7/83.8/126.2 ms, Massed 31.3/51.7/82.0 ms; no p99/device claim.
Logs: /tmp/outshine-native-witness-{shell,massed}-Graz.log. At 128 work units/post,
ceil(1070814/128)=8366 posts for one cell: do not couple one tiny slice to each frame.
2312 owns measured worker batching/admission and product transfer; 2298 owns selection.

The move fixture's old >1000-step minimum wrongly penalized the faster algorithm.
Replace only that specification with one transfer per actual work unit, nonzero queries
and splits; retain every intermediate state/bound, revocation/reset and self-move check.
Nine focused tests PASS; format 1213 files, zero errors. Forced collisions remain safe;
false match, omitted descendant, inward box and omitted move cancellation actually FAIL.
Logs: /tmp/outshine-surface-hierarchy-final-{format,focused}.log and
/tmp/outshine-surface-hierarchy-control-results.log. Full lint/Places are not green;
ASan/UBSan dense and move fixtures PASS:
/tmp/outshine-surface-hierarchy-sanitized-results.log. Runtime integration remains open.

0d5962a2a full tidy found 16 issues: direct includes/initializers, duplicate hash
constants, two complex functions and a side-effecting condition. Fix by using Digest
constants, explicit defaults and named seed/fold/search/sample steps; keep proof/work
ordering. Format 1213 PASS. Focused/full gates for this correction are pending.
0d5962a2a: full lint exits 2, 32 rules PASS, tidy is the sole red guard;
/tmp/outshine-repair-0d5962a2a-full-lint.log links the actual diagnostic report.
bec05b615 correction in 36e376880: eleven focused tests PASS, all seven control runs
match expected exits. Full lint/Places pending. /tmp/outshine-repair-36e376880-focused.log;
/tmp/outshine-surface-hierarchy-36e376880-controls.log.

36e376880 tidy finishes 257/257 with one remaining finding: adjacent sample-distance
parameters. Pass the existing TriangleDistanceBound instead; no arithmetic/work changes.
c8c670ea9 passes the existing bound as one value. Format 1213 and ten focused tests PASS;
targeted Declaring/SurfaceRefinement tidy has zero user findings. Full 4b54277f2 gate pending.
