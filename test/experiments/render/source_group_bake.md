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
read the whole form block. Native Fine group assets are now independently stored by `BuildingGeometrySource`.
World selection/IO/publication of mixed native groups remains work in #1955. This probe does not
replace that runtime integration or its Metal/Places/movement acceptance.

## Native Fine asset cache

`BuildingGeometrySource` stores selected Fine geometry, PBR materials, intrinsic
frame and exact revision/source domain in `GeometryAsset`. Its recipe includes
source count, canonical ordinals, generator family and the actual material catalog.
No camera or cell address is part of this Fine recipe. Unsupported/partial bakes
fail locally. Encoded-byte and triangle allowances are caller inputs, not peak-RAM
proofs; a worker must retain producer admission. This does not grant a smaller LOD
error bound or publish a mixed World cut.

```sh
cmake --build build --target outshine_experiment_building_geometry_asset --parallel
build/outshine_experiment_building_geometry_asset \
  build/assets/assets.sqlite <base-key> /tmp/group/products.sqlite \
  2000000 2097152 <source-count> <source-ordinal>...
```

Repeat with triangle allowance `0` in a new process to require a cache hit: no
prepared-input database/owner opens. The probe also reopens the asset store and
compares the encoded mesh/materials, frame/coverage and source-read counters.
The output separates first resolve and source-free restore; these are CPU/IO
measurements, not a World frame benchmark or GPU residency admission.
