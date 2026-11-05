# Lazy road surface LOD products

Owner: #1955 / #915. Analytic Upper-surface reference; no native port approval.
Extends the qualified position model in `road_surface_lod.py` without changing its
existing defaults, approximation formula, source recipes or stored geometry words.

`products(curve, errors_m, max_stations=..., max_queries=...)` yields strictly
coarse-to-fine products. Input curves remain immutable during iteration.

- Requesting only the first product performs no finer subdivision or allocation.
- Each next level refines the accepted frontier. Dyadic partitions remain nested;
  unchanged intervals reuse their measured bound and endpoint data.
- The generated buffers and position bounds match independent `prepare` calls
  at every requested error. No fine mesh is an input or simplification source.
- Query admission precedes pose evaluation. The caller bounds cumulative queries
  and stations; finer failure leaves already yielded coarse products valid.
- Geometry buffers are independent per level and can become independently readable
  asset payloads. This Python iterator does not implement the asset DB or residency.
- Storing every level duplicates buffer data. Generate/cache demanded levels within
  the shared DB budget; the comparison's retained validation buffers are not a runtime
  residency policy or a measured generator peak.

```sh
PYTHONPATH=lib/src python3 -B -m unittest discover \
  -s test/experiments/transport -p 'test_road_surface*.py'
PYTHONPATH=lib/src python3 -B test/experiments/transport/compare_road_surface_levels.py \
  --output /tmp/outshine-road-levels --plot-ordinal 59592
```

Use a Python environment with the existing reference dependencies; plotting additionally
uses matplotlib. Output stays outside the checkout. The driver verifies the fixed input
archive and checks that producer files do not change during the comparison. Every retained
original recipe is compared, rather than only successful candidates.

Errors 16/4/1/0.25 m are comparison inputs, not runtime distance bands or a new allowed
screen error. Camera projection and product bounds must choose actual runtime demand.
Upper boundaries do not contain final native port stitching. Normal interpolation,
material/UV behavior, junctions, bridge lower/wall/cap geometry and whole-world placement
remain separate requirements. Existing references for those contracts remain authoritative.
No native speed, full road coverage or 720p60 claim follows from this experiment.

## Native sampling step

`src/generators/road/surface/RoadDisplaySampler.h` consumes a native `ReferenceLine`
and caller-supplied station/pose budgets. It produces only ordered render stations,
position-model bounds and work counters. `Sweep` can consume those stations directly
by using the full curve length as its uniform step. No Fine ribbon is an input.
Source admission precedes qualification/evaluation; exhaustion returns an error without
a partial plan. Screen projection, cache keys/LOD metadata and World selection remain
consumer responsibilities.

Native spirals use Gauss8, whereas the Python reconstruction uses Gauss16.
`ReferenceLine::PlanarTruncationBoundM` therefore uses the native rule:
`C8 = (8!)^4 / (17 * (16!)^3)`, rounded upward, with dimensionless majorants
`a = max|curvature| * L`, `b = |curvature_rate| * L^2`,
`B0=1`, `B1=a`, `B(m+1)=a*Bm+m*b*B(m-1)`, remainder `L*C8*B16`.
Every query pays for integration from its original stored segment entry, including
a short queried tail of a long segment. Closed arcs/straight segments use no quadrature.
Node/weight values and native integration operations remain unchanged.

The sampler combines per-source-span second derivatives of height/bank/offset curves,
seam residuals, triangle twist, stored Float32 endpoint rounding and twice the local
source quadrature truncation allowance. It rejects offset folds, tangent poles,
unbounded derivatives and inadequate source accuracy before render subdivision.
The result is an analytic position-model bound, not a universal libm/IEEE certificate:
accumulated source pose, coefficient/evaluation FP error, later world transforms and
final port movement remain unqualified. Normal interpolation and complete bridge bodies
remain unqualified too. This module does not yet authorize World replacement or a lower
runtime LOD error. Native controls independently inspect actual emitted triangle
interpolation and exact quadratic chord error; a captured-corpus/native comparison is
still required before integration.
