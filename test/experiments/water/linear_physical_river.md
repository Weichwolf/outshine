# Derived normal bounds replace physical water norm cones (#760)

The [physical SOCP](physical_river_fit.md) supplies the same reference field/metric,
whole-prism enclosure, soft DEM priors and hard contact contract. Its 1492 normal
auxiliaries and norm cones are unnecessary when two independent tangent intervals
already bound each face's physical gradient. `normal_norm_bounds` derives a safe
bound from those **existing** rows; no new slope limit or world default is added.
The optional `primary_rows` parameter selects this linear QP. Omitting it retains
the original SOCP; the original Ill control is bit-identical to PR #843.

## Derivation and physical quality

At the cap centre u, physical horizontal gradient v is tangent to u and satisfies
`v·d_i = -n·d_i/(n·u)` for each unit tangent d_i. Two independent interval rows imply
`|v·d_i| <= b_i = max(|lower_i|,|upper_i|)`. The smaller eigenvalue of their tangent
Gram matrix is `1-|d_1·d_2|`; hence

```
B = sqrt((b_1²+b_2²)/(1-|d_1·d_2|)) >= |v|
n = (n·u)(u-v),  n.z=1
q = u.z - B |projection_tangent_u(anchor_Up)| > 0
S = sqrt(1+B²)/q >= |n|
```

The code includes explicit outward numerical allowance. Parallel/missing/wrong-face
primary rows and nonpositive q are rejected. This is a conservative bound, not a
minimum enclosing normal cone or an interval-arithmetic implementation.

Set the SOCP's norm auxiliary s=S in its linear prism inequalities and remove the
norm cone. This is not circular: their positive vertical condition and k<=1 imply
the two original **centre** grade intervals without first assuming |n|<=S. Those
centre intervals establish the derived norm bound, completing the whole-prism
argument. The result is a feasible **subset** of the original conservative SOCP,
with the same required physical grade quality. Its objective optimum need not be
identical. A separate cross-product reader checks actual normal bounds and whole-
facet grade envelopes; the original 1e-9 grade/contact checks remain unchanged.

The sparse assembly/solver code now belongs to `physical_river_program.py`; the
physical fit owns input validation, caps and geometry admission. Default-SOCP
consumers/tests migrated together. No native code or library fitter is changed.

## Original Ill comparison and decision

All methods use exactly the same 1492 vertices/faces, island, 13 original surface
bindings/1607 memberships, 121 raw course records, 4469 declared directions, soft
target and exact mass objective. The same 201105 direct physical samples per
method have zero interval violations, separately from their whole-facet checks.
Height targets remain **ENU-Up** after explicit geodetic model-height conversion;
source class/trajectory and exterior-chart restrictions remain those of #843.

| Quantity | SOCP / Clarabel | Linear QP / Clarabel | Same linear QP / OSQP |
|---|---:|---:|---:|
| Unknowns | 2984 | 1492 | 1492 |
| Constraint rows | 19382 | 13414 | 13414 |
| Stored constraint entries | 55148 | 34274 | 34274 |
| Constraint CSC bytes | 673716 | 417260 | 417260 |
| Full Hessian CSC bytes | 101460 | 95492 | 95492 |
| Requested solver-vector bytes | 178928 | 119248 | 119248 |
| Integrated soft-target ENU-Up RMS | 0.038573749 m | 0.038573923 m | 0.038573922 m |
| Final two cold constructor+solve calls | 276–365 ms | 153–207 ms | 4621–5386 ms |
| Process CPU time in those calls | 276–350 ms | 153–198 ms | 4445–5057 ms |

These are Linux/i5-1135G7 Python-library measurements in alternating cold order
SOC→QP→OSQP, then reverse; every call constructs a fresh solver. Matrix preparation
and actual geometric admission add their separately recorded wrapper time.
Compiler/Tidy jobs were active; recorded one-minute host load averages span
9.48–10.30. An earlier retained batch measured 252–257/154–158/3764–3832 ms;
the load-dependent times are not a throughput promise or a target-device gate.
Uninstrumented cold timings and Python-traced capture peaks are distinct. Native
factorisation work, allocator peaks and total copied bytes remain unmeasured.
No native solver/backend/Place/device-budget admission follows.

The derived QP changes the Clarabel result by at most **65.16 µm** from SOCP. The
two solvers on the **same bit-identical QP matrices/vectors** differ by at most
0.361511 mm in low-weight vertex degrees of freedom despite positive finite
primal/dual checks and almost identical integrated objective error. This is not
an observed-height accuracy statement. The local surface/error-map crop was opened;
its colour scales include full vertex extrema. No images are published.

Recommendation: use the derived **linear formulation** where the required primary
intervals exist. Clarabel QP is the measured host reference winner; OSQP is a valid
matrix consumer, **not a speed recommendation for this case/settings**. OSQP1.1.3
uses eps_abs=eps_rel=1e-11, cold starts, fixed 50-iteration adaptive-rho interval,
polishing and explicit iteration/time limits. One separately retained positive
global objective scaling probe (factor2984, duals converted back) reduced ~3.76 s
to ~2.90 s but still lost to Clarabel. This is not a general solver ranking or a
claim that further conditioning/native measurements cannot improve OSQP.

## Actual solver inputs and checks

`compare_linear_physical_river.py` captures the actual Clarabel constructor and
OSQP call. It checks CSC P/A and q/b bit-identical, exports the actual upper P,
A, q/lower/upper, relative-Up solution, dual and datum, then reconstructs primal,
stationarity, sign and complementarity checks using the independent existing
`inspect_solution` reader. These are finite native-consumer inputs, not a new
unversioned product cache. Add the datum to solution vertices; restore the omitted
constant target energy when comparing full objective values. The real case has
only upper inequality rows and no observed hard height pins; separate tests cover
hard contacts and rejected incompatible pins with both solvers.

Six new tests cover nonorthogonal interval bounds at four geographic anchors,
invalid/parallel bases, exact constant-normal substitution into original SOCP
rows, both QP contact paths, exhausted solver iterations and the complete original source/mesh case. Existing
seven physical tests and decoding/frame/provenance/triangulation/mass tests remain.
The first 37-case functional run hit OSQP's default 5 s budget under compiler load
and returned no geometry, correctly. That failure/log is retained. The functional
OSQP completion test now explicitly allows 10 s, as does the comparison; the
fitter's default 5 s and all native/physical quality gates remain unchanged.
The resulting 38-case suite passes; the separate one-iteration case verifies
bounded failure without publishing unvalidated geometry.
Parallel or unsupported bases require a different valid reference or an explicit
absent component; no fake norm bound or unvalidated fallback is emitted. World
partial recovery, lake/sea datum, real region/terrain/bridge/source-trajectory
contacts and native cache/renderer integration remain open.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error -m unittest test_linear_physical_river test_physical_river_fit \
  test_native_river_inputs test_geodetic_facet test_river_membership \
  test_river_surface test_profile_mass
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error test/experiments/water/compare_linear_physical_river.py \
  --input test/experiments/water/fixtures/feldkirch-ill-native-inputs.json \
  --output /tmp/outshine-research-water-linear-fit-<commit>/capture --repeats 2
```

Dependencies add installed Python OSQP1.1.3 to the previous reference packages.
SIGGRAPH-first review remains the visually inspected Kovalsky et al.2015 discussion
linked at the physical reference; this reduction is a direct Gram-matrix/normal
derivation, not that paper's iterative method. OSQP matrix/dual conventions and
reported timing distinctions follow its primary [Python interface](https://osqp.org/docs/interfaces/python.html)
and [solver description](https://osqp.org/docs/solver/index.html). The result does
not automatically approve the older affine native OSQP contract for physical water.
