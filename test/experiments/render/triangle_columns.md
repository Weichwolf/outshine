# Whole-source triangle columns

Owners #1845 and #1858, parent #1797. This bounded geometric reference supplies the surface
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

### Whole-source support before ray expansion (#1858)

Preparation also owns a 24 B immutable Float32 min/max box of all original
positions. Each query transforms its eight corners. For positive homogeneous w
throughout this box, a linear-fractional projection reaches its extrema at box
vertices. Outward rounding after every binary64 multiply, sum and division
encloses the projected box without a pixel epsilon. The y inversion and pixel
conversion use the same interval treatment. Fraction-based tests check exact
rational projections of the given binary floating-point inputs.

Only sample rows inside that enclosing rectangle expand rays and traverse cells.
Outside rows are known empty for this source; complete result arrays retain their
original order and size. Unknown or nonfinite projection, including a box touching,
crossing or behind the eye plane, keeps every sample on the previous path. Inside
rows still obey existing cell/reference/time limits and remain unknown on failure.
This support bound does not certify native raster rounding or the floating-point
ray-triangle algorithm. It adds neither dilation of rendered geometry nor runtime
field admission. The explicit `whole_support=False, pair_batch_size=0` control retains
the complete previous query; it is not a second product representation.

There is no repeated scan of all vertices in the query, extra BVH or new cache.
Source min/max is built once with the already owned geometry. Current callers
and source-hash manifests include this helper; material/corner bindings stay intact.

### Complete-ray intersection packets (#1858)

`triangle_pair_batches` borrows the same geometry/rays for one synchronous query.
Greedy packets contain complete candidate lists, with 4096 pair slots by default.
Two fixed staging arrays occupy 49152 B; physical distance/owner/unknown scratch
uses 17 B per selected ray. Candidate lists are copied into staging, not retained
as an unbounded queue or expanded into the complete ray/triangle Cartesian product.

One ray is never split between packets. Its `q @ direction` uses the same complete
matrix/vector shape and operation as the scalar reference. Flat `einsum` and
one-pair matrix products had changed silhouette coverage; the existing strict
pose/silhouette tests reject both. No tolerance or source geometry was changed.
Other vector operations and reductions span packets; equal finite distances choose
the lowest original ordinal, as in the sorted scalar candidate list.

A ray larger than the packet runs separately with its complete candidate list,
bounded by the unchanged 131072-triangle field preflight. Thus 4096 is a packet
limit, not a universal scratch ceiling: a separate ray can require larger temporary
arrays. Counters record these calls, largest kernel, row-ID bytes and gathered
payload. Field input limits bound every array; no all-query pair expansion occurs.
Only the configured packet and one standalone ray can be live at once.

Reference/time exhaustion flushes already submitted complete rays; the remaining
samples stay unknown. As before, time is cooperative between rays; a final packet
may complete after the check. Nonfinite candidate arithmetic marks the whole ray
unknown and keeps the original fallback. Query scratch dies with the call; full
output arrays retain their size/order. `pair_batch_size=0` retains scalar arithmetic
as a comparison control. No IO, new worker, cache policy or native format is added.

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

The field's numeric payload is 1695340 B, including 1402200 B copied geometry and
the new 24 B source box (the #1845 measurement preceded this box).
Its immutable snapshot copies that payload at least once; other NumPy copy
traffic is unmeasured. The retained attribute payload adds 3448456 B, giving
5143796 B before part metadata and Python objects. All original fine topology is still present.
This is larger than the old 327680 B decoded first-hit Capture per pose and is
not a low-memory distant asset winner. The observed R-tree build has 6498096 B
of distinct numeric arrays, excluding its native heap, Python objects and later
query caches; it is not a full residency measure.

The frozen #1859 same-run comparison reduced ray rows from 1600/4275 to 432/792
and visited cells from 1595/18953 to 527/12780. Geometry outputs, raw attributes
and the existing native coverage errors remain unchanged. Query CPU measured
109/282 ms versus 205/492 ms for the unbounded control; allocation events and
incremental peaks also decreased. R-tree256 remained about equal sideways, so
there is still no universal method winner. No frame/IO/UMA claim follows.
The exact qualified commit, counters, timing scopes, load and owner acceptance
belong to that delivery PR. The second #1858 comparison qualifies whole-ray
packets against the supported scalar path, 512/4096/16384 slots, unbounded scalar
and both R-tree sizes. It preserves complete depth/ordinal/raw-attribute arrays
and all eight diagnostic PNGs. The 4096-slot choice reduces CPU and allocation
events on both finite poses, at a larger incremental peak. The 16384-slot option
is somewhat faster but needs substantially more peak memory; 512 slots regress
side-query CPU. Exact tradeoffs/qualified commit live in the second delivery PR.

The original #1845 comparison had no universal CPU winner: columns reduced
preparation/scoped peak, while the existing R-tree was faster sideways.
Reducing R-tree batches trades speed for lower temporary peak. Exact figures,
commit/platform/load and the owner decision belong to #1845 and its PR. Peaks
are incremental measured scopes, not process/UMA/device budgets. No native,
frame/p99, load-time or fine-IO/residency improvement follows from these numbers.

```sh
export PYTHONPATH=lib/src:test/experiments/render
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -B -m unittest test_triangle_pair_batches test_triangle_query_support test_triangle_columns test_source_fragment_inputs test_attribute_words test_vertex_colour_palette
python -B test/experiments/render/compare_triangle_columns.py --cost --output "$fresh_temp"
```

Existing .venv/NumPy/Trimesh/Rtree; optional `--cost` uses existing Memray. Fresh
host-temp output via experiment_output. No new package, normal pin regeneration
or native code. The reference makes complete-surface coverage/depth measurable;
compact generator surfaces and actual material/live-World qualification remain open.
