# Exact prepared-source bakes

`BakePreparedStructureSources` processes a canonical nonempty ordinal list before
mesh generation. It walks the requested sources, including the existing sliced
planning/emission stages. Invalid, duplicate, unordered, out-of-range or conflicting
cell selections fail before lazy form access. Cancellation publishes no group.

The result is a render product: source count/ordinals, anchor, geometry/clusters,
view/detail and unchanged error metadata. It contains no terrain contacts, parent
coordinate table or cell-coverage claim. An unsupported mesh refuses the group;
callers retain other valid products and resolve that group locally.

The atomic native test compares fixed Fine and view-dependent results with an
independent single-source bake. Unselected lazy forms deliberately fail if opened.
This does not qualify a new error bound or native World replacement.

## Cost control

Build `outshine_experiment_source_group_bake`. Export cohort/support columns as in
[source_product_cut.md](source_product_cut.md). Run with an existing asset DB:

```sh
python3 test/experiments/render/compare_source_group_bake.py \
  --native build/outshine_experiment_source_group_bake \
  --asset-db build/assets/assets.sqlite --base <base-key> \
  --cells /tmp/source.cells --supported /tmp/source.supported \
  --triangle-attempts 2000000 --output /tmp/source-group/report.json
```

The controller requests eight supported sources from the largest supported cohort
and alternates subset/whole-cell bakes in three fresh processes each. `--cell`
selects another explicit input cohort. Unsupported geometry keeps the comparison
failed; retain that counterexample separately. Both modes use
fixed Fine geometry. Coverage deliberately differs: the cell control also builds
unrequested neighbours. This measures avoided generation, not World frame speedup
or the cost of the existing adaptive runtime LOD.

The attempt budget is an explicit experimental input, not a production quota.
Output heap counts owned product capacities; coordinate bytes are a component,
not an additional total. Neither number proves peak process/UMA residency.

Form blocks are still stored per physical cell. Even a small source request can
read the whole form block. Prepared native group assets and independent source
coverage before World IO/publication remain work in #1955. This probe does not
replace that runtime integration or its Metal/Places/movement acceptance.
