# Bounded row candidates for narrow grass blades — #465

The measured #463 contact query still visits millions of empty bounding-box
pixels. Keep its exact population, directions, finite geometry and contact rule;
avoid those pairs before allocating/evaluating barycentric batches. No world,
material, native, cache-schema or public library API changes are introduced.

## Construction and numerical limits

First consulted locally: SIGGRAPH1998
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
PDF page4 optically opened again. Disocclusion/holes and depth are not arbitrary
fill opportunities. The new scanline interval is our own elementary derivation,
not that paper's warp or an extracted SpeedTree algorithm.

At fixed pixel-centre Y each triangle weight is affine in X: w=A*X+B.
Intersect the three constraints w≥−1e−10 with the original clipped bounding box.
Enumerate only integer pixel centres in that interval. Then evaluate **the same
original** Float64 barycentric expressions, acceptance threshold and depth dot
product as `raster_batch.fragments`; candidate pruning never relaxes acceptance.
Face-index depth ties still choose the same canonical source.

Before solving intervals, inflate pruning by a magnitude-based64ε roundoff
allowance, including division by the determinant; integer interval ends receive
1e−8pixel slack. If the allowance exceeds1e−5, keep the whole original row.
Determinant rejection remains the reference1e−12pixel². Zero derivatives use a
parallel inequality. These are conservative guards for the tested Float64 domain,
not a formal all-input exact-arithmetic or raster-error certificate. Explicit
nearly degenerate controls and real west-view cases exercise unpruned rows.

At most32768 triangle/row records per batch; candidate pixel batches respect the
same explicit pair budget (262144 in this study). A row has at most image-width
pixels, so the existing minimum size² budget covers every individual row/triangle.
Image size is bounded at1024. The original final membership test discards all
extra interval slack. The instance-query default selects rows; `sampler="boxes"`
retains the exact comparison. Sampler choice does not change product identity.

## Same-quality measurements

Original saved source:2368 roots, eight shared graminoid forms,1×1m ground
footprint inside4×4m neighbouring support, identical to #463. Both paths perform
the same preceding root-box selection and expand278/417/430 roots. Three
alternating trials per method/profile, one numerical thread; medians include
root selection/expansion and contact construction, exclude IO and independent
GEOS/ray checks. No device/frame claim follows.

| View | Grid | Box / row wall ms | Box pairs → row candidate pairs |
|---|---:|---:|---:|
| Top |256²|150 /91|827394 →31076|
| East |256²|592 /139|5171694 →166839|
| West |256²|705 /152|5719565 →177024|
| Top |512²|498 /165|2893826 →124283|
| East |512²|2055 /276|18777763 →669569|
| West |512²|2328 /296|20779024 →711921|

The512² row methods also inspect202381/223951/251547 triangle/row records.
Accepted fragments remain122308/644248/685772; the west path deliberately keeps
10 numerically uncertain rows unpruned (five at256²). Candidates and true
fragments must not be conflated. Source IDs, heights, normals, coverage and every
repeat match the box reference exactly;256² also matches the original saved data.

For east512², scatter-grid initialization drops301989888→29360128B over72→7
batches. Selected expanded geometry remains2401920B, projected vertex copy1200960B
and canonical face keys400320B. Each512² contact output11534336B plus13981000B
of moment mips is reference data, not a proposed shipping format. The grid data
cost quadruples versus256² even though construction is faster; no free quality
or world-scale per-square-metre cache is claimed.

Valgrind3.24 DHAT, two equal single east512² processes, same input/output hash:

| Whole-process native heap statistic | Box | Row |
|---|---:|---:|
| Allocated blocks |84775|78739|
| Cumulative allocated bytes |4441889393|882342901|
| Live heap at global peak, bytes |101151325|98456207|
| Tracked heap reads, bytes |21972581210|2748461439|
| Tracked heap writes, bytes |11173936023|1410310721|

These include Python/NumPy imports, cache decoding and one query; not query-only
allocations, total system traffic, stack/GPU traffic or process RSS. Python arena
suballocations are not separately counted. Both profiled processes terminate0;
their hash1854cbf88617dbd2b4ece9568b0065687fc577ac3d830fdd59fed5b488c510ae
matches. Instrumented time is not used as an ordinary runtime benchmark.

## Correctness, actual images and reproduction

21 source/installed-reference controls pass: boundary/random/reversed triangles,
long thin/horizontal/vertical/almost-degenerate geometry, empty/offscreen cases,
bounded partitions, exact weights/depth and canonical ties, plus previous grass
contracts. Four new controls compare complete sorted fragment records, not only
mean coverage. No new wheel is claimed; the library is unchanged.

All786432 independently checked512² GEOS memberships agree;192 independent
Float64 rays agree in hit state/height. Same preset1percentage-point area criterion:
top/east/west errors0.0163/0.0925/0.0090percentage points. GEOS retains the explicit
1e−12m precision of #463. Mip means remain within1e−13. The fresh installed cache
reader forbids root/shape/vertex/raster reconstruction and succeeds on all views.

Actually opened1920×1080 top/east Blender pairs preserve the complete physical
left half pixel-for-pixel against #463. The cached right half at512² retains finer
blades and less enlarged-grid blocking than256², still with visible aliasing.
This is unlit fixed-direction coverage, not natural grass lighting, parallax,
moving LOD or accepted near detail. Software EEVEE/llvmpipe, four samples,
23.02s wall/23.56CPU-s/1993.2MiB process RSS under concurrent DHAT load.

Frozen comparison5c4ea812f:51.66s wall/51.64CPU-s/324.9MiB RSS including six
three-trial comparisons, GEOS,192 rays and IO. Ordinary comparison ran before
the allocation profilers. Full commands/receipts/originals are linked in the PR.
Use `.venv`, existing dependencies and explicit temp output paths:

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation .venv/bin/python -m unittest raster_rows_checks grass_projection_checks test_ground_cover test_graminoid
.venv/bin/python test/experiments/vegetation/raster_rows_experiment.py --inputs <original-463-products> --output <fresh-temp>
.venv/bin/python test/experiments/vegetation/raster_rows_profile.py --inputs <original-463-products> --sampler rows
```

This closes the measured empty-pair bottleneck in this study. Cold direct coarse
world construction, correlated light/view masking, actual botanical grasslands,
wind/motion, silhouette/parallax, angular interpolation and native/GPU/LOD
acceptance remain open. Material/Light ownership and #451 are unchanged.
