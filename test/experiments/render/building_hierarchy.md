# Source-first building hierarchy

Method and consumer limits belong to #1167 under #1083. Measured results and
acceptance remain in the delivery PR; this file describes reproduction.

The existing PSB1 source index supplies bounds in the parent Cartesian frame.
A deterministic median tree partitions whole source IDs. An orientation-independent
radial-size heuristic selects a cut before any BSC3 form read or contour work.
Bounds select demand; they never become opaque building geometry. This is not a
projected error certificate, visibility culler or Ready admission policy.

Only demanded nonleaf nodes build source-footprint unions and height bands.
Node-relative closing/simplification creates coarser parents; original holes are
protected. Existing strict Float32 meshing checks remain. Raised/variable-height,
singleton, failed mesh and unsupported material cases retain whole originals.
Source materials are measured once across demanded nodes and aggregated into
separate wall/roof diffuse means and roughness measures. No fine mesh is a producer
input. Node assets contain neither cameras nor baked light.

The experimental NPZ format stores lineage, but render-only reads omit it. Each
parent is bound to the exact source tree/frame and material recipe. The load probe
retains selected arrays across queries and reads only added parents, then releases
unneeded arrays. Full initial tree metadata, original-source map reads, original
needs, resident arrays and diagnostic assembly copies are reported separately.
This is no physical SSD-byte measurement, asynchronous GPU retirement model or
proposed shared cache API. Full tree metadata still scales with source population.

The image comparison keeps every selected source, including the original remainder.
The supplied native capture is used where available; other declared source cells
use an explicitly labelled analytic BSC3 geometry/source-material reference.
That reference removes exactly zero-area triangles after Float32 conversion,
retains every source and uses analytic patch semantics for material streams.
It makes no claim of native postweld, normal or mesher equivalence.

Two parent controls use identical geometry/materials: opaque and fractional coverage.
Three camera-independent coefficients compare source macro-prism unions to each
parent in XY/XZ/YZ projection; side projections use footprint-component extents.
Absolute geometric normal components interpolate those coefficients. This is a
testable approximation, not true fine visibility. Unclipped ratios are reported.
Each coarse group contributes its first ray hit once; actual opaque original
remainders and parent groups are depth sorted and composed as C += T*alpha*L,
T *= 1-alpha. Unknown mask correlation, fine gables, normals and shadows remain.
An all-opaque control must reproduce the original first-hit evaluator.

Both endpoints use the same bounded regular radiance quadrature. GEOS Double
opaque silhouette coverage is measured separately, tiled without reducing the
1280×720 image profile. The furthest views also compare doubled quadrature rates;
this is an empirical change, never a certified radiance integral or error bound.
Fractional coverage may reduce alpha error while increasing RGB error. Neither
control qualifies transport contacts, LOD transitions, occluders, hard shadows,
native bindings, device budgets or the complete world.

Run from the checkout with its `.venv` Python, one BLAS/OpenMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_hierarchy.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 44 --output /tmp/outshine-hierarchy-products
python -B -W error test/experiments/render/probe_building_hierarchy_load.py \
  --input /tmp/outshine-hierarchy-products --cells 33 44 \
  --output /tmp/outshine-hierarchy-load.json
python -B -W error test/experiments/render/compare_building_hierarchy.py \
  --input /tmp/outshine-hierarchy-products \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 44 --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --native-cell 33 --output /tmp/outshine-hierarchy-views
```

SIGGRAPH-first review: [Procedural Modeling of Buildings,2006](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
p.3/Fig.4 and p.8/Figs.17,19,20 visually inspected. Hierarchical annotation and
delayed/manual LOD motivate organization; they do not prove this automatic method.
[Porter/Duff, Compositing Digital Images,1984](https://keithp.com/~keithp/porterduff/p253-porter.pdf),
p.3 visually inspected, supplies the over operation and explicitly identifies the
unknown subpixel overlap assumption. It does not validate our coverage estimator.
