# Shared sparse layered-field tree (#899)

This is the first shared hierarchical comparison following #844. It replaces the
old fixed per-building capture sets with common spatial nodes across all sources.
It is a rejected representation proposal at the tested profile, not a native port.
The executable comparison and negative controls support further work under #899.

## Method and actual input

The unchanged full native cell is
`test/outshine/data/render/building-full-view/cell.json.gz`, SHA256
`31e296fdda1e8ffebba3303bf28f35e4de99c29410e19bc9983a6e375696d3da`.
Existing readers validate the archive/native view, original attributes and source
bindings.576sources/14169triangles remain the reference, not a synthetic skyline.
This is the supplied selected Full/Shell product, not a new Fine/world admission.

The cook partitions triangle bounding boxes into a shared octree of cubic nodes.
Crossing triangles participate in all intersected children; nodes are not owned by
individual buildings. Each node has an8x8grid per signed axis, with sparse variable
hit lists. Each triangle chooses its dominant geometric-normal axis, avoiding the
unbounded grazing reconstruction of earlier forward tangent patches. Hidden hits
remain; there is no four-layer truncation. Duplicate coplanar triangle hits are
retained too, so hit count is not automatically distinct physical depth complexity.

Cook samples use actual original triangles. Each stores Float32 local depth,
separate geometric/shading normals, explicit material code, source/original-face
provenance and affine coordinate/metre/RGBA fields. Source shader code and mod are
evaluated at vertices before interpolation. Default white RGBA and zero metre
fields use explicit sparse storage. Nonwhite alpha is tested independently.
Flat original shading normals are required by this profile; unsupported smooth
inputs fail rather than silently losing their variation.131072source triangles,
16384nodes,262144hits/node and2097152total hits bound the experiment.

The tree is sized for the explicitly supplied bounding box of actual test-camera
positions. `stamp` projects one physical grid edge using the declared viewport
and focal length. Its1px limit is a **sampling-demand parameter**, not a two-sided
geometry, opacity or PBR error bound; the full square/normal-dependent footprint
is larger. Original1280x720and2x2reference samples are unchanged. The self-contained
perspective sizing estimate is not imported from a parked geometric-proof module. Neither the
motion box nor this pixel-spacing criterion proves a universal reuse radius.
The field data do not depend on camera rotation or lighting during query.

Runtime first selects metadata nodes. Too coarse leaves report unresolved/original
product demand. It then reads only selected field blocks. Cut queries never read
source geometry or recipes. For the CPU reference, geometric planes and grid-cell
footprints reconstruct finite quads, clipped to their node boxes. These patches
are expanded to triangles and indexed once for the planned test-motion domain;
the same cut is held across the120queries. This separates offline preparation
from queries, not an implemented asynchronous engine or a legal frame-IO policy.
The real engine's original fallback/publication path remains unchanged.

Independent tests cover affine surfaces along all three dominant axes, metric
coordinates and nonwhite RGBA (including mixed default-white/coloured nodes),
more than four hidden layers, near unresolved
leaves, crossing source boxes and exhausted node budgets. Dominant-axis clipping
bounds footprints but does **not** bind their whole area to the original triangle.
An original-face tag only binds the sampled material provenance. Native Part/Face
IDs remain unavailable; a tag must not be mistaken for exact novel-view ownership.

## Measured rejection and costs

The initial actual cook creates917nodes/41955records across all levels. Every
source has samples in the stored tree. Total stored field arrays2300046B plus
94008Bmetadata exceed the original1028316Bstored geometry/attribute arrays, or
943302Bafter exact u16 index narrowing. NPZ container bytes are separate. This
comparison includes actual original fields, not just positions or a fabricated
vertex layout. Active-cut coverage of source identities is reported separately;
an unsampled source is named with a reason rather than silently discarded.

The planned current motion domain selects694nodes/31469records, reading1715254B
of fields. CPU expansion produces65624proxy triangles. The derived attribute
arrays2800741B and proxy geometry/record arrays5774912B are actually retained by
the CPU query. The original loaded field blocks are released after expansion;
adding their read bytes again as simultaneously held RAM would double count them.
Header arrays, Trimesh/Rtree allocations and query/output buffers add further cost.
Both the compact field representation and the CPU expansion are worse than the
native943302B/14169triangle alternative in this cell. Python cold preparation
and query timings are not native raster/GPU times or device-budget evidence.
The main comparison's cache files have just been written; its cut is cold in
memory, not an OS-cache-flushed disk measurement. `bench_layered_tree.py` separately
loads the field assets in a fresh process and queries the emitted request. That
consumer accepts no original geometry/recipe input. Its output arrays must match
the main trace, with cache-hit loading and held-query times reported separately.

The first actual static20control already loses10and adds7point samples, with57
changed material codes and49changed source owners among common hits. Known affine
coordinate tests pass, so these are not excused as a matrix/colour-reader bug.
Extending a sampled plane over its cell changes boundary visibility and field
ownership. Complete pose/normal/depth/two-light error records remain in the final
run, including all failures. No resolution/layer sweep or native port follows.

The full driver uses all120actual static/moving cameras and the union of native
and reconstructed extents at the same resolution. It checks reference rays
independently against all native triangles. Codes, coverage, source changes and
normal angles are measured for all relevant hits; continuous-field maxima are
explicitly conditioned on equal source/code. Raw arrays preserve the other hits.
Two above-horizon Lambert directions show unlit-field reuse, not native PBR.
Material-ID and normal images separately include the worst distinct error cases.

## Global selection and next contract

A separate metadata-only control partitions all3066actual inventory views before
reading any of their geometry/field payloads. The existing bounds leave all3066
leaves needing finer field metadata; no coarse joint node satisfies this profile.
These are native view/DAG bounds, not the proposed field-node bounds. For the
controlled view their extent is2232x2340x1744m while actual stored geometry spans
262x311x225m. That is a measured scope difference, not a newly declared native bug.
The one-cell reference is loaded separately for image validation. No global field
products, total residency reduction or world-budget proof are inferred from this
metadata control. A usable field hierarchy needs its own cooked spatial headers.

The isotropic object-space surface lattice retains too much hidden/back-facing
surface detail while still approximating silhouettes poorly. Sharing affine
attribute programs could reduce repetition, but would not repair these coverage/
ownership errors or the proxy primitive count. Further #899work should compare
projector-aligned, anisotropic shared sampling and explicit disocclusion support;
it must not disguise this failed profile with a larger pixel budget. Actual draw
material bindings, PBR/filtering/derivatives/opacity/TAA/shadows, movement reserve,
async publication and both target-device budgets remain open in #844.

SIGGRAPH-first basis: Shade et al.1998 *Layered Depth Images* (existing local PDF)
and Chang/Bishop/Lastra1999 *LDI Tree*, DOI10.1145/311535.311571, local
`doc/references/geometry/siggraph/1999-ldi-tree.pdf`; pages2/3with diagrams and
equations inspected. This uses their spatial sampling hierarchy idea, not their
published colour-filtering or splatting implementation/guarantees. No code copied.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -m unittest \
  test_layered_tree test_attributed_prisms test_full_view_reference
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/compare_layered_tree.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --product /tmp/outshine-research-layered-product-<commit> \
  --output /tmp/outshine-research-layered-report-<commit>
.venv/bin/python test/experiments/render/bench_layered_tree.py \
  --product /tmp/outshine-research-layered-product-<commit> \
  --request /tmp/outshine-research-layered-report-<commit>/cache-request.json \
  --output /tmp/outshine-research-layered-cache-hit-<commit>
```

Both paths must be explicit system-temp paths; product is created if absent and
binds its input SHA/parent/view. Existing NumPy/Trimesh/Rtree/Pillow are used. The shared Oct16x2 encoder lives in
packed_normals.py; its existing500000-direction roundtrip is included in the gate. Open
the generated PNGs locally; no image is published in doc/README. Full-world goal
and #899remain active; this delivery only completes the orthographic-tree control.
