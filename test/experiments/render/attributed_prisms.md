# Stored attributed procedural solids (#897)

This joins the geometry reference #888 and original prepared recipes #895.
The executable Python product contains closed convex solids, a BVH, material
coordinate tables, source/part bindings and complete unsupported-source fallback.
Camera and lighting do not participate in assembly. The driver saves the product,
reloads it, and queries only those reloaded arrays. JSON/recipe objects are bake
inputs, not query-time dependencies. It is an opaque Double point-sample control,
not an integrated native renderer or an approved moving-world replacement.

## Geometry and material contract

Input is the unchanged native complete cell under
`test/outshine/data/render/building-full-view/`; `cell.json.gz` SHA256 is
`31e296fdda1e8ffebba3303bf28f35e4de99c29410e19bc9983a6e375696d3da`.
The original archive/native view and BSC3 block are validated by existing readers.
All576source identities are partitioned atomically:557represented sources with
586convex parts/4327planes,19original fallback sources/887triangles. No source is
dropped. The representation is shared spatially and camera-independent; there
is still one primitive per supported shape. It does **not** solve worldwide
selection/residency or eliminate individual shape work in the far field (#844).

Each primitive retains its source and own part ordinal. Its planes use the
existing convex roof/bottom/wall construction, without a new shape approximation.
The unknown native original Part/Face IDs stay -1. The analytic own boundary is
an independent contract: first plane bottom, subsequent ring-edge walls, final
roof planes. Source/part and plane owner are stored with every diagnostic hit.
A native/analytic part equality claim is impossible where the native ID is absent.

Material tables contain original source frame, floor/seat/foot/deck heights,
ring-edge origin/vector, bay count, normal/gable category pair and metre-use flag.
Native BuildingSurface/FacadeUv/BuildingWallMaterial formulas determine these
values; no observed vertex UV is used to infer a missing recipe. Bottom/roof
categories retain the original Ident. Wall Gable is evaluated at the analytic
hit height relative to the original deck; entrance becomes back above the deck.
A native mesher's original patch context at the eave is not thereby recovered.
Party walls suppress bays, and glazing suppresses masonry metre coordinates.

**Category never passes through a recomputed packed U.** Categorical material ID
and continuous bay/height coordinates are separate outputs. A0.2mm known boundary
case in the tests changes a repacked floor(U/256) category but leaves this product's
explicit ID unchanged. This is the explicit research contract, not a newly agreed
GPU binding. The ordinary native shader ABI and its existing data remain unchanged.

Original reference input follows `litVertex.glsl`: derive flat category and mod
at each original vertex, then interpolate coordinates/metres/RGBA/normal at the
original hit. The flat category must agree among all three original corners.
Do not interpolate packed U and then silently treat its quotient as a flat ID.
The original full geometry/source IDs and the two paths' own hit points are used.
This now includes normal quantization/interpolation, colour and UV differences
that were deliberately excluded from the earlier geometry-only comparison.

Unsupported shapes retain original vertices, packed normals, UVs, metre fields,
RGBA when present, index order, bindings and original global triangle IDs. Source
colour-factor recipes without an analytic colour contract also retain original
fallback. A fully unsupported scene is supported, not an empty-product failure.
The reader optionally preserves RGBA in `native_capture_meshes.fields_at`; existing
RGB callers keep their default output. A known nonwhite/alpha fallback regression
fails with the former RGB-only path and passes with the RGBA path. Actual supplied
cell colour factors are absent/white. Retaining alpha does not implement native
opacity or coverage: this experiment still traces opaque geometry point samples.

## Comparison, cost and limits

The driver processes all120actual native static/moving poses, unchanged1280x720
projection and focal length,2x2samples in complete cell ROIs. Candidate bounds
join original extents so approximation growth cannot disappear outside the crop.
Offscreen poses are reported. Coverage/depth, source changes, category changes,
all-common normal angles and same-source/same-category continuous field errors
are separate. Unknown original parts are counted rather than inferred. Raw NPZs
contain all hits and fields, including errors; no tolerance/post-hoc mask forces
agreement. Sixteen exhaustive original triangle-ray controls per visible pose
check the original intersection path independently. Existing explicit-solid ray
controls and stale-history/fallback tests remain available and are run unchanged.

The material-ID and normal PNGs include the usual actual static/moving poses and
the worst categorical/coverage pose if different. Open both locally. ID hash
colours and normal RGB are diagnostics, **not** actual PBR material/shaded images.
The archived geometry input does not supply actual native draw material bindings;
PBR/texture/derivative/opacity/TAA/shadow and GPU/device admission remain open.
The geometric errors already measured in #888 are not erased by adding attributes.
A finite camera sequence also establishes no continuous observer-volume bound.

The stored CPU-array payload is594035B, including all material tables, full fallback
attributes/bindings and BVH. Additional decoded fallback normals, vertex-stage
coordinates, flat categories and the white RGBA constant occupy61260B. All are
counted, not hidden in recipe/wire bytes. NPZ compressed size is separate. Original
supplied stored geometry/attributes occupy1028316B; narrowing the same original
indices to u16 gives943302B. This reference deliberately includes that cheaper
exact original alternative. The original native GPU allocation/layout and the
candidate GPU packing/bindings are unqualified; these are actual stored-field/
CPU-array payload comparisons. They are not a total CPU/GPU residency claim.

Assembly/reload/query wall and CPU intervals, traced peaks and whole diagnostic
RSS are recorded. Peak RSS includes both representations, decoded inputs and
validation/output work. Rtree/library acceleration allocations, frame resources,
query arrays and native resources add further memory. Geometry node/plane tests,
material-hit/fallback-field queries, logical record operands and output-array
bytes are explicit. Per analytic hit,195B of declared material/normal record
operands are addressed;48B/box and32B/plane model logical geometry operands.
These operands are neither actual DRAM traffic nor copied bytes. Allocation count,
physical copied/traffic bytes, native times and GPU runtime remain unmeasured.
No ray-versus-native-raster speedup or frame-budget conclusion follows.

SIGGRAPH-first context: Mueller et al.2006, *Procedural Modeling of Buildings*,
local `doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf`;
page3/Figure4 inspected. Only the parametric shape/coordinate idea is used;
current owned Outshine code supplies the actual rules. No CGA code/textures copied.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -m unittest \
  test_attributed_prisms test_prepared_fields test_full_view_reference \
  test_roof_prisms test_two_phase_occlusion
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/compare_attributed_prisms.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-research-attributed-prisms-<commit>
```

Use the existing NumPy/Trimesh/Rtree/Pillow environment and fresh explicit system-
temp outputs. Terminal measured results and the port/next-step decision belong to
#897; implementation details and gate evidence to its PR. No doc/README images.
