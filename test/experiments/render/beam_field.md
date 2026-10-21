# Adaptive directional candidate field (#899)

This experiment addresses the gap filling in #909 by retaining complete original
surface candidates wherever coverage/depth is uncertain. It is an opaque-primary
camera experiment for one supplied native group, not a filtered appearance model
or a global renderer. Its current numerical results and decision belong to the PR.

## Representation and geometric bounds

The projection frame is a right-handed signed axis permutation. It preserves the
represented source coordinates exactly. A positive source-depth guard rejects
groups crossing the supported observer domain or profile near plane. Rotation
does not change the field: any forward direction within its source angular bounds
is routed through the same tree; opposite directions miss that group. Origins
must stay inside the explicit actual-motion box. No radius is inferred from the
finite poses or enlarged using a Python cook time.

`beam_intervals.py` propagates outward Float64 intervals through differences,
products, cross/dot products and divisions. Source triangles have possible and
fully-covered angular regions over the entire observer box. Edge coefficients
remain separate at each of the eight box corners before evaluation in an angular
rectangle: collapsing coefficient ranges first loses essential correlations.
Depth bounds intersect the plane quotient with the original triangle's depth
extent. Only a strictly nearer covering surface can remove another candidate;
ties and unresolved cases retain their original candidates. A conservative
boundary allowance includes the reference triangle routine's barycentric
tolerance. Mathematical interval bounds and finite floating-point renderer parity
are separate evidence; no universal numerical equivalence is claimed for every
ill-conditioned input or renderer.

`beam_patch_fusion.py` also tests two-triangle disks sharing an exact welded edge.
Both triangles must face every observer; boundary winding must close into four
distinct vertices and its projection must remain strictly convex over the domain.
This accepts slightly nonplanar quads without replacing their actual triangles.
The projected disk supplies joint coverage across its internal diagonal. Its
maximum original depth supplies an occlusion bound. Concave, nonmanifold,
unsupported or uncertain pairs do not supply a joint certificate. Material/source
associations remain separate; fusion never averages codes or attributes.

The tree refines mixed regions. A half-pixel projected size stops additional
query-index refinement **only**: a mixed leaf still evaluates every retained
original triangle. A node budget likewise keeps complete lists. Best-first
refinement spends the shared budget on angular area times candidate count. That
is an explicit CPU query-work proxy, not an optimality or GPU-cost claim. It
avoids a depth-first budget leaving an arbitrary suffix of the domain coarse.

Only one shared pool of original programs is stored. Leaf references index that
pool; nodes do not hold copies of original geometry. The pool keeps positions,
UVs, packed normals, metres, RGBA, original binding and face IDs. Runtime performs
the existing library plane/barycentric tests, then the original post-vertex
category and interpolation rules. Categorical IDs are never filtered as floats.
The warm consumer does not load original exports, source plans or recook forms.
An unmet domain/identity is rejected before payload reads and needs another or
original product. That rejection alone is not a complete world transition.

## Controls and cost

The driver uses all 120 supplied static/moving poses and eight distinct interior
positions in the same domain, at unchanged 1280×720 projection and 2×2 subsampling.
The actual domain here is planar; synthetic controls include 3D observer motion.
Reference triangles/Rtree and independent Moller–Trumbore controls remain separate
from directional routing. The stored source fields support relighting diagnostics;
Lambert controls do not prove native PBR, texture filtering, TAA, shadows or
reflections. Original logical/caster assets remain separate and are not deleted.

Array bytes include the entire index, references, original program pool and
lineage. CPU preparation separately counts decoded normals, triangle gathers,
geometric/unit normals and vertex-stage copies. Interval and patch construction
are offline working data, not free runtime storage. Query counters count routed
rays/nodes and actual candidate-plane pairs; largest pairs bound one gathered
packet. Total allocation count and all transient copies are uninstrumented.
The exact Float32 XYZ/attribute factorization from #906 is reused as a storage
control with the same original fields and provenance, without reusing its
rejected vector-field layout. Python timings/RSS do not prove device budgets.

This field remains tied to primitive identity in unresolved subpixel regions.
Its global 3,066-view products, original-caster coexistence, domain changes,
construction reserve and complete residency amortization are unproven. If index
and references already lose the cost screen, no material packing or larger tree
budget is a justified continuation of this model. More exact candidates alone
do not demonstrate appearance aggregation independent of object count.

## Reproduction

Use the existing `.venv` NumPy, trimesh, rtree and Pillow dependencies. Outputs
must be fresh system-temp paths; no diagnostic images are published:

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python \
  test/experiments/render/compare_beam_field.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-beam-comparison
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python \
  test/experiments/render/bench_viewcell_product.py --product-kind beam \
  --product /tmp/outshine-beam-comparison/product \
  --request /tmp/outshine-beam-comparison/request.json \
  --output /tmp/outshine-beam-consumer
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python \
  -m unittest test_beam_field test_viewcell_product test_viewcell_visibility \
  test_projective_field -v
```

The benchmark defaults to the original viewcell consumer; `--product-kind beam`
uses the same ready-product protocol for this field. Fresh outputs must equal the
comparison's candidate depth, fields, source and original-face arrays. Inspect the
saved material/normal comparison PNGs, and report offscreen poses explicitly.

## Primary source

Durand, Drettakis, Thollot and Puech, SIGGRAPH 2000,
[Conservative Visibility Preprocessing using Extended Projections](https://people.csail.mit.edu/fredo/PUBLI/Sig2000/index.htm).
The local paper is
`doc/references/geometry/siggraph/2000-conservative-visibility-extended-projections.pdf`;
page 3, Figures 2–4 and the depth definitions were visually inspected. The paper
motivates conservative extended coverage and depth/fusion tests. This prototype
implements an interval-bounded directional candidate tree and two-triangle
coverage fusion, not the paper's projection-plane occlusion sweep or full PVS.
