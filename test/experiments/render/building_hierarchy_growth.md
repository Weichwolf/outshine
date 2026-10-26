# Real source population growth before fine geometry

Owner #1167. Extend the existing source hierarchy to multiple original cells;
do not clone buildings, assume cell numbers encode geography or rebuild fine
meshes as inputs. `build_case` accepts a cell set. Existing single-cell `case`
remains a wrapper with its previous report identity and generated products.

Populations are nested nearest-cell sets around an explicit evidenced seed cell.
Cell centres use the original supported source bounds. A cell with no evidenced
centre fails selection, rather than silently disappearing. Every source inside
each selected cell stays selected, including unsupported/unknown sources. This
is growth inside one archived parent, not geographic worldwide coverage.

First audit only the original index: source bounds, binary hierarchy and radial
cuts at16/64/240km. Check that no form read occurs. Count visited nodes, selected
parents, individual original leaves, unbounded originals and full tree bytes.
These original needs are only a lower bound before parent geometry/material
qualification. A small selected cut does not imply a small metadata read: the
current diagnostic loader still loads the stored whole tree arrays.

Then generate the existing parent recipe for64/240km and both existing fixed
directions, before any fine-image oracle. The target estimated span remains8px,
focal length900px and output profile1280×720. A larger physical region naturally
covers a larger image area at the same distance; input size changes, not the
recipe/quality settings. No equal measured image-quality claim follows. The16km
audit is metadata-only; no near-world/frame-budget admission is inferred.

Source forms are prepared once for the demanded nodes, material surface measures
once for their union, and node products reused across queries. Within each shape,
wall-domain construction reads each immutable roof boundary once and retains its
coordinates only for that call. Edge projections, interval ordering/tolerances,
polygon construction and material accumulation remain unchanged. This avoids
repeated GEOS extraction without a persistent cache or approximate geometry. Compare actual
post-qualification original needs with the earlier lower bound. Missing parent
files retain every source in that subtree; unbounded originals remain required.
Count known source-summary faces separately. They are not native runtime
triangles or byte prices. Unknown originals have explicit unpriced IDs, not zero
cost. Source/function/near products stay intact; this adds no asset/DB format.

The unchanged single-cell producer can be checked byte-for-byte against earlier
tree/node arrays, preserving its existing image proofs. Larger populations need
appropriate complete original-image controls and remain unqualified where those
are missing. No new export/API/cache framework or native C++ implementation.
Actual findings, source-input contracts and missing prices belong in the PR.
The optional component expansion comparison is specified in `building_parent_buffer.md`;
scalar remains the default recipe.

Use `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_hierarchy_growth.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --seed-cell 33 --cell-counts 4 \
  --output /tmp/outshine-hierarchy-growth-4
```

For process-peak comparisons run each population1/4/16/64 in a fresh process
with its own fresh Temp output. In a multi-case invocation, process RSS peaks
are cumulative, not independent case peaks. The outer cost includes original
archive verification/index loading; per-case cost starts after that input stage.
Form bytes are decoded source-package bytes, not all IO. The archive and its
manifest remain read-only. No source acquisition or automatic update occurs.

Input provenance and the finite hierarchy recipe remain `building_hierarchy.md`
and `building_transition.md`; no new rendering/simplification algorithm is
introduced here. Prior SIGGRAPH/source checks and image limitations remain.
The experiment records work, arrays/bytes and process costs; it neither measures
all allocations/copies/bandwidth nor admits A18/Iris GPU, whole scenes, missing
originals,240km visibility or1280×720/60-p99.
