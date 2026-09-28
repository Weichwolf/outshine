Type: feature
State: active
Architecture: ready
Parent: 2298
Depends:
Priority: P0
Area: base, generators, engine
Tags: geometry, lod, proof, bounded

# Structure variants carry conservative surface-error bounds

## Problem and evidence

RequestedStructureCellDetail uses the whole cell envelope as error. Shell now retains
core roof/footprint/foundation surfaces; its independent geometry tests pass. No
smaller error is proved yet. Do not lower the existing bound using triangle counts,
vertex samples, guessed facade depth or a camera-dependent bake.

Analytical control: a 3x3 m roof cap fills the 1x1 m central opening of a roof frame.
Every cap corner lies on the frame: vertex-only error is zero, but the directed
surface error is exactly 0.5 m at the opening centre. The other direction is zero.
The standalone prototype /tmp/outshine-surface-bound-prototype.py yields an upper
bound 2.236068 m initially, 0.559017 after 32 subdivisions, 0.500029 after 128.
This validates the mathematical approach on an analytic case, not the engine code.

## Ownership and implementation

- base/spatial owns allocation-free double point/segment/triangle queries. Add a
  narrow TriangleDistance.h/.cpp if no equivalent exists; TriangleBvh currently
  supplies rays/heights, not nearest-surface queries. Reuse Vec3 and native math.
  No importer types or second authoritative geometry representation.
- generators/building owns StructureSurfaceError.h/.cpp. Read native Raised wall/
  roof streams and indices through scoped views; compare explicit Fine reference
  with Shell/Massed from the identical pinned source snapshot and common frame.
  Query/subdivision scratch is worker-owned, bounded and reusable. No frame queries.
- Compare both directed surface distances; the maximum bounds Hausdorff distance.
  For each source triangle choose its centroid c and radius r=max(|vertex-c|).
  Distance to a target surface is 1-Lipschitz, so every point in the triangle has
  distance <= distance(c,target)+r. An upper estimate of centroid distance is safe;
  merely sampling source vertices or centroids without the radius is not.
- Refine the largest upper-bound triangle deterministically into four children.
  Replacing a triangle keeps its complete coverage. A bounded target search may
  overestimate distance using an actual target point, never underestimate it.
  Translate both inputs by the same origin. Enclose relevant arithmetic outward,
  including barycentric weights, denominator and norm; uncertain face projections
  may fall back to target edges/vertices. A final nextafter alone is not a proof.
  Unrepresentable arithmetic/invalid indices/nonfinite inputs produce unknown/error.
- Initial experiment budgets [SET]: 65536 triangles per input, 131072 target-triangle
  evaluations, 4096 live subdivision nodes, 0.02 m target uncertainty. These are
  work/memory caps, not a frame-time promise; measure and revise before runtime use.
  Comparison is a resumable StructureBuildTask phase; slices observe cancellation
  and source generation. Never publish proof computed for another source snapshot.
  When work expires return a still-valid coarse upper bound if full input coverage
  was established; otherwise unknown. Empty/nonempty mismatch is never zero error.
- BakedTile owns the optional bound for its explicit detail and source key. Bounds
  transfer with staged products into TilePieces; do not put visual LOD metadata in
  authoritative footprint/navigation semantics. Generator owns computation, renderer
  owns resident selection, streaming owns bounded requests and product replacement.
- Unknown bounds retain the existing whole-cell guard. A finite proved bound may
  tighten projection/hysteresis only after independent controls pass. Account for
  instance scale conservatively; no optimistic Refined state on missing/failed proof.
  Material/lighting differences are outside this geometric certificate.

## Primitive contract and evidence

BoundPointTriangleDistance encloses nearest distance between LowerDistanceM and
UpperDistanceM. TargetPointEstimateM is rounded output, not an exact surface point.
Upper proof uses nested convex interpolation with parameters in [0,1] and outward
arithmetic after every operation. Face/edge conditioning only chooses parameters.
Lower proof uses max(0,n.dot(p)-max(n.dot(targetVertex)))/norm(n), any direction n;
enclose numerator down and denominator up. Cauchy-Schwarz bounds every target point.
The rounded estimate chooses n only. Union minima require ALL target triangles.
Collapsed targets stay valid; nonfinite/unrepresentable upper bounds stay unknown.
Current independent oracle: 6000 queries, Decimal-160, all vertex permutations,
binary scales -100/0/100, collapsed/nearly collinear targets; no invalid interval.
/tmp/outshine-2312-interval-oracle.py and .json preserve the independent experiment.
Upper-as-lower and last-vertex-as-support mutations cause actual test FAIL, not BUILD.
/tmp/outshine-2312-lower-negative-*.log. Primitive (47 checks) and native-envelope
(48 checks) suites plus full lint pass; clang-tidy zero. Git activation was pending in the previous environment.
/tmp/outshine-2312-lower-{restored-suites,full-lint}.log.
Adaptive dual prototype with engine queries encloses 0.5 m by [0.4921875,0.5096568] m:
86 splits, 268 nodes, 3270 primitive queries; .py/.json under /tmp/outshine-2312-adaptive-dual-prototype.
Not native-task integration. Enclose child vertices and interior samples numerically;
rounded midpoints alone cannot prove coverage. Include vertex deviation in radius and
subtract sample deviation from the lower distance. Radius alone does not prove uncertainty.

Mesh-envelope slice: immutable scoped Raised pair from one pinned source/common frame;
Reset performs only bounded header checks, Step consumes at most its corner-work budget.
First validate both complete index runs, then bound each referenced source vertex against
one valid target point in both directions. Convexity of the norm encloses every triangle
interior; this is a coarse upper certificate, not adaptive precision or runtime LOD proof.
Source-key mismatch and cancellation revoke completion. No partial validation certificate.
Shared Vec3 arithmetic replaces duplicate dot/subtraction. Native envelope suite passes;
forcing zero bounds and allowing stale-key reads each cause actual FAIL, not BUILD.
Adaptive refinement and StructureBuildTask/product integration remain required; do not
tighten selection yet. Logs: /tmp/outshine-2312-envelope-{final-suite,negative-*}.log.

## Independent acceptance and error flow

- Point/segment/triangle: analytic face, edge, vertex, collapsed and collinear cases;
  translation/rotation/scale variants and nonfinite/overflow refusal. Verify returned
  target points are on the target primitive; no oracle calling the same query.
- Surface bound: separated parallel planes, identical differently tessellated planes,
  filled opening with zero vertex error, pitched roof replaced by flat cap, inverted
  input direction, reversed traversal order and degenerate triangles. Check analytical
  distances never exceed the reported upper bound, including exhausted budgets.
- Native BuildingMesh pairs cover Shell/Massed, concavity, roofs, sloped foundation,
  very long walls and different anchors. Validate error/source/detail ownership and
  budget counters; copy/move and staged replacement cannot lose certification.
- Negative controls: force zero error on the filled opening; remove the radius term;
  reuse a bound after source change; force Massed near the camera. Each must fail.
- make format; affected base/spatial, BuildingMesh, StructureBake, TilePieces and
  StructureCellDetail suites; full make lint including clang-tidy/API documentation.
  After integration render paced/static Hockenheim through outshine-client, open PNGs,
  compare geometry and report p50/p95/p99 and memory. SDL/Metal render availability is
  currently an environment limitation; CPU evidence cannot close visual acceptance.

Git is writable; activation and implementation commits are being recovered separately.
