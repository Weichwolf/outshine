# Weighted directional water-height comparison (#760)

The real Ill fixture now has a weighted quadratic reference alongside the existing
HiGHS minimax LP. Both use exactly the same whole-face/source-face gradient
inequalities and separate hard-height equations. Their objectives differ: LP
minimizes the largest vertex error; QP minimizes positive weighted squared vertex
error. The comparison changes neither the existing library fitter nor native water.

## Inputs and procedure

Fixed original `fixtures/feldkirch-ill-profile.npz`, SHA256
`9c925970942c835b4af1ddef540afaff46b22117d2d33e1f7453b6737e7236f5`;
its unmodified provenance JSON SHA256 is
`e8d85a46526456dbbdfc155f0984c06c341f051eb2516e6039f08aaa5e7bd5d6`.
The current native observation supplies nine shared source bindings, exact uint64
provider-ID strings, 13 river surfaces and 17 courses (13 river, 3 stream, 1 canal).
Old historical admission/epoch limitations remain distinguishable from this added
observation. No assured original OSM flow direction is supplied. The selected
body, original rings/island, mesh construction and continuous height prior are
unchanged. No terrain/provider/native implementation is added.

SIGGRAPH-first: Desbrun et al., *Implicit Fairing of Irregular Meshes* (1999),
[local paper](../../../doc/references/transport/siggraph/1999-implicit-fairing.pdf),
SHA256 `dd00a080220cef32625d136db861cd149a4e8a44544ae9223612b459f08e6ecb`;
introduction and page 2 equations were read and visually inspected.
This motivates examining constrained sparse surface objectives; this experiment
is a height-prior fit, not that paper's curvature-flow/fairing algorithm.
The existing weighted fitter supplies the positive diagonal objective model;
[Clarabel's official formulation](https://clarabel.org/stable/python/getting_started_py/)
supplies the standard QP/linear-cone interface. No foreign code is copied.

`fit_weighted_surface` retains shared height vertices and the same limits
`-0.1 <= gradient · flow <= 0`, `abs(gradient · cross) <= 0.005` and every additional
`[face, dx, dy, lower, upper]` row, including repeated face IDs. These numbers are
laboratory assumptions, not supplied OSM widths/grades. Directions are normalized;
weights are positive and normalized. Default comparison weights are incident XY
triangle area divided among their vertices; uniform weights are a separate control.
Hard pins map individual vertex IDs to individual ASL heights. They are not forced
to share the reference datum. The optional real-mesh pin control takes the minimum
and maximum heights of a valid LP result: explicit synthetic contacts, not observed
seams/water levels. Their values are 431.437336220201 and 480.4295351725916 m.

At actual LP and QP solver calls the complete gradient matrices/bounds and hard
pin equations are compared exactly. The LP error/t column is excluded from that
comparison; the QP has a diagonal Hessian. Independent edge-equation gradients,
all additional source-face rows, distinct pins and the QP dual/stationarity/gap
certificate decide validity. Solver completion alone does not. Input caps remain
8192 vertices/16384 faces/32768 directed rows, 200 QP iterations and 2 seconds;
zero budget or incompatible pins return explicit absence, with no relaxed pins.

The 10 m mesh exposed a useful countercase: solver `Solved` at relative gap 1e-9
still missed the independent absolute 1e-7 certificate (gap 1.039e-7). Tightening
solver feasibility/absolute/relative gap targets to 1e-11 passes without increasing
budgets or acceptance tolerances. This actual case is retained as a test.
The fresh reader reproduces objective normalization exactly; it does not compare
an already-normalized caller vector with the solver's second normalization using
a tolerance. The exact serialized Hessian/linear vector are the native reference.

## Results and limits

| Same body/prior and directed feasible set | LP weighted RMS m | QP weighted RMS m | LP maximum error m | QP maximum error m |
|---|---:|---:|---:|---:|
| 10 m, area weights, 2716 vertices/faces | .349292638 | .063321567 | .642632312 | .792287994 |
| 20 m, area weights, 1487 vertices/faces | .354016418 | .062448684 | .607062722 | .779381540 |
| 40 m, area weights, 899 vertices/faces | .368897344 | .065047611 | .524008834 | .846953198 |
| 20 m, same two distinct pins | .366993535 | .066911173 | .607062722 | .779381546 |
| 20 m, uniform weights | .361856777 | .056092665 | .607062722 | .780141473 |

The 20 m area-weighted RMS drops 82.36%, while its worst vertex error increases.
Neither objective automatically establishes better water appearance or physical
truth. All 99 directed source segments are sampled at 65 points each: 6435 points,
149 outside the body, 6188 covered adjacent pairs and zero rises above 1e-7 m.
Outside samples are not bridged into an artificial gradient check. The 21 nearly
flat source segments retain unknown direction. Source-face rows additionally
cover every positive-length face crossing; fixture segment indices are not
provider feature IDs. Outside lengths/parts remain explicit in each report.
The existing 20 m domain coverage is 90.591 m outside 12333.576 m source length.
No global use rate, native float conversion, region contact or terrain certificate.

The whole 1280×720 3D diagnostic was generated and opened. All three panels use
the same camera, axes and height colours, with declared Z×60 exaggeration. The
body's kilometre-scale shape is preserved; sub-metre differences are small in this
whole view. This is a geometry diagnostic, not an engine screenshot or proof that
native flat-water steps are fixed. No images are published.

## Costs and reproduction

Linux/Debian 13, i5-1135G7 4 cores/8 allowed CPUs, 32 GB RAM; Python 3.13.5,
NumPy 2.5.4, SciPy 1.18.1, Clarabel 0.11.1, Shapely 2.2.0/GEOS 3.14.1.
Three fresh normal-priority processes per method, alternating order, shared host,
BLAS/OMP one thread. Whole fit setup/solve/validation included, import/source
read/triangulation excluded. Different objectives and backends: these costs are
not a solver-independent speed comparison. Solver limits can overrun within a step.

| 20 m area case | LP | QP |
|---|---:|---:|
| Whole fit median ms | 159.874 | 80.660 |
| Rows / nonzeros, excluding Hessian | 11896 / 32710 | 8922 / 26762 |
| Sparse constraint payload B | 440108 CSR | 327096 CSC |
| Hessian diagonal entries | 0 | 1487 |
| Maximum fresh process RSS B | 119779328 | 109359104 |
| Separate Memray allocation events | 278898 | 123734 |
| Separate cumulative requested B | 70483784 | 18581851 |
| Separate tracked heap peak B | 24336114 | 7153945 |

Both successful paths explicitly copy 89220 mesh bytes: one working vertex copy,
then the shared owned Mesh's vertex and uint32 face copies. Other internal copies
are unobserved. Allocation requests include repeats and reallocations, not unique,
live or copied bytes. Profiled heap excludes preexisting inputs; RSS includes them.
No GPU/A18/native budget claim. Each method's three ordinary output hashes match.

```sh
export TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
export PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport
"$PY" -W error -m unittest discover -s test/experiments/water
"$PY" -m weighted_river_compare --input "$FIXTURE" --output "$RESULT"
"$PY" -m verify_weighted_river "$RESULT" --input "$FIXTURE" --output "$RECEIPT"
"$PY" -m weighted_river_display "$RESULT" --input "$FIXTURE" --output "$IMAGE"
"$PY" -m weighted_river_cost --input "$FIXTURE" --method weighted --output "$COST"
```

`FIXTURE` is the fixed NPZ above; all fresh outputs must explicitly use `/tmp`.
Use `--spacing-m 10|20|40`, `--weights area|uniform`, optional `--pins` on the paired
comparison. Cost alternatives are `--method minimax|weighted`; allocation captures
run separately with `--profile`. Comparison/cost CLIs also cap CPU 120 s and address
space 4 GiB. Installed replaces `lib/src` with the actual wheel installation.
24 source and 24 Installed water tests pass. Seven real cases pass fresh readers;
Source/Installed no-pin and two-pin cases have exactly equal complete NPZ products.
After updating artifact hashes, corrupted dual data and corrupted serialized pin
heights still fail semantic reading and write no receipt.

Exact `problem.npz`: CSC matrix arrays/shape, RHS, Hessian diagonal P, linear q,
number of inequality rows, reference datum, separate pin indices/heights, primal,
dual and slack. Unknowns are `x = ASL_height - reference_height_m`, all unbounded.
First `inequality_count` rows have `A x <= b`; remaining rows are equalities.
Objective is `0.5*x.T*P*x + q.T*x` (constant omitted). Native comparison must retain
these semantics. `weighted.npz` preserves mesh, prior, weights and complete
flow/source-face rows; original source semantics stay in the pinned fixture JSON.

Verified producer/cost commit `0d6855f1e`, final reader `75e027007`; private proof
root `/tmp/outshine-transport-resume-1f10d55eb/`. Current important pins:

| Artifact | SHA256 |
|---|---|
| `water-fit-final-area20/comparison.json` | `e22f95d35f6bcbbe8f2d91ed917c77b5037307903f8b7b136983dcc8d573f85d` |
| `water-fit-final-area20/problem.npz` | `f84cbed712a84821c4620dd820b0df81080a9c97cca4dba1e94f402a71129793` |
| `water-fit-final-pins20/problem.npz` | `980a6786f40ef8437937120ee20ce23ead35177f3aad1ca0605b78f365f39a18` |
| `water-weighted-comparison.png` | `297e8c06b629ffcc039c0ec2ccda12530241fabc10b0b2f86a2731198319f65e` |

The existing shared norm-constrained weighted fitter has a different feasible set
and flat-reference pin contract. It is deliberately unchanged. An optional directed
library contract, arbitrary seam pins and its consumer migration require a separate
verified change. Master owns native water/terrain/contact/cache integration; #760
and the transport goal remain active.
