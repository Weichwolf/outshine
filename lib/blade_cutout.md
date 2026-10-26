# Authored cutouts on curved blades

`outshine_ref.vegetation.BladeCutout` trims an existing analytical `Blade` with
one authored polygon in `(s,t)` parameter space. `s` is transverse in [-1,1],
`t` longitudinal in [-petiole/length,1]. The carrier's width profile, bend,
twist and linear fold/quadratic cup remain unchanged; this is a cutout of that
surface, not an arbitrary new rectangular leaf carrier.

```python
from outshine_ref.vegetation import Blade, BladeCutout
from shapely.geometry import Polygon

blade = Blade(lobes=0, lobe_depth=0)
bottom = -blade.petiole / blade.length
cutout = BladeCutout(blade, Polygon([
    (-1, bottom), (1, bottom), (1, 0), (0.7, 0.3),
    (0.9, 0.6), (0, 1), (-0.6, 0.7), (-0.9, 0.3), (-1, 0),
]))
mesh = cutout.mesh(along=64, across=3, max_faces=8192, max_vertices=8192)
restored = BladeCutout.from_recipe(cutout.recipe())
```

One finite valid 2D polygon with at most1024 boundary coordinates is accepted.
Holes are supported, while the entire midrib and both attachment anchors must
remain covered. The petiole parameter band must remain complete when present;
zero-width bases and tips require a single terminal parameter point. Existing
`Blade` and `PlantPlan` recipes, statistics and fixed reference rows are unchanged.
The separate `blade-cutout-1` recipe/identity contains carrier and normalized WKB.
It is a trusted authored recipe, not a hostile-input deserializer.

The mesher intersects a finite parameter grid with that domain and uses GEOS
constrained Delaunay triangulation. Shared `geometry.conform_triangles` joins
interfaces. Grid cuts explicitly contain s=0 and t=0, so neither a hard midrib
fold nor the petiole derivative seam is crossed by a triangle. Existing Blade
jets supply per-corner analytical normals, retaining distinct fold-side normals.
The returned owned arrays match Blade meshes: Float64 vertices/parameters/corner
normals, Int32 faces. Only explicit caller mesh demand creates these arrays.

Adjacent polygon clips need consistent precision: an actual valid input otherwise
produced almost coincident seam points and an invalid zero-length conformance
split. GEOS intersection therefore uses the shared binary parameter grid2^-42.
Coordinates move at most its geometric snapping scale; petiole base and terminal
anchors are retained. This is numerical roundoff control, not contour simplification.
The complete parameter coverage is checked against the original domain, with
area difference <=max(1e-18m², domain_area*1e-10). This tolerance is not a physical
surface, silhouette or LOD error certificate.

`along` is an integer2..128, `across`1..16. Caller face/vertex capacities are integers
0..32768, defaults8192/8192; exhaustion returns `None` before curved mapping and
output normal creation. Grid/GEOS clipping and triangulation are bounded by input
and grid sizes, but can precede that refusal; these capacities are not a wall-time
or total allocator/GEOS-RSS bound. Invalid inputs raise; facets inconsistent with
the analytical orientation raise `ValueError`, as for Blade. A retained strongly
lobed counterexample fails at32/48 longitudinal divisions and succeeds at64/128.
Do not silently accept those inverted facets or interpret detail as a general LOD.

## Reference and executable comparison

Local [SIGGRAPH2005 leaf venation](../doc/references/vegetation/siggraph/2005-leaf-venation.pdf),
PDF page2, was optically inspected: Figure1 distinguishes entire/toothed/lobed
margins, and Figure2 distinguishes domain growth/deformation. This supports keeping
the authored contour separate from its surface carrier; no new growth/venation
simulation is claimed. The earlier actual Ghidra exports `SetupSkinForMesh` and
`ComputeAnatomicalAnchor` are recorded in [#378](https://github.com/Weichwolf/outshine/issues/378):
stored indices and optional opposite winding, plus parent-relative anchor traversal.
They do not establish SpeedTree's contour-generation algorithm. No vendor shape,
texture, mesh or implementation is copied.

Consumers under `test/experiments/vegetation/`:

- `cutout_shapes.py`: own deterministic asymmetric entire/lobed/toothed families,
  saved as polygons. These generic controls are not botanically approved species.
- `cutout_experiment.py`: eight organ prototypes on the fixed64-axis/571-leaf rows
  from #367. The old wood/poses/materials remain; new morphology/mesh area and cost
  have their own identity/metadata. Source PlantPlan area does not describe them.
- `cutout_cost.py`: same contour/carrier/grid through vector GEOS clipping/CDT and
  the existing shared projected-height strip builder; fixed domain, capacities,
  complete coverage and orientation requirements. Both use the shared conformance
  algorithm. Finite facet-centroid residuals remain diagnostics, not global bounds.
- `cutout_replay.py`: finished cache read with placement/mapping/meshing forbidden,
  then separately byte-exact one-organ recipe replay. Finished hits load arrays.
- `render_cutout.py`: saved original and cutout leaf under identical light/material;
  `render_tree_entry.py` consumes the unchanged placements and saved new prototypes
  for the whole plant. No showcase server or published research gallery.

Source/wheel controls independently check complete domain coverage/edge incidence,
holes, seam preservation, exact anchors, analytical flat area, differential normals,
identity/replay and budget refusal before mapping. Whole/close1920×1080 Blender
images are optically inspected. Measurements and exact frozen source/product/image
provenance are in the delivery PR. Eight reusable prototypes avoid571 separate mesh
builds, but the chosen fine curved product has more triangles than the old regular
Blade. Its output is not automatically an efficient runtime LOD or forest product.

The visible result is rounder asymmetric leaf margins; the sparse tree architecture
and qualitative materials remain. Natural mature crowns, species/vein calibration,
canopy coverage, wind, native shading and device budgets remain open.

For a fresh local run, activate `.venv`, set
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`, and choose an explicit
system-temp destination. No private input cache is required:

```sh
python test/experiments/vegetation/tree_entry.py --output /tmp/outshine-cutout-base
python test/experiments/vegetation/cutout_experiment.py \
  --inputs /tmp/outshine-cutout-base --output /tmp/outshine-cutout-result
python test/experiments/vegetation/render_tree_entry.py \
  --inputs /tmp/outshine-cutout-result --output /tmp/outshine-cutout-shot
```
