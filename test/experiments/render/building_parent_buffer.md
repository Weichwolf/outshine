# Component expansion of building parents

Owner #1167. The measured positive MultiPolygon buffer can dominate parent
construction memory. Compare its scalar execution with independent positive
buffers of the already unioned polygon components, followed by `union_all`.
Keep the original closing radius, mitre joins, mitre limit2, negative buffer,
simplification, protected original holes and strict F32 extrusion checks.

`building_parent_buffer.expand` retains scalar execution by default. The
experimental `components` path only decomposes inputs with at least256 polygon
components. Smaller inputs keep the exact scalar call, avoiding the observed
extra overhead and geometry changes in small groups.256 is a conservative
scheduling heuristic, not a measured optimum or geometric error threshold.
There is no threshold sweep or relaxed geometry tolerance.

[Shapely2.2 buffer](https://shapely.readthedocs.io/en/stable/reference/shapely.buffer.html)
accepts individual geometries or arrays and exposes the join parameters.
[GEOS cascaded polygon union](https://libgeos.org/doxygen/classgeos_1_1operation_1_1geounion_1_1CascadedPolygonUnion.html)
describes eliminating segments during union rather than presenting all overlapping
segments to a single buffer operation. This motivates the candidate execution;
it does not prove exact equivalence of finite-precision mitre-buffer outputs.
Operation order changes can affect later simplification, mesh order and projected
coverage, even when the expanded sets differ very little. No new SIGGRAPH render
method is introduced; existing hierarchy/reference sources remain unchanged.

The alternative receives a separate recipe identity in the existing tree/node
binding. Readers retain the strict binding check. No new asset schema, native
format, cache framework or native implementation is introduced. Existing scalar
products keep their recipe identity and must remain byte-identical.

Use one fresh process per population/path for meaningful process-peak comparison:

```sh
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
.venv/bin/python -B -W error \
  test/experiments/render/build_building_hierarchy_growth.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --seed-cell 33 --cell-counts 4 --buffer-path components \
  --output /tmp/outshine-parent-components-4
```

Repeat with `--buffer-path scalar` and a different fresh output. Include full
source/material work and the original remainder; the geometry-only microbenchmark
is not a complete producer memory measurement. Archive verification/index loading
is included in the outer cost. Other host work and cumulative RSS peaks remain
explicit limitations, as in `building_hierarchy_growth.md`.

`compare_building_parent_buffers.py --scalar <population-dir> --components
<population-dir> --output <temp-dir>` checks identical source hierarchies/frames
and required original remainders, then measures isolated parent-only C/T changes
with a shared camera/ROI and finite16×16 quadrature where the ray bound permits.
Its image omits the required originals explicitly. It cannot establish complete
source-image error, shadow/depth equivalence or original byte/triangle prices.
Larger populations still need complete native original controls. Measurements,
opened diagnostic images, counterexamples and the delivery decision belong in
the PR; Python results do not admit native devices, world memory or p99.
