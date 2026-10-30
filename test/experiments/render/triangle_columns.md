# Whole-source triangle columns

Owner #1845, parent #1797. This bounded geometric reference supplies the surface
information absent from the first-hit model in #1842. It is not a new renderer,
runtime field format, distance LOD or qualified replacement for the native group.

SIGGRAPH first: the existing *Layered Depth Images* (1998) and *The LDI Tree*
(1999), especially the latter's PDF page 3, distinguish hidden samples, support
and sampling density. The figure/equations were opened. This reference does not
implement their image splatting or octree: it retains original triangles in
projected columns. Intersection uses the barycentric derivation in Möller and
Trumbore, [*Fast, Minimum Storage Ray-Triangle Intersection*](https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/pubs/raytri_tam.pdf),
1997; PDF page 2 equations/figure were opened. No foreign implementation is bundled.

## Ownership and representation

`native_world_inputs.load(..., attributes=True)` optionally retains all verified
original part declarations and their normal/UV/UV1/colour arrays. Defaults and
the existing provenance comparison are unchanged. Three original parts, material
indices 0/1/2, contain 38476 triangles and 78374 vertices from the whole 1558-source
group. No per-face SourceID is invented. A returned triangle ordinal refers to
the unchanged global triangle order; original part counts and vertex offsets
resolve its attribute indices. Attribute arrays remain caller-owned and unchanged
for this synchronous experiment; no live shader/material authority is inferred.

`triangle_columns.prepare` owns immutable copies of original Float32 positions,
UInt32 triangle indices, the Capture matrix and a CSR cell/triangle index.
Every original triangle, including hidden and Capture-edge-on faces, is indexed
by its projected bounding rectangle. Cells retain all depth layers; no sampled
first-hit colour/mask is substituted for a geometric outline. The 32/64/128 square
grids differ in acceleration only: all retain exactly the same geometry.

All original vertices must fit the declared complete Capture volume. Triangle,
vertex and total-reference capacities are checked before CSR expansion; exhaustion
returns no partial product. The original group remains the fallback. This is
generator-side preparation that reads the entire fine geometry, not a cache-hit
or pre-fine-IO selector. Original attributes remain outside the geometric index.

## Query and limits

Select the existing complete Capture-volume World rectangle before expanding
World rays. Traverse its projection using the **whole original vertex depth
range**, not the first-hit depth range. Collect indexed triangles, remove duplicate
ordinals, then test original 3D triangle support and nearest front-face depth.
The existing bounded Capture-cell traversal is reused; it creates no extra views.

Front-face determinant threshold is 1e-12, consistent with the existing CPU
reference. This is not a watertight or certified GPU arithmetic claim. Result
depth is computed with the native recomputed Lens coefficients in binary64,
not GPU-uniform replay. Raw depth remains per sample; order with native terrain
or the opening first, then reduce common 2×2 samples from 2560×1440 to 1280×720.

Cell, reference and cooperative time limits leave explicit unevaluated samples.
A group with any unresolved geometry, unknown material/lighting or missing
runtime qualification keeps its original path. `evaluated` alone never admits
the field. Query source arrays stay unchanged; field geometry is an owned snapshot.
No IO, threads, publication, cache policy or World lifetime is introduced.

## Native finite result

Both poses use the same geometric field. Across all requested rays and all three
grid sizes, masks and original triangle ordinals match the independent existing
Trimesh/R-tree reference. Raw-depth differences are at most 4.24e-21. This finite
CPU agreement does not certify arbitrary scenes or native sample coverage.

Against actual native #1838 samples, each pose still has one mask disagreement,
versus 57/76 for the first-hit cells. Maximum visible-source coverage error falls
from 1/0.75 to 0.25 against terrain; the side opening reaches zero error. The
0.25 cases remain above the unchanged 0.125 research limit. Raw-depth errors on
common native hits reach 2.65e-10/3.29e-10; colour/material rendering is unmeasured.
Four grayscale coverage panels were actually opened. They contain no surrogate
shading filled from native colours. All four original-group fallbacks reproduce
actual native union RGBA and unfiltered sample depth exactly.

## Costs and remaining use

The held-input comparison measures complete geometric queries, including World
ray construction, against the existing R-tree reader. Its original 256-ray batch
and an explicit 32-ray alternative are compared at the same geometry/quality.
Seven CPU rounds and separate Memray allocation traces keep profiling overhead
out of CPU times. Build cost includes the complete index/owned geometry; loading,
source verification, images and native readback precede or follow the scopes.

For the 64-square grid, actual nearest-triangle tests are 23109/246334, after
24170/320513 gathered references and 1595/18953 visited cells. Result payloads are
27200/72675 B. `direct_triangle_pairs` is the hypothetical full Cartesian product,
not measured R-tree work. Its internal tree/candidate operations are unmeasured.

The field's numeric payload is 1695316 B, including 1402200 B copied geometry.
Its immutable snapshot copies that payload at least once; other NumPy copy
traffic is unmeasured. The retained attribute payload adds 3448456 B, giving
5143772 B before Python objects. All original fine topology is still present.
This is larger than the old 327680 B decoded first-hit Capture per pose and is
not a low-memory distant asset winner. The observed R-tree build has 6498096 B
of distinct numeric arrays, excluding its native heap, Python objects and later
query caches; it is not a full residency measure.

There is no universal CPU winner: columns reduce preparation/scoped peak and
slightly improve the near query, while the existing R-tree is faster sideways.
Reducing R-tree batches trades speed for lower temporary peak. Exact figures,
commit/platform/load and the owner decision belong to #1845 and its PR. Peaks
are incremental measured scopes, not process/UMA/device budgets. No native,
frame/p99, load-time or fine-IO/residency improvement follows from these numbers.

```sh
export PYTHONPATH=lib/src:test/experiments/render
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -B -m unittest test_triangle_columns test_building_sample_originals test_native_world_inputs
python -B test/experiments/render/compare_triangle_columns.py --cost --output "$fresh_temp"
```

Existing .venv/NumPy/Trimesh/Rtree; optional `--cost` uses existing Memray. Fresh
host-temp output via experiment_output. No new package, normal pin regeneration
or native code. The reference makes complete-surface coverage/depth measurable;
compact generator surfaces and actual material/live-World qualification remain open.
