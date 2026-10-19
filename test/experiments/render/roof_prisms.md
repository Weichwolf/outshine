# Hierarchical procedural roof prisms

Owner: #888, parent #844. Python geometry comparison only; no native admission.
This tests existing shape plans before individual triangle emission. It does not
replace logical objects, collision or simulation, or select a new Capture radius.

## Representation

A convex footprint and continuous concave piecewise planar roof define a closed
solid by halfspaces `n · x <= d`. Walls retain original ring order; the bottom is
plane zero, then walls, then roof bands/facets. Gable, Flat, Shed and Mansard use
the supplied band partition; Hip uses supplied facet planes. Convexity, continuity,
roof controls, wall-deck consistency and a positive interior witness are checked.
Unsupported shapes cause **whole-source** original-triangle fallback, including
otherwise supported sibling parts. Nonconvex footprints and holes stay original.
No convex hull replaces an unsupported footprint. This is a distinct analytic
representation; cached Shell tessellation, welding and Float32 placement differ.

Actual records are Float64 ECEF planes and primitive bounds, UInt32 offsets and
original fallback-face IDs, Int32 source-index/part pairs, and Boolean roof flags.
A balanced median BVH stores Float64 node bounds and Int32 children/leaf indices.
The complete source partition is retained. The fallback stores compact original
Float32 positions and UInt32 triangle indices; no original fallback triangle is
removed. Source indices refer to the versioned input parent, not global object IDs.

Each 256-ray packet traverses boxes before clipping ray intervals against leaf
planes (Cyrus/Beck). Nearest depth prunes further traversal; ties select the first
primitive. Interior exits are culled to match the reference's front-face convention.
The balanced tree admits at most 65,536 primitives. Query rays are finite/nonzero,
at most 4,194,304 per call. Box padding is an engineering Float64 allowance, not
interval arithmetic or an all-input omission theorem. No hardware ray tracing is
required by the Python algorithm; a GPU implementation has not been supplied.

## Actual complete input and result

`test/outshine/data/render/building-full-view/cell.json.gz` is the original dense
Tokyo cell from #844/a4bc3a1a3, validated against its native archive and manifest by
`full_view_reference.py`. All 576 sources and 14,169 original Shell triangles are
included, including hidden/back content. 557 sources become 586 primitives with
4,327 planes; 19 unsupported sources keep all 887 original triangles.

| Actual geometry arrays | Bytes |
| --- | ---: |
| Planes, offsets, source/part bindings, roof flags | 149,827 |
| Primitive bounds and BVH | 93,704 |
| Original fallback IDs, compact positions and indices | 35,828 |
| Complete candidate payload | 279,359 |
| Original Float32 positions + declared UInt32 indices | 514,092 |
| Exact shared positions + UInt32 indices | 269,880 |
| Exact shared positions + UInt16 indices | 184,866 |

Candidate geometry is 45.66% smaller than the original unfactored arrays (1.84×),
but **does not beat exact position sharing**: 8,321 unique position records plus
UInt32 indices already use 269,880 B; UInt16 indices use 184,866 B. The candidate
is 51.1% larger than that narrow-index control. Every one of 42,507 original
triangle corners is bit-identical after remapping, including winding/order.
`compare_vertex_storage.py` stores and checks both actual alternatives.

The original vertex buffer has
28,672 vertices. The comparison **excludes attributes on both sides**: candidate
UVs, metres, colours, stored shading normals, opacity, PBR, material recipes,
mips and shader records are absent. Comparing 279 KB to the original full
48-byte vertex model would overstate the saving. Archive size is not residency.
Python objects, reference indices and diagnostic/control meshes are additional.

Both unchanged 60-frame benchmark tuples are evaluated at 1280×720, focal
691.5535657096198 px, including native world-axis rotation and the 6 m moving
endpoint. The cell intersects the screen in 28 poses; 92 poses are rejected by
conservative whole-cell bounds. Visible same-resolution ROIs include original
geometry and analytic primitive bounds, so outward changes cannot hide outside
the crop. Two-by-two subsamples are used; these are finite pixel samples, not a
continuous 1 px silhouette/coverage bound. This one cell is not the entire city.

The first full run evaluates 120,676 subsamples. Maximum coverage L1 per pose is
0.25 pixel: static-025 loses one quarter sample, and one pixel crosses below half
coverage. The largest-coverage-error image was also opened separately. Maximum
common camera-depth discrepancy is 0.01776 m. Two synthetic
lights shade **white flat geometric normals** on both sides. These images do not
compare the original stored shading normals, UVs, actual material or PBR response.
The contact image must be opened; it includes actual static20/moving20/moving21.

The BVH performs 181,681 primitive-ray and 1,377,588 plane-ray pairs, compared with
70,716,136 primitive-ray pairs for a flat all-primitive scan. It additionally does
8,150,100 AABB-ray tests, including child ordering queries. That flat scan is a
work control, **not a native raster baseline**. Original fallback R-tree work is
not instrumented. Counters for logical box/plane operand volume assume uncached
coefficient loads per pair; they are not measured physical memory traffic.
Allocations and copied physical bytes remain unknown. Timings include Python and
tracemalloc; candidate-with-fallback time is reported separately from BVH-only
work. They do not estimate GPU time or replace native p99/device evidence.

## Independent controls and remaining decision

Unit tests include parallel/grazing/missed/interior rays, a gable, source-atomic
fallback and malformed/nonconcave partitions. Independent convex-solid controls
solve all triples of represented planes, keep feasible vertices, use SciPy/Qhull
to triangulate the solid, then intersect with an all-triangle Möller traversal.
This checks the represented solids independently of BVH/interval clipping; it is
not an independent reconstruction of the original building recipes. Actual image
rays additionally compare the corrected Trimesh/R-tree native-mesh reader with
an exhaustive Möller reader. Geometric agreement with the native mesh is measured,
not assumed. Sparse samples do not certify every ray or continuous motion.

This fixed halfspace model is not the geometry-storage winner. Its low sampled
geometric error alone does not justify native adoption. Exact full 48-byte vertex
sharing finds no duplicate records: 28,672 remain, and UInt16 alone changes
1,546,284 B to 1,461,270 B. The position-only control does not provide a complete
material/attribute layout or a native draw contract. Neither comparison establishes
a GPU-time improvement. Material evaluation/opacity, shadows, actual GPU traversal
versus raster work, near views, other geography/roof classes, cache generation,
transitions and complete world budgets remain open. The reference loads the full
native input for comparison and fallback extraction; no native decode/emission
saving has occurred. This per-cell BVH still contains one record per accepted
shape; a worldwide hierarchy with aggregate far representations and bounded
streaming remains necessary. Vegetation, weather, vehicles and NPCs retain the
same shared budget; no fixed allocation or sight/content/profile reduction.

## Reproduction and primary sources

NumPy, Shapely, SciPy, Trimesh, Rtree and Pillow are required in `.venv`.

```sh
.venv/bin/python test/experiments/render/test_roof_prisms.py
.venv/bin/python test/experiments/render/compare_roof_prisms.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-research-roof-prisms-<commit>/tokyo
```

The output must be fresh and explicit. It contains serialized queried arrays,
per-visible-pose raw arrays, a report including source hashes and a contact PNG.
Private diagnostics stay in system temp; no gallery/doc images are published.

```sh
.venv/bin/python test/experiments/render/compare_vertex_storage.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-research-roof-prisms-<commit>/vertex-controls
```

SIGGRAPH first: [Pharr et al. 1997, Rendering Complex Scenes with Memory-Coherent
Ray Tracing](https://graphics.stanford.edu/papers/coherentrt/) motivates demand
and locality before geometry creation, not a promised real-time implementation.
The author PDF pp.2–3/Figs.1–2 were inspected optically. Its geometry cache uses
triangles; this experiment's direct halfspace representation is a separate choice.
[Cyrus/Beck 1978, Generalized two- and three-dimensional
clipping](https://doi.org/10.1016/0097-8493(78)90021-3) supplies the convex interval
clipping basis. No third-party implementation is copied.
