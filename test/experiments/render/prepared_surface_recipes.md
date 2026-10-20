# Prepared material recipes against original native fields

Research issue #895, parent #844. This supplies the previously missing material
recipe input for the parametric geometry comparison #888. It is Python research;
no native renderer, shader layout, exporter or GPU admission changes.

## Input and method

Use the unchanged complete native building cell under
`test/outshine/data/render/building-full-view/`. Its `cell.json.gz` SHA256 is
`31e296fdda1e8ffebba3303bf28f35e4de99c29410e19bc9983a6e375696d3da`.
The already archived BSC3 block
`fd5393e86f763a2f27b14afd9caaa0cdd86e4a6f90b96d148584c4ff7456f2f1.bin`
has raw SHA256 `ba0941533a5cc8af43665c14cbd52c3abccf75b8b5fc7c97f8908fa9431cde20`.
No additional native export is needed. Validate the archive manifest, parent/view,
raw package digest and every available geometric source/frame/shape/hip field.
The original ascending prepared-cell source order binds all576sources/618parts.
The compressed stored package CRC is provenance only: this archive contains the
unwrapped payload, not the compressed bytes over which the native CRC was made.

`prepared_surface_recipes.py` reads the current BSC3 wire schema with explicit
byte/scalar/list/hip limits, finite scalars and exact EOF. Legacy BSC1 is rejected;
missing fields are not invented. Besides shape geometry it reads Ident, facade
style/variant, bay/floor dimensions, front edge, party walls, optional colour and
hip plans. Reader budgets are experiment limits, not a new product wire contract.
`prepared_material_fields.py` evaluates existing BuildingSurface, FacadeUv and
BuildingWallMaterial coordinate formulas/cast sites; the style catalog comes
from the owned `FacadeProfiles.inc`. The report hashes those native rule sources.
The analytic wall evaluator requires the hit's explicit Gable context. Native
VertexAt receives this from Hit/patch, rather than computing it anew at each
corner. This is part of the required representation state.

The full native mesh does **not** supply Part/Face IDs. They remain -1 in the
original bindings. The diagnostic association only covers one-part, convex,
supported plane recipes with a unique outward plane match. Its tolerance includes
the native1mm weld and world Float32 coordinate quantization. It associates
11946triangles;2037multipart triangles and186unsupported/nonconvex triangles
remain unknown. All14169original triangles and their attributes remain present.
The associated face labels are diagnostic hypotheses, not recovered native IDs.
Corner Gable context uses triangle-centroid height; hit probes use hit height.
These geometric contexts do not prove native patch identity at boundaries.

The comparison evaluates all42507original triangle corners (35838associated,
6669unknown). It also samples every supplied static/moving camera pose, using
the unchanged original geometry and its original hit points. This isolates
coordinate differences from geometry/coverage/depth differences. The2x2 lattice
and actual1280x720 camera produce small cell ROIs, not resized full-screen tests.
Across120poses,92are offscreen;6157covered points are associated and5049remain
unknown. Associated hit categories match, with maximum UV component errors
0.00327003/0.00055514 and metre errors0.00018631/0.00018018. This does not remove
the corner counterexamples or qualify the unknown points.
Original per-triangle material categories are constant at all three vertices for
all14169triangles. Hit category therefore uses that original flat input, not a
category reconstructed from interpolated encoded UV. Raw UV/metres remain in
NPZs and unknown candidate values remain NaN. The3row PNG displays native IDs,
analytic IDs and categorical differences, with unknowns grey. It is a material-ID
diagnostic, not PBR colour, lighting or a claim of a visible product improvement.

## Finding and representation requirement

Small coordinate error does not establish material equivalence. In the measured
cell,252associated corners cross down one encoded material category even though
maximum encoded-U error is only0.00390625. A native edge corner can have U=256
while recomputing it from the quantized stored position gives U=255.999939.
The current `litVertex.glsl` uses precisely this floor(U/256) category decode.
Decoding the recomputed packed value changes the category;164first corners are
affected. `corner-differences.png` lists actual original/analytic values and IDs. The independent known
case in the tests reproduces this with a0.2mm displacement. Do not clamp errors
away or report only the interior sight lines that happen to agree.

A parametric consumer must retain categorical material identity and patch/Gable
context separately from continuous bay/UV/metre coordinates, or prove an equally
stable original-compatible encoding at boundaries. The current diagnostic does
neither and is not approved as an original-material replacement. Explicit IDs
are a requirement for a future representation comparison, not a newly agreed GPU
binding or a unilateral native API change. PBR texture evaluation, colour factors,
opacity, native raster/TAA, normals, coplanar ties and Float32 GPU execution remain
unproved. Geometry error/cost findings in #888 also remain in force.

## Cost and reproduction

The recipe block is346222B:50510scalar reads and10006list elements. This includes
618parts, including the geometrically unassociated ones. The unchanged stored
native geometry/attributes occupy1028316B:28672vertices at24B (688128B),14169u32
index triples (170028B),42540wall-metre floats (170160B), zero wall-colour floats.
This is the actual supplied stored-field accounting, **not** a fabricated48B
vertex record or a qualified GPU allocation layout. The recipe block alone lacks
draw geometry/indices, GPU material bindings and other replacement state; it is
not a complete smaller rendering product. Python decoded objects/BVH/input arrays
and transient copies are additional memory. The report records decode/association
wall interval, tracemalloc peak and whole-process RSS. Allocation count, physical
copy/traffic bytes and native/device times are unmeasured, not zero. Standalone
research CPU time does not support a native performance or frame-budget claim.

Primary methodological context: Mueller et al., SIGGRAPH2006, *Procedural
Modeling of Buildings*, local
`doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf`,
page3/Figure4 inspected. Only the compact parametric shape/material-coordinate
idea is relevant here; current owned Outshine recipes determine the formulas.
No third-party CGA implementation or textures are copied.

```sh
.venv/bin/python -m unittest discover -s test/experiments/render -p test_prepared_fields.py
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/compare_prepared_fields.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-research-prepared-fields-<commit>
```

Output must be a fresh explicit system-temp path. NumPy, Trimesh/Rtree and Pillow
come from the existing .venv. Open the generated comparison.png locally; no
research image is published in doc/README. The report contains pose counts and
all actual mismatches; #895 owns the terminal validation and measured results.
The full-world moving representation and both device budgets remain open in #844.
