# Window selection against complete closed-window images

[#1075](https://github.com/Weichwolf/outshine/issues/1075), under #33/#1052.
The earlier demand experiments compared identical requested subsets. This comparison
instead uses **all existing closed windows** in the same building as its finite
image reference. It finds and fixes a real ownership defect, rejects a fixed 5 m
radius as a general image rule, and exposes the separate solar-shadow requirement.
It does not qualify a global LOD policy or a native renderer.

## Ownership correction before comparing images

Paired-wing decorative piers accidentally carried their neighboring window ID.
`WindowDetails` consequently removed the pier together with the flat window,
although the closed frame/pane/sill recipe does not replace that pier. In the
original 297-window source, all-window selection removed all 594 pier triangles.
They now have the derived owner `window.identifier + "/pier"`; original logical
window/source IDs remain intact. No mesh construction, dimensions or material
filter changes. The legacy polygon/batched-coverage comparison canonicalizes both
historical and derived pier owners to the same original window for comparison.

Across ten original plans, all 60 coarse mesh fields and numerical triangle-owner
arrays remain word-identical to #1072. Only 1,553 paired-wing owner names change;
all 11,744 historical material/depth coverage groups remain identical. The original
594-triangle omission is preserved as a saved counterexample. After correction,
all non-pier triangle positions/normals/UVs/materials/extents are word-identical to
that saved full draw. Actual before/after images were opened; vertical piers now
survive refinement, partial selection and release. No tolerance or oracle relaxed.

## Finite selection and picture controls

Three versioned provider inputs: original two-court cascade, concave paired-wing
and compact cascade, with 252/297/91 windows. Near eyes come from the existing
explicit-demand driver; distance is increased fourfold for the second pose, with
the same target/FOV. Every image is 1280×720. Compare all closed windows, the
existing closed-sphere/AABB 5 m request, and camera-plane AABB candidates.
Requests are computed from logical plans/metadata **before** any detail construction.
The camera control has no occlusion, backface, size, shadow or reflection policy.
Its binary64 guard is a finite research control, not a universal native bound proof.
Actual omitted closed triangles are independently clipped in the tests.

Complete original coarse geometry remains available in every strategy. Capacity is
explicitly raised to the existing maximum so this quality comparison silently drops
no requested details. Full reference geometry is generated once per source and reused
at the far pose; candidate updates generate only entering IDs. No floors or full
physical-building/interior model is asserted. No existing runtime default changes.

Camera, diagnostic object name, materials, exposure, world/sun and ground span are
matched. Same-renderer Eevee/16-sample/software-llvmpipe comparisons measure output
error of these recipes, not independent physical truth. A second sun azimuth is
used for the court distance view. Full references are repeated on both light sides;
both repeats are pixel-identical. The main dataset has 23 actual renders, plus two
pre-fix ownership images and two explicitly separate no-solar-shadow controls.
Comparison sheets and critical native-size images were opened. Nothing published.

## Image result

Number of pixels whose maximum display-RGB8 channel error exceeds 1/255, out of
921,600. This is a measurement, not an invented acceptance threshold or linear
radiance bound. `test/scripts/pixels.py` crosschecks the NumPy/Pillow statistics.

| Input/pose | Windows all/radius/camera | Radius error pixels | Camera error pixels |
|---|---:|---:|---:|
| Courts near |252/5/140|3396|0|
| Courts distance |252/0/183|112523|0|
| Concave near |297/8/37|2051|209|
| Concave distance |297/0/91|54743|1838|
| Compact near |91/6/41|1610|5492|
| Compact distance |91/0/60|85277|138|
| Courts distance, opposite sun |252/0/183|49701|9|

The radius loses clear frame/sill depth and shadows at distance: court distance
mean absolute RGB8-channel error is 4.641217/255, maximum 151/255. The camera
candidate is pixel-identical in two of seven normal-lighting comparisons only.
It is not monotonically closer than the radius: compact-near camera error exceeds
radius error. More visible fine products alone do not establish shading quality.

Compact-near camera selection retains 41 rather than 91 closed windows and loses no
potentially visible fine triangle in the independent clipping check. Nevertheless
its solar-shadow image differs on 5492 pixels by more than 1/255, maximum 33/255.
With the **same two geometries** and only sun shadows disabled, the images match
exactly. This isolates dependence on this renderer's solar-shadow path; it does not
identify each physical caster versus shadow-raster/filter effects. It is an explicit
diagnostic, not a candidate quality profile or a repair by removing shadows.
Changing the court sun side likewise changes exact equality into a small residual.

Decision: keep explicit-ID construction and metadata query, but do not admit a
fixed radius or camera-bounds-only selection as a general image/LOD rule. Native
adoption needs separate shadow/reflection/functional demand, coherent fallback
and measured transition error. These data supply a concrete reference/counterexample
for that work; no extra capture hierarchy or renderer implementation is introduced.

## Work, retained arrays and scoped cost

Geometry/cost producer `e9c0a4e64dc9a72ba6434c4e73911f07c887bbb4`, based on #1072
`7f3f98f26` with Master `ee7bbd775`. No-shadow renderer producer `54f8ef843` adds only
the diagnostic light switch. Delivery includes Master `15a948e18` through updated
#1072 `bf9a441fa`; intervening changes are native/AGENTS only and all measured
Python sources remain identical. Three fresh ordinary cost processes and a separate
allocation trace produce identical requests and arrays. Linux/i5-1135G7 shared host,
Python 3.13.5/NumPy 2.5.4/Shapely 2.2.0/GEOS 3.14.1, one BLAS/OMP thread. Fixed method
order and shared load make timings indicative; exact entering-window work is stronger.
Render generation/times are separate from these CPU stages.

| Input | New windows full/camera over both poses | Detail-update median ms full/camera | Far retained detail B full/camera |
|---|---:|---:|---:|
| Courts |252/183|1268.24/900.48|7846272/5697888|
| Concave |297/91|1343.15/376.24|8256600/2529800|
| Compact |91/60|478.07/291.98|2833376/1868160|

Camera updates add 43/54/19 windows after their initial 140/37/41. This measures the
existing per-window manager, not the cheaper complete batch or a whole physical
building. #1074's template-sharing optimization is not combined with these timings.
The much cheaper radius has different image quality; it is not a same-quality winner.

Common coarse arrays are 1507594/1733052/617438 B, with 149–321/205–361/140–197 ms
construction across methods/processes. Logical preparation and metadata/selection
are separate stages. First ordinary metadata acquisition takes 1.16–1.80 ms, combined
selection controls .83–1.06 ms/pose. Incremental traced detail peaks full/camera are
8419983/4696095,9003566/1655238,3068398/1404908 B; native heaps are incompletely traced.
Whole-process RSS 168.4–174.1 MB holds all controls/imports/plans/diagnostic copies.
It is not app/device residency or a DB/IO/upload/UMA-budget measurement. Snapshot
materialization/hash/export and saved reference geometry are diagnostic work outside
update timers. Retained/materialized array lengths and scoped peaks are measured;
all internal allocation/copy traffic is not counted. No end-to-end loading-cost admission.

## Verification and reproduction

21 focused geometry/demand/image-control tests pass (88.292 s), including the existing
full-batch checks, all-window ownership preservation, exact extent predicate and
independent actual triangle clipping. Six focused #1074 consumer checks also pass
(8.812 s), with its frozen helpers and the corrected common coarse owner. No source
or window identities changed to obtain a pass. All four cost workers terminate 0.
A fresh saved reader, original-source opens forbidden and generators absent, matches
432 array fields across processes; requests, dtype/shape and all words agree.

Use existing `.venv`, NumPy/Shapely/Pillow and bpy; no new dependency.
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1` and
`PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets`.
Run `python -B -W error -m unittest test_window_detail_demand test_window_extent_query test_window_detail_images -q`.
On frozen sources, `window_detail_images.py --output <fresh-system-temp>` emits the
18 scene jobs; repeat twice and separately `--trace`. Render each geometry with
`render_window_details.py --geometry <scene-geometry.json> --output <system-temp>`.
Add `--opposite-sun` for the court distance trio and repeat its full references.
Use `--no-sun-shadows` only for the separately labeled compact-near diagnostic pair.
The normal bpy renderer logs its existing API deprecation; one preliminary invocation
with global `-W error` failed before producing an image. No warning filter was added.
The existing post-render teardown workaround remains explicit, not claimed repaired.
Private raw reports, inputs, render/analysis reproducers, hashes and receipt are linked
from the PR. Pre-fix reports remain counterexamples, superseded for corrected costs.

SIGGRAPH-first: [GPU-Driven Rendering Pipelines, 2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
pages 13/28 read and visually inspected: camera and shadow passes are separate.
No GPU algorithm is ported. Native world/LOD/radiance, motion, DB/IO/upload, function
and both device budgets remain open; Master owns integration and native validation.
