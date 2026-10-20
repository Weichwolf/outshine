# Shared column geometry probe (#899)

This is a geometry experiment, not an accepted far renderer. One common tangent
frame holds sparse XY columns with a variable number of disjoint vertical spans.
It consumes the supplied parent index and 64 prepared form blocks. The parent has
13,218 source records and 15,554 shapes; these are not 64 native reference views.
The one supplied native cell remains the independent image/depth control.

## Input and construction

`parent_surface_inputs.py` verifies the original archive/package digests and
joins each form block to the index's original source ordinals for that cell.
Membership need not be contiguous. Frame, bounds, minimum height, colour and face
count must agree before use. All supplied external source IDs happen to be
anonymous `(0, 0)`: deduplicating by that pair would incorrectly collapse the city.
The finished field retains parent identity and a source table containing original
ordinal, external kind/ID, cell and support state. It does not need the index to
recover those identities at query time. The PSB1 reader is reused without the
parked cached-stream/transport import chain; its scalar budget is explicitly
larger than the unchanged default budget for individual BSC3 blocks.

Flat, gable, shed, mansard and sawtooth roofs become planar footprint bands.
Supplied hip facets are checked for coverage before use. Footprint holes and
nonconvex rings use Shapely intersections, not a convex approximation. Domes,
invalid footprints, uncovered hip plans and absent source forms produce named
local original/finer needs. The selected native cell stores its needed original
source triangles as a ready fallback; the full parent has no additional native
geometry export and retains its unresolved needs explicitly.

Each roof patch maps affinely from its actual source frame into a common tangent
frame. Intersecting grid cells take its roof maximum and lower height bound.
Overlapping vertical intervals are united; separated intervals retain their gap.
Every interval keeps all original source/part contributors. A common-up wall
approximation remains: the measured lateral tilt displacement is recorded, not
claimed to be a complete geometric error bound. Outward-rounded Float32 heights
avoid contraction during storage conversion.

The probe chooses the largest dyadic pitch whose XY-cell diagonal projects to at
most one pixel at the nearest supplied probe depth, with the unchanged
1280×720 cameras. This yields 4 m here. **This is not a total image/depth error
bound or a relaxation of native LOD requirements.** Filling the rest of a touched
cell can bridge a visible gap and change the first surface by hundreds of metres.
No cell-centre-only sampling, layer cap, fractional opacity or averaged categorical
material identity is used. Three original poses screen geometry before a full
120-pose/attribute gate; rejected candidates do not advance to that gate.

## Consumer and cost interpretation

The finished consumer builds a dense XY lookup index and directly traverses the
implicit spans with packet DDA. It does not expand boxes into triangles or rerun
form/mesh cooking. Queries from inside an occupied near-plane column explicitly
require an original/finer product; no interior reuse domain is claimed. Axial box
normals and neutral Lambert lighting only visualize geometric distortion. They
do not preserve original normals, UVs, metres, codes, RGBA, PBR or current shadows.

Array payload, sparse indices, full lineage/source table, native fallback and
derived dense lookup bytes are reported separately. The independent geometry
storage control factors identical original Float32 XYZ bits and uses the smallest
fitting integer corner indices; it excludes shading and lineage and is labelled
accordingly. There is no comparison against an invented native GPU layout.
Cook work counts shapes, patches, polygon/cell intersections and input intervals;
consumer work counts traversed ray/cell pairs and ray/interval tests. Python wall
time, fresh-consumer traced memory and process peak RSS are separate evidence.
Total allocation count and copied bytes remain uninstrumented. Neither Python
time nor a geometric byte reduction proves an A18/Iris frame or world budget.

The common parent build, sparse occupancy and named needs extend the experiment
beyond isolated per-object surfaces. They do not supply the remaining 3,066-view
world products, an adaptive hierarchy, projection/error-driven group selection,
motion/build reserve, caster replacement or global amortization. Those remain
#899/#844 work. A one-pixel cell-size heuristic alone is rejected as an error
criterion; adding material packing or a resolution sweep cannot establish it.

## Reproduction

Requires the existing `.venv` packages NumPy, Shapely, trimesh, rtree and Pillow.
From the checkout root (private outputs must be fresh system-temp paths):

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python \
  test/experiments/render/compare_column_field.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-column-comparison
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python \
  test/experiments/render/bench_column_field.py \
  --product /tmp/outshine-column-comparison/cell \
  --request /tmp/outshine-column-comparison/static-020.json \
  --output /tmp/outshine-column-consumer
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python \
  -m unittest test_column_field test_prepared_fields test_attributed_prisms \
  test_projective_field -v
```

Inspect native/field/error PNGs and the candidate-only 1280×720 parent image.
The latter has unresolved local needs and is not an original full-parent image
comparison. Fresh-consumer candidate depth, normal and interval arrays must equal
the cold comparison's arrays. Numeric outcomes, inspected artifacts, commit and
terminal checks belong to the delivery PR; no diagnostic images are published.

## Primary sources

- Losasso and Hoppe, SIGGRAPH 2004,
  [Geometry Clipmaps](https://hhoppe.com/geomclipmap.pdf); local paper
  `doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf`, page 2 visually
  inspected. Its terrain assumptions and explicit exclusion of buildings do not
  validate using a single height field for a city.
- Gobbetti and Marton, SIGGRAPH 2005,
  [Far Voxels](https://www.crs4.it/vic/data/papers/sig2005-farvox.pdf), page 3 and
  Figure 3 visually inspected: grouped hierarchical volume proxies and
  direction-dependent appearance. This probe implements neither their BSP
  hierarchy nor visibility-fitted directional reflectance. The paper remains a
  primary comparison for subsequent spatial grouping, not a device-performance
  claim for this model.

Native recipe semantics remain in `BuildingPreparation.cpp`, `BuildingSurface.cpp`
and `RoofSurface.cpp`/`RoofSurfaceHip.cpp` under `src/generators/building/`.
