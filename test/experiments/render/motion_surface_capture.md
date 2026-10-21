# Intrinsic coarse cell and bounded multiview controls (#1045)

The current exact-eye capture cannot fill a moving world cheaply. This experiment
keeps one complete coarse Shell cell independent of the camera and compares it
with two projectors, each retaining the first two front-facing depth layers.
It uses the existing complete dense native input and existing ray/field readers.
No exporter, native implementation, cache format, GPU binding or runtime changes.
Initial comparison producer`b854ba13b`; final intrinsic-selector producer
`ece8be9f7`. The full repeat preserves all prior output buffers and six panel hashes.
Master owns intrinsic Fine/Shell/Massed production and finite cache identities.

## Result and decision

Keep the existing intrinsic coarse geometry as the moving-world fallback.
One camera-independent **tangent-frame cell box** selects exactly the same
triangles as the tested32source groups in all123poses. It needs120bytes of
Double axes/bounds and one box test instead of32. An ECEF AABB also selects this
offscreen cell at two additional poses; orientation matters in this finite case.
The32-group split removes no triangles in any visible pose and is not a useful
additional camera hierarchy inside this cell. This says nothing against useful
larger spatial hierarchy or shadow/light patch granularity (#1048).

The two-view/two-layer selection is **not an admitted moving-world replacement**.
Even generous whole-original-triangle and whole-source closures lose coverage and
change native source/material codes. This is a bounded geometric capture control,
not a new LDI texture renderer. It separates selection loss from resampling/filter
errors before constructing another sampled representation. Other multilayer/
multiview methods, adaptive fallback and global hierarchy remain open.

## Inputs, products and scope

`building-full-view` retains576sources,14169Shell triangles and28672native vertices
of one complete Tokyo cell at5.60–5.92km. All directions/hidden surfaces remain.
`full_view_reference.load` verifies the four original file hashes and compares
geometry bytes/source ranges against the original native archive. No network or
private source cache is needed. Source/Part/Face unknowns stay unknown.

The two projectors look at this cell from the actual static eye and actual6m
moving endpoint. Both use the native focal length/1280×720profile and2×2samples
in the full integer-aligned ROI. Rays retain two distinct nearest front depths;
exact equal-depth duplicates keep the first original triangle. Distinct depths
are never merged by an epsilon. Each projector has1880rays;153have more than two
front layers. All triangles touched by either retained layer form one product.
The second control retains complete Shells of those same captured sources.

Products preserve original Float32 positions, UV/category payloads, packed normals,
metre fields and RGBA; bit-identical full-record sharing only. No vertices weld
on position alone. Uniform white RGBA/zero metre fields can be implicit. Lineage
and original triangle IDs remain separate. Files are saved and reloaded before
querying; query code consumes only stored arrays. Uncaptured source IDs/reason
are reported, and the complete coarse product still owns every source.

The intrinsic box is cooked once from the body and its fixed ECEF anchor. The
geocentric tangent frame handles both poles, contains every supplied vertex,
and never depends on a camera/projection. Query transforms only view metadata
and uses the existing box/frustum test. Float64 padding is an engineering guard,
not an interval-arithmetic or all-input culling theorem. Native precision and
producer/cache integration still require their own checks. Coarse products and
simulation/caster/room needs are independent of the camera draw cut.

## Actual image and independent checks

The unchanged120native static/moving poses plus−30m,+6m,+30m lateral controls
give31visible/92offscreen cases. All preserve1280×720and2×2subpixel rays. Complete
coarse coverage, depth, source/code, normal, material coordinates, metres and RGBA
match exactly in every visible case. The123intrinsic cell decisions match the
32-group primitive sets exactly; all14169triangles are selected when visible.
There is no source regeneration or new geometry for changing these poses.

| Control | Sources / triangles | Stored core bytes | Diagnostic lineage bytes | Worst lost quarter-pixel samples | Worst changed material codes |
|---|---:|---:|---:|---:|---:|
| Complete coarse cell |576 /14169|1002518|396732|0|0|
| Whole captured triangles |134 /329|29302|9212|137|78|
| Complete captured-source Shells |134 /3239|230474|90692|23|21|

Capture coverage L1 reaches34.25/5.75pixels respectively; source changes reach83/21.
These aggregate sample measures **are not** the native continuous1px geometric
error bound, and no new RGB/material tolerance is invented. Both smaller products
remain unqualified. Lost image samples are not merely different coplanar triangle
IDs: all disputed geometry rays plus regular controls receive independent exhaustive
Moller-Trumbore checks. In the first complete run4456selected rays produce
63137064/63137064/1466024/14432984pairs for original/coarse/faces/source controls.

Six panels at the actual worst coverage/code poses and original diagnostic poses
are opened locally. They magnify the native ROI with nearest-neighbour display;
1536×500panel size does not change the measurement profile. Two Lambert lights
show real silhouette/ownership differences. These are opaque diagnostic fields,
not native PBR, texture/opacity/TAA, shadows, Fine→Shell error or full-world images.
A fresh process forbidding original-fixture opens replays the stored products at
static026 and matches every hit/field byte, including the original discrepancies.

Nine focused checks cover the full archived native byte replay, complete disjoint
source groups, field discontinuities/signed zero, implicit constants, exact depth
ties, overflow, empty/malformed rays, an independently checked third-layer side
disocclusion, polar frames, containment and all123intrinsic-box decisions.

## Costs and limits

i5-1135G7/Linux, one BLAS thread, normal priority/shared host. Three fresh metadata
processes, seven alternating123-pose loops each: grouped process medians
19.668/20.756/20.847ms versus intrinsic-cell12.975/14.388/12.289ms.
Box tests3936→123; traced query peaks29744→4995bytes. Source geometry is untouched
by these queries. Concurrent complete reference queries/shared load are declared;
none of these Python intervals estimates device/frame time.

The actual group file has58344array bytes:1668bounds/offset header plus56676original
face-membership bytes. A native contiguous-cluster layout can avoid that membership
list; do not advertise58344→120as a native saving. The matched header comparison
is1668→120bytes. No all-input/global optimum is claimed for the cell frame.

The **cheaper exact native control** uses943302geometry/attribute bytes with u16
indices (1028316with u32). The coarse diagnostic adapter pads59216metre bytes on
roof vertices, explaining its1002518core bytes. Full-attribute sharing does not
reduce the28672original vertex records here. These NPZ layouts are not proposed
product packing. Lineage, decoded Double normals, shader-stage copies and ray
indices add memory; all are reported separately. Keeping a capture alongside
the full coarse fallback **adds** bytes, not a net world-memory saving.

The first two capture query phases take2.349/2.200s after reference-index setup;
source verification/decode0.714s, coarse record construction23.534ms, grouping
126.255ms. These are one normal diagnostic run, not stable device benchmarks or
native cook costs. Separate traced peaks: coarse sharing5581179bytes, grouping
1003459bytes, first layered query20297059bytes including newly populated input
index caches. Full-run RSS294207488bytes includes original/all candidates/derived
fields, ray controls and outputs; it is not candidate or app residency.
Total allocation count/physical copy traffic, native IO/upload, p99/GPU/world
budget and native async readiness remain unmeasured. No ray-versus-raster speedup.

## Reproduction

Existing NumPy/Trimesh/Rtree/Pillow/Shapely environment; no new package. On a clean
frozen checkout set `OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1` and
`PYTHONPATH=lib/src:test/experiments/render`. Run
`python -m unittest test_motion_surface_capture -q` and
`python test/experiments/render/compare_motion_surface_capture.py --input test/outshine/data/render/building-full-view/cell.json.gz --output /tmp/<fresh-directory>`.
All original products/rays/reports/panels and private replay/metadata/tracing
reproducers remain under `/tmp/outshine-research-building-motion-933e89cd0/`.
No versioned code depends on that private output path; no pictures are published.
The runner resolves system-temp roots on Linux/macOS and refuses checkout outputs.
Peak RSS records host units correctly: bytes on macOS, KiB converted to bytes on Linux.
Sparse zero-metre fields are omitted only for all-zero words; signed zero remains exact.

SIGGRAPH-first: [Layered Depth Images1998, p3/p5/Fig3–4](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
optically inspected for depth-layer/disocclusion context. Existing whole-view,
vertex-stage field, BVH and ray readers are reused. No paper implementation,
texture renderer, ordering proof or GPU performance claim is imported.
