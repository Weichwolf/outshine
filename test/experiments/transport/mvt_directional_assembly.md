# Batched directional constraint assembly

Owner: [#1003](https://github.com/Weichwolf/outshine/issues/1003).
The physical height model, source domains, face/segment membership, gradient
limits, precision margins, row order, projection, solver and post-audits remain
unchanged. This optimizes Python construction of the existing program, before
asynchronous asset generation; no frame-path or native change is introduced.

A profile of the seven-fit [alternative-port search](mvt_port_alternatives.md)
identified repeated per-face inversions and scalar LP-row insertion. For each
non-stair owner, `mvt_t_directional_facets` now computes original/Float32-coordinate
gradient matrices once per source face, reuses them for all influencing segments,
and appends each segment's directional rows in one block. Stairs retain their
existing whole-norm facet construction. `Conditions.add_rows` owns copied Python
row terms while preserving the same sparse term/row order and duplicate entries.
Temporary arrays are limited by the existing local source/face caps. No persistent
cache, extra invalidation mechanism or geometry tolerance is added.

`mvt_directional_scalar_control.py` preserves the pre-optimization implementation
from Master f8b38e692, solely for tests and comparison. Production never imports
that control. This is NumPy batching of an existing affine-gradient calculation,
not a new transport algorithm or solver. The graph/geometry separation remains
that of [the local SIGGRAPH transport references](../../../doc/references/transport/README.md);
no additional paper or dependency is needed for this assembly change.

## Reproduce

With the existing Python research environment, from a checkout:

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/transport "$PY" \
  test/experiments/transport/compare_mvt_directional_assembly.py \
  --method batched --repeats 32 \
  --output "${TMPDIR:-/tmp}/outshine-directional-batched"
```

Repeat with `--method scalar` and a different output directory. `--trace` is a
separate instrumented run. The standalone comparison first constructs/audits the
actual original native-control body and its shared consumers, then measures
repeated assembly/CSC conversion with the exact arguments captured from that fit.
It reports the full-body phase separately. Inverse/row-call instrumentation runs
outside the timed phase and must reproduce the same program hash.

## Evidence and limits

On i5-1135G7/Linux/shared host, normal priority, three fresh runs per method with
32 assemblies each gave 1.285/1.108/1.113 s for scalar construction and
0.593/0.547/0.539 s for batching (medians 1.113/0.547 s). Initial trials overlapped
a nice-10 matrix-equality check. Imports, serialization and plotting are excluded.

| Work per original-control assembly | Scalar | Batched |
|---|---:|---:|
| 2×2 inverse matrices | 434 | 352 |
| NumPy inverse dispatches | 434 | 50 |
| Scalar row appends | 2288 | 736 |
| Row-block appends | 0 | 4 |
| Separate Python traced peak bytes | 932040 | 904856 |

The mathematical LP rows and stored CSC/bound arrays are identical. Fewer row
appends mean fewer Python calls, not fewer mathematical constraints. Batching
adds temporary coefficient/index arrays; these are included in the traced phase.
Python traced memory excludes opaque native allocations and is not complete
allocator/copy accounting. Whole-process peaks were approximately 108–110 MB;
no meaningful app-memory saving is established.

All 48 combinations of the eleven actual queried native profiles were rebuilt
with both assemblers. Full projected CSC indices/data, original bounds,
objectives and variable bounds were identical, including numerical array bytes.
The targeted tests also compare raw sparse term order on original and rigidly
transformed inputs, disabled precision, a tighter crossfall recipe, empty owners,
uncovered geometry, bad precision and copied row-buffer ownership.

Eight fresh full seven-fit searches (three normal and one traced per method)
produced byte-identical compact bodies and shared consumers, also matching the
previously opened #1041 diagnostic. Full measured query/selection/fit/body/audit/
consumer phases had medians 1.636/1.543 s; one batched first run was slower
(2.058 s versus scalar 1.843 s). No repeatability beyond this small shared-host
comparison is claimed. The model-building component improved more than the full
pipeline. Full-search traced peaks were 2239535/2236270 bytes, effectively unchanged.

The actual compact body remains 372 vertices/740 faces/18 risers with unchanged
source coverage, fixed contacts and physical audits. These are Python generation
measurements, not native/device, frame-time, world or Places admission. Native
source ownership, outside splitting/packing, nav stations and terrain remain open.
