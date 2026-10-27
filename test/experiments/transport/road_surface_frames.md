# Analytic road shading reference

[#1531](https://github.com/Weichwolf/outshine/issues/1531) extends the direct
Upper reference [#964](https://github.com/Weichwolf/outshine/issues/964).
`road_surface_frames.py` evaluates surface differentials, upward normals,
orthonormal tangents and metric texture-chart coordinates directly from existing
analytic recipes. It neither reads an emitted Fine mesh nor changes the existing
position/index assembler. These are reference functions, not a native shader ABI.

## Model and units

Use the existing east/north/height convention. Let `s` be reference-line horizontal
arc length, `u` signed horizontal offset leftward, `T=(cos(theta),sin(theta),0)`,
`N=(-sin(theta),cos(theta),0)`, and `q=tan(bank)`. The existing ruled surface is
`P(s,u)=C(s)+u*(N-q*Z)`, where `C` has height `h` and plan curvature `k`.
Direct differentiation gives

```text
P_s = (1-u*k)*T + (h' - u*q')*Z
P_u = N - q*Z
q'  = bank' / cos(bank)^2
normal = normalize(P_s cross P_u)
```

Height/bank derivatives come from the original cubic Bezier controls, not sampled
display triangles. The unit tangent follows `P_s`; handedness is +1 and
`cross(normal,tangent)` follows increasing `u` after orthogonalization. This frame
does not represent texture stretch; `differentials()` retains the complete
nonorthogonal metric. Metric chart coordinates `(s,u)` remain Double. They are
not normalized UVs or intrinsic surface arc lengths; material scale/phase and
placement/port continuity belong to their future consumer. Normal/tangent outputs
are Float32. Normalization scales vectors first to avoid unnecessary overflow.

The whole-width condition `width*max_abs_curvature < 1` rejects horizontal sweep
singularities/folds before normalization, including folds between requested
stations. All 1,462 existing recipes meet it. This is not a proof against nonlocal
self-intersections or a complete physical-road test. Source boundaries explicitly
choose left/right analytic continuations; no normal averaging hides a discontinuity.
The largest actual left/right jump is about 0.00329 degrees. Existing source endpoint
position/heading disagreement remains part of the positional model qualification.

## A rejected shortcut, with the actual counterexample retained

Analytic vertex attributes are bit-identical at the same station/offset across
display LODs. That alone does **not** qualify interpolated shading between vertices.
The same real 1,462-record corpus and unchanged 0.25 m position budget expose this:

| Actual record / sampled angular error | Triangle vertex normals | Analytic vertex normals |
|---|---:|---:|
| 8329, 37.949 m road | 10.741 degrees | 11.045 degrees |
| 32370, 0.004941 m bank transition | 92.567 degrees | 91.446 degrees |

Both columns normalize barycentric interpolation over the original indexed strip
diagonal. The comparison queries 257 stations by 65 lateral offsets. These are
observed errors, not whole-domain bounds. Record 32370 changes bank by 0.03265 rad
over about 5 mm while its end bank rates vanish. Endpoint normals remain nearly
vertical although interior edge normals approach horizontal. The positional
distance criterion can accept the two-ring strip while both vertex-normal
methods miss its lighting. Do not promote the vertex-only shortcut to a selected
render path or relax its error criterion. Existing native emitted normals agree
with the analytic endpoint model within about 0.00329 degrees in this corpus;
this is not a demonstrated defect of native endpoint-normal calculation.

The reference instead accepts arbitrary in-domain `(s,u)` query grids, so a future
normal texture, shader evaluator or shading-aware sampler can be checked against
the actual source surface. A separate tiny closed-form cubic-bank control and
central differences verify the interior behavior independently of this comparison.
Physical plausibility of such short bank transitions also needs profile admission;
the renderer must not silently flatten an accepted source model to hide it.

## Checks, costs and reproduction

Nine new independent controls plus the existing eighteen surface tests cover
constant grade/bank, combined curvature/grade/bank-rate central differences,
handedness, matching LOD stations, folded width, explicit one-sided seams, the
tiny transition and finite extreme-grade normalization. No prior tolerance/pin
is changed. The complete corpus preserves its original position/index arrays.

The frame outputs contain 36,252 vertices, 1,595,088 B and three owned output
arrays per recipe (4,386 total). These are payload and minimum final allocations,
not complete copy/allocation traffic or simultaneous runtime residency. Frames
also include tangents and metric coordinates; the area-weighted normal-only
control has less output and different quality. Their diagnostic CPU times must
not be turned into an equal-quality speed claim. No new dependency is required;
NumPy and Matplotlib are already used by the existing experiments.

From the repository root, with `.venv` and one BLAS thread:

```sh
export PYTHONPATH=lib/src:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
.venv/bin/python -B -W error -m unittest test_road_surface_frames test_road_surface_lod test_road_surface_uniform test_road_surface_quadrature -q
.venv/bin/python -B -W error test/experiments/transport/compare_road_frames.py --output /tmp/outshine-road-frames
.venv/bin/python -B -W error test/experiments/transport/plot_road_frames.py /tmp/outshine-road-frames/frames.npz --output /tmp/outshine-road-frames/lighting.png
```

Choose a fresh explicit system-temp output. The saved-data figure shows Lambert
lighting, sampled angular errors and actual edge profiles; axes in the metric
charts have independent scales. Open it for visual inspection. It is not a PBR
world scene, opacity/collision/port result or either device's frame qualification.
No image is published in the repository. Native frame/shader/texture design and
angular interpolation qualification remain open; #964 is not complete.

SIGGRAPH starting point: [Chen et al. 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page 2/Figure 2, separates source graph and generated geometry. The complementary
[Galin et al. 2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
page 8/Figures 17–18, models roads with explicit cross sections along a trajectory.
Both pages were inspected visually. The derivative/frame construction above is
our differentiation of Outshine's existing analytic sweep, not a claimed algorithm
from either paper.
