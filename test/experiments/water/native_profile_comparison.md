# Native water-profile comparison (#760)

Standalone C++ probes compare installed HiGHS 1.15.1 and OSQP 1.0.0 on the fixed
Ill fixture. This adds no production dependency or runtime water/terrain change.
Input geometry, provenance and the directed feasible set remain those of
[the Python comparison](weighted_river_comparison.md). Complete gradient matrices
and bounds are compared exactly against its independently assembled constraints.
Flow vectors use the same explicit normalization, including floating-point order.

## Objective and counterexample

Minimax retains the LP control. Uniform and lumped-area vertex least squares are
controls for an exact area-integrated P1 height-prior objective. On each triangle,
`M = area/12 * [[2,1,1],[1,2,1],[1,1,2]]`; summing these blocks gives
`integral(error²) = error.T * M * error`. The solver scales M to mean diagonal 1;
this changes neither the minimizer nor the separately reported integrated RMS.

For a unit square triangulated along a diagonal, prior `z=x`, and nonincreasing
X with cross-grade bound .005, lumped weights manufacture a Y slope: heights
`.5025,.5025,.4975,.4975`. Exact integration produces constant `.5`, without that
artificial bias. Analytic triangle/square integrals and subdivision invariance are
three independent tests. This is a quadrature counterexample, not a universal
claim that every lumped method produces visible error.

SIGGRAPH-first: [Implicit Fairing of Irregular Meshes (1999)](../../../doc/references/transport/siggraph/1999-implicit-fairing.pdf),
page 2 equations inspected. It motivates considering continuous integrals
and sparse surface objectives; this fit is not its curvature-flow algorithm.
Native calls follow the installed public HiGHS and OSQP headers/CMake targets.
No foreign source is bundled. HiGHS reads standard MPS for both probes; its sparse
transpose converts the lower Hessian to OSQP upper CSC. Already-compressed input
columns are copied once with identity rows for bounds; no triplet reconstruction.
A production OSQP integration should build CSC directly and need neither MPS nor
HiGHS merely to parse it. All assembly and solving belongs to asynchronous asset
creation; cache hits must not repeat them.

## Measured finite case

Apple A18 Pro, macOS, normal priority/shared host; serial native solvers. The same
20 m boundary refinement gives 1487 vertices/faces, 8922 gradient rows and 26762
constraint nonzeros. These are laboratory choices, not terrain/LOD tolerances.
All positive source/face overlaps and separate hard pins remain enforced.
Three fresh processes per method, alternating order; source read, triangulation
and Python MPS export excluded from these native solve times. RSS is per process,
including model parsing/libraries; it is not application memory or an allocation count.

| Method | Median solve ms | Peak process RSS MiB | Integrated RMS m | Max vertex error m |
|---|---:|---:|---:|---:|
| HiGHS minimax | 47.193 | 21.47 | .338281453 | .607062722 |
| HiGHS exact area QP | 1010.845 | 75.53 | .038433900 | .774724981 |
| OSQP exact area QP | 154.442 | 13.11 | .038433888 | .774724981 |

For the same area QP, native solve ratio is 1010.845/154.442 = 6.55. This is a
backend comparison, not a Python-to-C++ speedup: SciPy's LP already calls HiGHS.
Integrated RMS versus minimax drops .338281453/.038433888 = 8.80 times while the
maximum error increases. Neither metric alone proves better native appearance.
Each method's three fresh-process height arrays hash identically.

The complete single OSQP trial (including Python matrix assembly/MPS export,
subprocess, and independent checking) took 209.19 ms; native setup 19.67 ms and
solve 159.01 ms. Its 10409 working rows include 1487 identity bound rows;
A has 28249 nonzeros and the upper Hessian 4461. Separate rebuild/check costs
must not be hidden by quoting solver time alone. Factorization/time-limit steps
can overrun their nominal limit; no hard realtime guarantee is claimed.

## Validation and reproduction

27 ordinary Python tests and 38 native analytic cases pass. The latter cover
both solvers, minimax/uniform/exact objectives, incompatible distinct pins,
zero time/iterations, multiple directions per face and the retained lumped bias.
All seven real native objective/backend combinations pass independent primal,
dual-sign, stationarity/complementarity, pin and edge-reconstructed face checks.
Declared experimental tolerances: KKT residuals 1e-6 in the mean-diagonal-scaled
problem, height and grade violations 1e-7. OSQP exact case: maximum grade violation
5.55e-8 m/m, stationarity 7.71e-7, complementarity 4.91e-7. These are not changed
library/native admission tolerances. HiGHS/OSQP integer ABIs may differ; sparse
indices are converted explicitly after size/range checks. Invalid models do not
produce an admitted profile. Both C++ units pass the repository clang-tidy rules
with their complete standalone compile commands; format and focused Ruff pass.

```sh
brew install highs osqp
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$PY" \
  test/experiments/water/native_profile_compare.py --output "$RESULT"
PYTHONPATH=lib/src "$PY" -W error -m unittest discover -s test/experiments/water
```

Use a Python environment containing the reference dependencies and an explicit
system-temp RESULT. Linux needs installed HiGHS/OSQP CMake packages and HiGHS
pkg-config metadata; no distro package availability is asserted. Private receipt,
MPS, native arrays and repeated costs are in
`/tmp/outshine-water-fit-d252209ee/reproduced/` on the A18 host; build flags are in
its compile_commands.json. The output command saves all inputs/results anew.

Still open: source/body admission and lake/sea separation, genuine region seams,
Float32/camera-relative conversion, one shared native profile for geometry,
queries, bed/shore/bridge contacts, codec/versioned asset integration, and actual
Place images/costs. Native water remains flat-fragmented until that integration.
