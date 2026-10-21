# Equal-bound uniform control for direct road upper surfaces

Research [#964](https://github.com/Weichwolf/outshine/issues/964), extending the
adaptive reference in [#966](https://github.com/Weichwolf/outshine/pull/966).
The missing uniform opponent is now measured against the complete same corpus.
Keep the adaptive Python reference: it uses less work, data and build time here.
This is neither a native port request nor a complete road/LOD admission.

## Same input and quality condition

All 1,462 original recipes / 13,944 analytic intervals are retained. The versioned
`road_curve_requests` archive SHA256 is
`df68c2ff306fe5115db4daf7b1efd366caa96338bf673634bd9c5bba9f9d2bf8`.
Generation reads only range, section, seams and intervals. Original emitted Upper
arrays are offline controls, not construction input. Source IO/decode and prior
functional-profile fitting are outside the measured model/mesh stages.

Both methods include the same `Curve` preparation, piecewise longitudinal
envelope, triangulation twist and Float32 vertex displacement. The admission limit
is the existing 0.25 m **model** bound; no threshold was relaxed. Uniform starts
with one whole-curve interval and doubles the global interval count until every
interval qualifies. Each failed grid stops at its first failing interval. This is
the first qualifying dyadic grid, not an optimal uniform station count or a sweep
over arbitrary grids. Adaptive still splits only failing intervals.

The shared mesh assembler is extracted without changing adaptive output. All 4,386
adaptive station/position/index arrays match the frozen #966 complete product in
dtype, shape and words. Four new analytic tests plus the existing nine pass.
Both methods are checked against native metadata midpoints and sampled actual
triangulated surfaces for all recipes. These reuse the declared analytic evaluator;
closed-form parabola/circle/twist controls remain separate. The real-arithmetic
envelope is not a universal rigorous floating-point/geometry proof.

## Result and cost

Scientific producer `d2f62837b`, Python 3.13.5 / NumPy 2.5.4, Linux shared host,
one BLAS thread. Three fresh normal processes alternate method order per recipe;
the second also reverses that order. A fourth process traces allocations separately.
Concurrent foreign builds/lint explain substantial absolute-time variation; retain
all runs rather than selecting one. They are not native i5/Iris-Xe/A18 timings.

| Whole corpus | Adaptive | Uniform dyadic control |
|---|---:|---:|
| Stations | 9,063 | 30,481 |
| Upper triangles | 45,606 | 174,114 |
| Float32 positions + UInt32 indices, B | 982,296 | 3,552,456 |
| Model pose evaluations | 80,152 | 119,584 |
| Largest stored-model bound, m | 0.249861 | 0.249750 |
| Largest sampled surface error, m | 0.227286 | 0.227286 |
| Build including model, three runs, s | 5.015 / 9.673 / 8.106 | 13.146 / 25.163 / 20.596 |
| Model preparation within those runs, s | 0.358 / 0.686 / 0.594 | 0.362 / 0.715 / 0.635 |
| Largest single-candidate traced build peak, B | 740,271 | 789,104 |
| Retained diagnostic plan arrays, B | 1,054,800 | 3,796,304 |

Adaptive saves 73.8% of upper triangles, 72.3% of position/index payload and 33.0%
of evaluated poses against this control. Uniform takes 2.54–2.62 times its build
time, including identical model preparation. All 8,772 arrays repeat word-for-word
across the four processes. No zero-area or nonpositive-XY triangles were found in
either stored output. This does not prove absence of nonlocal intersections or
valid ports, collision bodies and complete physical road surfaces.

Each method retains 4,386 final plan-array owner buffers in the diagnostic corpus;
stations are included in that count, but excluded from the geometry-payload row.
Derived model-array owner-byte volume is identical at 3,968,208 B across recipes.
Known position materialization writes (Double concatenation plus Float32 cast)
total 1,305,072 / 4,389,264 B. The four known final array allocations per recipe
(stations, Double positions, Float32 positions, index owner) allocate at least
1,924,848 / 6,722,480 B over the corpus. These are work volumes, not simultaneous
residency or complete allocation/copy traffic. Intermediate pose/index/list and
Python allocations remain outside those lower bounds; traced peaks cover their
incremental stage allocation. Whole-process RSS is 80.74 MB with offline pin/probes,
67.17/67.24 MB in the other normal runs and 68.20 MB traced, including both saved
output sets. Do not infer a 72% application-memory reduction from payload bytes.

## Visual check and remaining scope

A fresh reader renders only saved meshes plus separately saved original Upper and
densely sampled analytic-reference meshes; original-corpus opens are forbidden.
Two actual recipes show the largest uniform triangle excess and largest sampled
uniform error. The four-way 3D diagnostic was opened. It is an engineering plot,
not PBR, a full native world image or a device frame qualification.

Matplotlib 3.11.2 was observed to mutate the array passed to `set_box_aspect`.
The first new plot consequently cropped later panels. The corrected helper passes
a copy and asserts all view limits. The older saved #966 image was reopened and
remains visibly complete; its helper was also made safe for current Matplotlib.
No original numeric proof or pinned old image was overwritten.

The original native Fine sampling still is not a uniformly qualified 0.25 m
opponent; its previously recorded violations remain. This comparison closes only
the missing equal-envelope uniform control. Normals/materials/UVs, placement,
source ports/junctions, walls/caps, coverage/residency, full generation cost and
both native devices remain unqualified under #915/#964. Functional profiles and
existing Fine products stay unchanged. No new cache, native ABI or shader.

## Reproduce

From the repository root use `.venv`, NumPy and existing reference-library deps;
set `OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1` and
`PYTHONPATH=lib/src:test/experiments/transport`:

```sh
.venv/bin/python -B -W error -m unittest test_road_surface_lod test_road_surface_uniform
.venv/bin/python -B -W error test/experiments/transport/compare_road_uniform.py --output /tmp/outshine-road-uniform --verify
```

Outputs must be fresh explicit system-temp directories. Add `--adaptive-pin` with
the frozen #966 `plans.npz` for the byte control; `--uniform-first` reverses pairing.
Run three normal processes and a separate `--trace` process after freezing sources.
Local raw reports, pin input, plots/reproducers and receipts are linked from the PR;
no images are published in the repository. SIGGRAPH starting point remains
[Cohen et al. 1996, Simplification Envelopes](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf),
page 3/Figures 2–4 visually inspected. This is surface-distance context, not an
implementation of that mesh-envelope construction.
