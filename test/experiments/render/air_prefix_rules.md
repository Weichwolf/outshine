# Complete-prefix control for the thin air moment

Existing candidate [PR #252](https://github.com/Weichwolf/outshine/pull/252),
parent [#206](https://github.com/Weichwolf/outshine/issues/206), replaces four
quadrature nodes with a first source moment for optical depth ≤0.001. This
follow-up checks complete repeated Python prefixes and costs. It changes neither
the proposed moment, cutoff, shared AirPrefix nor any C++/GLSL/layout/ABI.
It does not adopt the optimization or establish native GPU benefit.

## Inputs, methods and bounds

Frozen procedure producer: `57712575ef6c6062c78625186cb4df1c734a20f5` on original
candidate `3ed160238`. Input producer: `af28666aa25900ae7ed3ef0963d3fd4e3ab535f6`;
its Atmosphere.h and three scenario files are byte-identical to master `4de773af7`.
Existing `place_cases`, `build_cells`, `AirPrefix`, coefficient restriction and
`moment_rule` are reused. The wrapper intercepts only the existing cell call;
the prefix's state transitions and old public API remain unchanged.

18 actual clear-gas model paths use Rosenheim, Koerbersee and Malcesine at their
declared camera/times plus existing dawn/dusk/elevation controls. Four further
controls cover homogeneous RGB with a zero-extinction channel, vacuum,
threshold-crossing channels and quadratic extinction with a proportional source.
The model includes Rayleigh/ozone and physical cuts, not the native Mie/Psi/LUT
source. The three scenarios are test inputs, never product special cases.

Coefficients are quantized to Float32 then evaluated in Float64. Edges, query
distances and arithmetic remain Float64: this is not native FP32 branch replay.
There are 392 prepared cells, **7,984 queries**, including exact duplicates,
regular distances, every knot and offsets up to 1 mm on both sides. Zero-distance
calls return the same identity as the native early exit. The before control also
runs the unchanged public Python prefix; every result matches the four-node wrapper
bit for bit. No source values, physical cuts, errors or pins were changed.

`exact32`/`exact64` independently integrate only the selected thin channels using
the existing direct Gauss RTE reference. Thick channels keep the same normalized
four-node result in all variants. Thus these are hybrid references, **not** a claim
of converged thick-cell/full-atmosphere transport. The earlier single-cell
positive-source Taylor bound remains 0.5e−6·exp(0.001) = **5.005002501e−7** relative;
positive prefix weights preserve it without multiplying by the number of cells.
Hillaire, EGSR2020 [page3, equations1–2](../../../doc/references/atmosphere/egsr/2020-production-ready-atmosphere.pdf),
opened optically, supplies the transport context; the bounded moment is the own
polynomial derivation already in #252, not an algorithm attributed to that paper.

## Result and work

All 22 complete cases pass; 16 unchanged prefix/path analytic tests also pass.
Ruff/format, functions ≤120 lines, documentation contracts and whitespace pass.

| Complete-prefix check | Maximum / result |
|---|---:|
| Thin reference 32→64 refinement, relative | 4.380e−16 |
| Moment versus hybrid reference, relative | 4.569191e−9 |
| Moment versus previous four-node prefix, relative | 4.569197e−9 |
| Transmittance arrays | byte-identical in all four modes |
| Duplicate-query radiance/transmittance | byte-identical |

Relative errors use the unchanged 1e−5 radiance floor. Moment-versus-hybrid must
pass the analytical bound plus 2e−14 roundoff; moment-versus-four must separately
pass the existing 1e−5 native radiance tolerance. Closed homogeneous/proportional
controls retain their stricter analytic checks. No transmittance tolerance was
used to hide drift: the arrays are exactly equal.

8,354 cell calls include 784 zero calls and 22,710 positive channel integrals.
The moment handles 10,922 thin channels; 11,788 channels retain four nodes.
Actual evaluated quadrature channel nodes fall **90,840→47,152**, saving 43,688
(48.0934%). Moment polynomial/masking/selection work is additional; those counts
alone do not select a GPU winner or establish whole-frame savings.

## Costs and actual diagnostics

Linux Intel i5-1135G7, Python3.13.5/NumPy2.5.4, shared host with concurrent native
work. BLAS/OMP1. Three ordinary repetitions of the same preloaded full cohort,
normal priority, are separate from three nice10 Memray repetitions with Python
allocators traced. Source evaluation, input loading and plot/export are excluded.
Reported seconds in the functional receipt are diagnostic nice10 observations.

| Whole Python cohort | Four-node | Moment wrapper |
|---|---:|---:|
| Ordinary median, s | 0.933268 | 2.201790 |
| Allocations, three instrumented repetitions | 5,834,328 | 9,574,088 |
| Allocated bytes, three instrumented repetitions | 954,048,552 | 1,333,382,697 |
| Tracked live peak, B | 190,935 | 195,337 |
| Ordinary process peak RSS, KiB | 42,112 | 42,264 |

The Python wrapper is slower and allocates more, despite fewer quadrature nodes.
It constructs masks/restricted arrays and calls the retained thick reference on
subsets. This neither ranks native arithmetic nor excuses those costs. Actual
GPU cost, divergence and complete native-prefix replay remain necessary in #252.
The original four-node product path remains available; no optimization is adopted
from these work counts or this Python implementation.

Preloaded coefficient/edge/query arrays total 123,632 B. Each full repetition
copies 383,232 B into contiguous L/T result matrices in both modes. Those are the
explicit result-assembly copies, not total traffic. Other intermediate copies
remain uncounted; Memray's allocated bytes are not copied bytes or device memory.
All counters and Python allocation/loop overhead are included in these narrow
costs, not charged to an unimplemented native renderer.

The complete error curves, whole-prefix RGB curves, node counts and separate
ordinary CPU results were exported as one PNG/PDF and both actually opened.
They are scientific diagnostics, not rendered world/Place images or a claimed
visible improvement. No image is published under doc/ or README.

## Reproduction and remaining acceptance

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error test/experiments/render/compare_air_prefix_rules.py \
  --input-root "$INPUT_CHECKOUT" --output "$RESULT"
PYTHONPATH=test/experiments/render .venv/bin/python -W error -m unittest \
  test_air_prefix test_air_path_integral
```

Use a frozen input checkout with the declared clear-medium header/scenarios and
a fresh explicit system-temp RESULT outside both checkouts. Dependencies: NumPy;
Matplotlib for diagnostics and Memray for the separately instrumented cost probe.
The final output-path guard was tightened after the frozen numerical run; only
CLI path validation changed, with numerical procedure ASTs preserved.

Original inputs/snapshots, all four complete L/T result arrays, source/input SHA256,
terminal logs, separate cost commands/traces and opened PNG/PDF remain under
`/tmp/outshine-research-air-prefix-4de773af7/` on the research host. Main receipt:
`whole-prefix/report.json`; each `whole-prefix/case-N.npz` retains coefficients,
edges, queries and all results. `prefix_cost.py` replays those snapshots without
regenerating source data. Functional runs used the detached producer above.

This closes the Python whole-prefix mathematical/work comparison only. Native
FP32 threshold/rounding, actual Vulkan/Metal prefix buffer replay, repeated-query
GPU cost, source fitting/LUTs/Mie/Psi, runtime aerial composition, motion, Places
and device/frame/p99/memory budgets remain separate, open requirements. No new
feature PR or native implementation is introduced by this Python handoff.
