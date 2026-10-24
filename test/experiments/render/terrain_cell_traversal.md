# Exact terrain-cell queries — #1237

This finite Python comparison uses the same unchanged fine source triangles for
all ray methods: full triangle scan, flat cell-box scan and a level-batched
implicit quadtree. It is a geometric query experiment, not a new native renderer
or asset format. The current native 0.5-pixel geometry-LOD allowance is unchanged.

`prepare(vertices, faces, side, strategy=...)` owns an immutable `Mesh` snapshot.
The supported source is a power-of-two cell grid, at most 33×33 vertices, with
the exact existing fine cap index order. Optional outer faces must be the complete
existing indexed skirt layout, including original endpoint identities and exact
XY positions. Unrelated extra sheets are refused. No welding, replacement plane,
bilinear leaf surface or new skirt geometry is introduced. Actual skirt heights
are preserved and enclosed in their boundary cells at every hierarchy level.

Tree metadata stores outward-rounded Float32 Z intervals for the 341 internal
nodes of a 32×32-cell page: 2728 bytes. Leaf heights come from the source. XY bounds
use actual node-corner coordinates plus a two-component residual enclosure audited
against all fine vertices of every node. This also encloses nonaffine/sagged source
geometry and small coordinate-evaluation residuals. Plane subtraction/division in
slab queries is rounded outwards with `nextafter`; no physical height, source-error,
LOD or intersection tolerance is relaxed. The CPU construction is not a proof of
GPU arithmetic or outward shader bounds.

`sample(model, origins, directions, ...)` returns the first accepted one-sided
triangle hit with `t_min < ray_t <= t_max`. `ray_t` is a parameter; it is a metric
distance only for unit directions. Exact numeric ties select the lowest original
face ID. Output barycentrics refer to that same source/version. Misses have
`valid=False`, face −1, infinite parameter and NaN barycentrics. No fake zero depth
or highest-surface selection is used. The three methods share the same triangle
kernel; analytic controls and the independent perspective raster supply separate
correctness checks.

Traversal carries bounded ray/node batches down through the implicit hierarchy.
It tests every admitted leaf candidate and reduces actual hit parameters, so it
does not assume the first visited cell owns the first hit. No fixed step count or
coarse-height heuristic substitutes for an unresolved intersection. Unlike the
paper's incremental ray stepping, this batched traversal needs no re-ascent after
a failed leaf. Its worst case remains linear in all cells per ray; no universal
logarithmic complexity or global optimum is claimed.

Defaults: at most 16384 input rays, batches of 64, 262144 pairs per bounded stage
and 67108864 combined triangle/node tests per call. Model inputs are bounded to
4096 vertices/faces. Budgets are explicit work/capacity limits; cancellation is
checked between batches and hierarchy levels, not inside a NumPy operation.
Failure raises before publishing any result; it never becomes an ordinary miss.

## Fixed real input and independent reference

`test/outshine/data/terrain/native-cell-page/` pins one actual existing cached page,
its original 4900 height bytes, exact fractions and provenance. Extraction reads
SQLite in read-only mode, checks package sizes/CRC, decodes the bounded existing
Zstd package and checks the supported version-5 region/page prefix. It does not
independently revalidate trailing classification/network/geometry products.
The pinned page is selected by maximum page zoom, then distance to the original
region anchor, then original index; virtual status and source zoom remain explicit.
The producing commit/recipe is unknown and is not reconstructed from the key.
This is not the current A18 Koerbersee capture or a fresh source acquisition.

`terrain_cached_page.py` reproduces extraction given the existing cache/key and
an explicit output. It requires the already used system `libzstd`; no new Python
package is added. The experiment itself depends only on the committed fixed page.
Changing source data requires a separate explicit input decision.

The existing `page_geometry` reference evaluates decoded native words, fractions,
tile-local WGS84 coordinates and sag in Double. The full original outer skirts
remain in the common source. This is ideal CPU geometry, not captured GPU varyings
or Float32 raster equivalence. The generic test camera derives from that page's
bounds; no place-specific optimization enters product code.

The independent `current_visibility.raster` renders the same full triangle mesh.
The driver refuses near-crossing geometry for this raster check. Pixel-centre,
opaque, single-sided depth and complete skirts are compared; no MSAA, alpha,
material, normal, shadow, temporal, motion or full-world admission follows.
Small diagnostic dimensions are explicit and do not replace the 1280×720/60 target.

## Cost and decision boundary

```sh
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/render/terrain_cell_traversal_experiment.py \
  --width 128 --height 72 --repeats 8 --render \
  --output /tmp/outshine-terrain-cell-traversal
```

Per-method source snapshot/preparation is separate. One first and eight following
queries rotate method order, giving equal start-position counts across the nine
invocations. Wall/process CPU are recorded together; timer overhead is reported
without subtraction. Preparation and query traces are separate calls after GC.
Live traced blocks are not cumulative allocations. The independent raster's single
validation invocation is timed separately, not included in the rotated ray series.

Counters include tested nodes, actual triangle/skirt tests, largest pair batch and
selected corner/triangle/boundary-height gathers. They are not all NumPy copies,
total allocations, texture reads, GPU invocations or hardware memory traffic.
Traced peak/RSS and selected array bytes remain distinct. RSS before export/plot
includes all three models, inputs, outputs and raster diagnostics. Common owned
mesh buffers in this CPU reference are not a new proposed native resident format.
The tree's additional 2728 bytes/page would alone total 44343640 bytes for 16255
identical qualifying pages: an arithmetic footprint, not measured UMA residency.

Reducing brute-force ray work is not evidence of a faster native raster pipeline.
The native renderer already selects geometry LODs and visibility; this experiment
uses the full fine surface. Current projection bounds, real shading, global page
traversal, coverage/camera motion and complete GPU costs need their own comparison.
The current implementation also reads source coordinates during traversal: it
does not yet prove metadata-only selection before Fine-IO. No native rendering
port is recommended from a single-page result. Measured results, actual image
inspection and the next decision belong in the PR/owning issue.

## Primary references

SIGGRAPH 2000 [Relief Texture Mapping](../../../doc/references/geometry/siggraph/2000-relief-texture-mapping.pdf)
was reviewed first, including its forward-warp/outlier and single-layer limits.
It is not a drop-in heightfield/world contract.
[Maximum Mipmaps, Tevs/Ihrke/Seidel, I3D 2008](../../../doc/references/terrain/i3d/2008-maximum-mipmaps.pdf),
especially pp. 3/5 and their diagrams/algorithms, motivates hierarchical height
intervals. Our leaves retain original triangles, instead of the paper's bilinear
patches. Its LOD heuristic and historical GPU timings are not adopted as Outshine
error/performance guarantees. Source: [author's project page](https://projects.tevs.eu/www.tevs.eu/project_i3d08.html).
