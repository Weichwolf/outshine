# Weighted transport bed heights

Owner: [#265](https://github.com/Weichwolf/outshine/issues/265), under #20/#50;
native integration remains #11/#48. This is the installable quadratic method
investigated in [PR #275](https://github.com/Weichwolf/outshine/pull/275).

```python
import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget
from outshine_ref.transport.weighted_heights import fit_weighted_heights

prior = Mesh([[0, 0, 5], [20, 0, 6], [0, 20, 6]], [[0, 1, 2]])
result = fit_weighted_heights(
    prior, np.array([np.eye(2) / 0.025]),
    reference_datum_m=5.0, fixed_vertices=[0], weights=[1, 1, 1],
    budget=HeightBudget(time_limit_s=8, max_iterations=100),
)
if result.mesh is not None:
    fitted = result.mesh
```

The caller supplies an owned `Mesh` with local metric XY and absolute height
priors in its explicit local Z datum. A finite nonsingular 2×2 frame per triangle
maps that triangle's affine XY height gradient into normalized grade components;
its Euclidean norm must be at most 1+1e-7 over the whole face. Independent
longitudinal/crossfall axes can be encoded in the frame. A few edge slope checks
are insufficient. The chosen input must already be conforming/nonoverlapping in
XY; there is no hidden source preparation, topology repair or surface selection.

`reference_datum_m` is an explicit **horizontal** feasible reference, not an
inferred DEM height, arbitrary sloped reference mesh or automatic ground choice.
Each `fixed_vertices` integer ID binds its height to that common datum, not its
possibly different prior. Repeated IDs are canonicalized; other IDs/shapes fail.
To preserve an entire support footprint, the caller must fix all vertices of all
intersecting bed facets and independently verify complete coverage. Fixing one
anchor or the footprint corners alone does not prove contact. Frame/kind/source,
physical support and height-interval/plan/reservation binding stay at the caller.

`weights` contains one finite positive weight per vertex, default uniform.
The objective is one half the weighted squared vertex-prior residual. For lumped
triangle-area quadrature, the caller accumulates one third of each triangle area
at its three vertices; this is not an exact continuous DEM integral. Weights are
used as supplied. Normalizing their mean to one often gives sensible numerical
scale; global rescaling preserves the exact optimum but may affect numerical
conditioning and a timing-limited solve. Unrepresentable objective/error scales
are rejected. There is no smoothing, C1, curvature or terrain-error bound hidden
in this objective.

The result uses the existing immutable `HeightFit`: a read-only owned Float64/
Uint32 `Mesh`, or `mesh=None` and a concrete reason. Original XY, vertex order
and triangle indices are preserved exactly. A prior already satisfying whole
face limits and exact fixed contacts is returned directly, without solver work.
All other results use the same optional Clarabel dependency as the existing
minimax fit; SciPy/Clarabel imports are deferred until a solve is required.
No new package, foreign code or SDK dependency is introduced.

Candidate handling first projects fixed contacts exactly. Let M be the maximum
normalized gradient after that projection. The horizontal reference has gradient
zero, so `gradient(d + alpha*(z-d)) = alpha*gradient(z)`. With
`alpha=min(1,(1-1e-5)/M)` the candidate is conservatively contracted toward the
reference, then checked again on every complete face and contact. Geometry
acceptance remains 1+1e-7 and 1e-7 m. An additional quality check rejects candidates
with weighted RMS worse than the feasible flat reference (numerical cost
comparison allowance 1e-12 relative plus 1e-12 m). This is a valid candidate, not an
optimality certificate. A solver status alone never grants acceptance.

`HeightBudget` keeps the existing default 8 s solver limit, 100 iterations,
65536 vertices/faces; callers may explicitly choose larger input caps. Preflight
returns explained absence before large preparation when those caps or a zero
solver-time limit are reached. Preparation, validation, output ownership and
whole-face checks are outside the solver timer and included in `total_ms`;
one solver step can overrun its time limit. Worker wall/CPU/address-space bounds
remain the experiment/runtime owner's responsibility. This is offline work,
not a frame-time or native asset-cache loading contract.

Reports retain solver status/iterations, sparse dimensions/nonzeros, preparation/
solve/total observations, reference RMS, contraction and whole-face results.
Invalid inputs raise; numerical/nonfinite/invalid/worse candidates produce
explained absence. Neither path silently mutates the old surface or invents a
source omission. The previous minimax function is unchanged.

The camera experiments import this kernel, retain original source/rail products,
and replay the selected whole bed/footprint checks independently. Real input
hashes, method comparison, costs and existing fixed-camera pictures are in
`test/experiments/transport/camera_height_comparison.md`. New finite meshes are C0;
C1 ports/bank changes, tier/terrain coupling, sloped rails/turnouts, train/vehicle
routing, structural load, LOD, native integration and device acceptance remain
separate work. A raised road mesh alone is no building/vegetation ground product.
