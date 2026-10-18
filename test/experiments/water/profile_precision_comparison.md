The shared water profile should retain Float64 XY and heights for canonical
geometry and contact queries until a checked lower precision recipe exists.
The fixed Ill counterexample in #760 rejects ordinary Float32 storage, including
a local XYZ datum, under the existing 1e-7 m/m directional bound. This is a
Python storage experiment; no native profile, cache, terrain or rendering changes.

`compare_profile_precision.py` assembles the previous exact area-integrated P1
objective with `area_mass`, the same complete base/source-face rows and inferred
directions. Installed OSQP 1.1.3 solves through its Python interface, without
building either native probe. These unpinned comparisons do not claim bitwise
identity with the prior A18 OSQP 1.0.0 runs. Independent primal/dual sign,
stationarity and complementarity checks must remain <=1e-6; independently
edge-reconstructed whole-face grades must remain <=1e-7 m/m. Solver status alone
does not grant acceptance. Time limit 5 s, iteration cap 100000; a solver step
can overrun the limit. Numerical solver eps is 1e-9 for 20/40 m, 1e-8 for 10 m.
The admission bounds are unchanged. Numerical settings need checking per mesh;
a tighter solver epsilon is not itself an independently checked certificate.

SIGGRAPH-first basis remains [Implicit Fairing (1999), page 2](../../../doc/references/transport/siggraph/1999-implicit-fairing.pdf),
whose equations were inspected optically. This experiment adds no smoothing or
fairing algorithm. Storage uses NumPy's documented [floating-point types and
spacing](https://numpy.org/doc/stable/reference/generated/numpy.finfo.html);
the QP uses the official [OSQP Python interface](https://osqp.org/docs/interfaces/python.html).
The precision conclusions below are measured counterexamples, not claims from
those papers or a universal rounding-error bound.

All modes retain vertex/face identities and check the original directions and
intervals on decoded coordinates. `absolute32` rounds ASL and local ENU directly;
`local32` subtracts the bounding-box centre in Float64, rounds, then reconstructs
in Float64. Mixed modes preserve the named Float64 axes exactly. `split32` stores
Float32 high/low residual pairs and reconstructs them in Float64; this does not
simulate GPU high/low arithmetic. No face or source row is omitted or repaired.

| Mesh spacing | Vertices/faces | Absolute32 maximum violation m/m | Local32 maximum violation m/m | Local32 failing base faces/source rows |
|---|---:|---:|---:|---:|
| 10 m | 2716/2716 | 8.4880e-4 | 6.1351e-7 | 14/9 |
| 20 m | 1487/1487 | 4.2394e-4 | 5.3522e-7 | 3/2 |
| 40 m | 899/899 | 1.4576e-6 | 2.9634e-5 | 3/0 |

All Float64 and split32 controls pass; no decoded faces collapse or reverse in
these three real meshes. Both mixed modes fail at 10/20 m; at 40 m only keeping
XY64 and local Z32 passes. Thus neither height-only nor XY-only rounding explains
every failure. No mesh spacing or storage variant is a general LOD/error bound.
At 20 m, local32 displaces XY by <=0.1221 mm and height by <=0.9530 micrometres,
yet still violates the directed bound. Each of three complete 1280x720 private
diagnostics was opened: the whole-body views show no discernible shape change;
the constraint maps expose the failures. This is no visible product improvement.

Twelve analytic tests also cover an oblique saturated face becoming uphill after
ASL rounding, collapse after a large XY translation, restoration by subtracting
the datum before casting, orientation reversal, separate hard pins, an extra
source direction and overflow. A synthetic shared height 500.000123 m decoded
against separate 500/400 m datums differs by 9.29693e-7 m. Independent regional
datums alone therefore do not preserve a 1e-7 m contact. Seam ownership and a
canonical shared value still need an agreed native contract. Re-adding ASL in
Float32 also discards the gain from storing local heights.

At 1487 vertices, modeled raw XYZ storage is 35688 bytes for Float64, 17844 for
absolute32, 17868 for local32 including a 24-byte Float64 origin, 23816/29764 for
the mixed modes and 35712 for split32. High/low pairs offer no storage saving
over Float64. These counts exclude topology, provenance, alignment and container
overhead; saved NPZ files contain all decoded Float64 controls, not such a codec.
Each mode converts N vertices and checks every F base face and R source-face row;
no hidden reduction of coverage is used. Conversion writes/temporaries and
NumPy/native solver allocations are not fully instrumented.

Linux VIRUS, i5-1135G7, Python 3.13.5/NumPy 2.5.4, shared host with three concurrent
single-thread comparison processes and the nice-10 test process: total traced
preparation/solve/comparison was 3536/1102/983 ms for 10/20/40 m; local32 conversion
plus complete inspection 2.45/3.27/2.71 ms. Tracemalloc peaks 10.62/5.89/3.61 MiB
exclude untraced solver/library memory and are not RSS or device/application
budgets. Work at 20 m: 8922 inequality rows, 26762 constraint nonzeros and 7435
full mass-matrix nonzeros. These single observations do not select an optimizer
or forecast native frame times; assembly and solving remain asset-build work.

Reproduction from the checkout uses an empty explicit system-temp output each
time. Required comparison-only packages are OSQP and Matplotlib, alongside the
existing transport/reference dependencies; no product dependency changes.

```sh
.venv/bin/pip install -e './lib[transport]' osqp matplotlib ruff
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  .venv/bin/python -W error -m unittest discover -s test/experiments/water
PYTHONPATH=lib/src:test/experiments/water OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  .venv/bin/python -W error test/experiments/water/compare_profile_precision.py \
  --output "$RESULT/spacing20" --spacing-m 20 --plot
```

Repeat with 40 m and a fresh output, and with 10 m plus `--solver-eps 1e-8`.
Each receipt binds the fixed input and complete saved arrays by SHA256. All
39 water tests, focused Ruff/format, and all three real comparisons terminated
successfully. A fresh NumPy-only reader independently decoded all 18 saved
coordinate arrays and solved their edge equations with `numpy.linalg.solve`;
reported violations, failing-row counts and finite-check decisions matched.
Private receipts, complete arrays, diagnostics, logs and independent-reader
result remain on VIRUS under `/tmp/outshine-research-water-precision-e1fca141b/delivery/`.
The source fixture and its provenance remain versioned under `fixtures/`.

Recommendation for #760/#48: preserve one canonical Float64 height body and
shared seam values for geometry/LevelAt/bed/shore/bridge consumers. A local
Float32 render stream may be derived separately, but still needs actual native
arithmetic, coverage, seam/contact and image checks. A bounded quantization-aware
fit could be investigated if native profiling proves this small canonical
payload matters. Relaxing the bounds, independently rounding shared contacts,
discarding thin faces or silently re-fitting each consumer is not justified.
Native integration, geographic counterexamples, terrain/region seams, cache
versions, actual Places and A18/Iris Xe budgets remain open.
