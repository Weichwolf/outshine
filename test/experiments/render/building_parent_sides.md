# Selective sides on source-hierarchy parents

Owner #1167; source hierarchy and controls are #1312. This bounded continuation
compares unchanged complete parents with six fixed dominant-normal signs.
It does not repeat a group-size or camera sweep. Existing #894 mixed-group and
native cost counterexamples remain applicable; this is no native culling port.

`facing_cones.py` extracts the unchanged Double cone construction/predicate from
`native_backface_cost.py`: enclose geometric normals, move the apex behind each
triangle plane, reject only beyond the cone with the existing spatial margin.
Preparation and metadata-only querying are separate. Degenerate/unknown cones
retain geometry. The counter independently verifies every rejected original
parent face against its own strict plane predicate. Original source identities,
remainder geometry, logical/collision assets and shadow needs do not change.

The raw-array model charges64-byte Double descriptors and8-byte index ranges per
nonempty side, shared positions and all parent material/frame/tree/needs arrays.
Only selected index/triangle-owner arrays become smaller. Full shared XYZ remains;
no invented perfect vertex loader or compressed descriptor is credited. The
model is deliberately optimistic about container/native dispatch overhead and
does not implement a split cache. Diagnostic parent-face maps and preparation
arrays are separately reported; they preserve comparison tie order, not original
source IDs per pixel. Their potential runtime ordering cost is unqualified.
Tree metadata and actual original-remainder costs remain in #1312's comparison.

The decision follows total bytes and image controls, not rejected triangles.
Coarse parents can have too little index data to pay for direction metadata.
An occasional positive descriptor-only count is no overall runtime/device gain.
No split format or native standard follows from this experiment. Measured results
and the positive/negative cases belong only to the delivery PR.

Run with `.venv` Python, one OpenBLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`.
First reproduce the products and image reference in `building_hierarchy.md`.

```sh
python -B -W error test/experiments/render/measure_building_parent_sides.py \
  --input /tmp/outshine-hierarchy-products --cells 33 44 \
  --output /tmp/outshine-parent-sides.json
python -B -W error test/experiments/render/check_building_parent_sides.py \
  --input /tmp/outshine-hierarchy-products \
  --comparison /tmp/outshine-hierarchy-views \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 44 --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --native-cell 33 --output /tmp/outshine-parent-side-views
python -B -W error -m unittest test_building_parent_sides test_native_backface_cost
```

Image controls retain the original regular quadrature, resolution, sources,
materials and coarse groups; opaque and fractional C/T must agree within1e-12.
The eight nonempty parent cuts are evaluated; four near cuts contain no parents
and reuse their unchanged controls. Contact sheets expose the same original
hierarchy's image deficits. Equality here qualifies only this omission in the
finite Python reference, not native raster/binding/rounding, shadows, transitions,
physical IO or the complete world.

SIGGRAPH-first source: [GPU-Driven Rendering Pipelines,2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
PDF p.23 optically inspected again (static triangle backface diagram). It motivates
early facing rejection; it does not prove our descriptor byte budget profitable.
