# Prepared terrain-normal fields — #1241

The current lattice vertex and each fine fragment-normal node read four height
neighbours again. This experiment moves their differences or local normalization
to a bounded producer. It compares the same Float32 response with the existing
height stencil, two R32 difference planes and a full RGBA32 local-normal field.
No native shader, format, binding, asset/DB or geometry-LOD contract is changed.

`source` owns a frozen 35×35 Float32 page, including its actual halo. A content
digest is computed once during preparation; the captured asset/page/tile binding
is retained separately. Effective metric steps follow the shader's Float32
`max(step, 0.001)`. A difference product consumes height words only. A local-normal
product additionally binds the exact effective Float32 step words. `bind` rejects
stale dependencies and owns a frozen basis. Sampling never hashes or prepares the
source again. These Python records are not a proposed native wire/cache format.

The baseline is the actual east/north central difference, local normalization,
basis application and world normalization. Two R32 fields preserve the differences;
RGBA32 preserves the local unit normal and its unused fourth component. The native
format enum has R32Float/Rgba32Float, not RG32Float: neither an imaginary two-channel
format nor a 12-byte RGB texture is used to make memory appear smaller.
Two R32 fetches versus one RGBA32 fetch remain different costs.

`sample_fine` keeps the current source-cell diagonal, source-node selection,
per-node basis/normalization and Float32 barycentric operation order. It returns
the interpolated normal, as the existing helper does; later material composition
owns final normalization. Hardware bilinear filtering and moving normalization
across interpolation are not substituted. Analytic signs, halo/extrema, flat
normals, nonorthonormal bases and source/metric changes have targeted controls.

The fixed input is the existing page pinned by #1239. Original source words and
provenance remain unchanged; no new source, pose, quantization or packing series.
Current Python Float32 word identity is not a GPU compiler/fast-math guarantee.
The opened figures are normal-response diagnostics, not native PBR/world admission.

Queries are bounded to 262144 nodes/1048576 nominal Float32 source words; fine
calls admit at most 65536 fragments and evaluate three actual source nodes each.
Cancellation is checked at the bounded query's entry/exit, not inside NumPy.
Budget/failure publishes no partial normal. Preparation is only a fixed 35×35
page; no generator/solver is added to the frame path.

## Compare work and memory before proposing a native field

Normal-only nominal fetches per node are 4 / 2 / 1, while texture float words
are 4 / 2 / 4. The original heights remain necessary for geometry/shadow receivers.
Their neighbourhoods are shared: all 1089 nodes consume only 1221 distinct height
words (4884 bytes), including position/receiver heights. The extra fields require
8712 / 17424 bytes; combined different texture words become 3267 / 5445 (13068 /
21780 bytes). Fewer instructions therefore do not establish lower bandwidth.
Unique logical addresses are not measured cache misses or physical transactions.

At 16255 identical pages the extra field payload would be 141613560 / 283227120
bytes, before native packing, metadata, staging, queues or allocator overhead.
This is arithmetic, not measured residency or permission to spend the app budget.
Fields are conditional alternatives; keeping both in this comparison is not a
proposed production resource set. The common source remains needed.

```sh
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/render/terrain_normal_products_experiment.py \
  --width 256 --height 256 --repeats 20 --render \
  --output /tmp/outshine-terrain-normal-products
```

One first plus 20 following calls rotate order equally across 21 invocations,
separately for all source nodes and a fixed UV chart. Source snapshot/hash/metric
preparation and product build/bind are outside query timing and reported separately.
This measures prepared reuse, not a real DB hit or fresh offline-process loading.
Build/query allocation traces use separate calls after GC. Live traced blocks are
not total allocations; selected gather/result bytes omit other NumPy copies.
Whole-process RSS includes all compared fields, outputs and unique-address
diagnostics before export/plot; no UMA saving follows from a Python peak reduction.

A native choice needs measured total benefit against the current height atlas,
including neighbour cache reuse, extra IO/upload/residency, actual shared bindings,
compiler arithmetic and image stability. The older #1182 shared-stencil/gather
negative remains valid. This reference does not justify a new native micro-loop
or default change merely from Python time/instruction counts. Current results
and actual image inspection belong in the owning PR.

## Primary source

[Geometry Clipmaps, SIGGRAPH 2004](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf),
page 3/section 3, was read and optically inspected. Its separate normal maps are
updated with geometry changes. That motivates preparing a reusable appearance
product; its 8-bit encoding, doubled normal resolution, toroidal updates and LOD
heuristics are not adopted here. Our target remains the existing fine normal
response with the same source data and operation order.
