# Shared closed-window geometry for outer shadow demand

[#1077](https://github.com/Weichwolf/outshine/issues/1077), following
[#1076](https://github.com/Weichwolf/outshine/pull/1076). Camera-only window detail
selection lost solar-shadow contributions. This experiment keeps the original
visible window meshes and supplies **all remaining closed window geometry** from
shared intrinsic templates. All seven finite comparison images are pixel-identical
to the complete closed-window controls. This does not qualify a general shadow
selector, textured/PBR replacement, moving-view policy or native renderer.

## Representation and quality boundary

Compile the existing no-legacy-trim closed frame/glass/sill/mullion recipe once per
exact binary64 width/height pair. Store immutable local geometry, original ordered
Window/Source IDs, template indices, original placements and the forward core frame.
No dimension tolerance, view-dependent asset key, inverse frame or persistent history.
`window_geometry_plan.py` owns generator access; `window_geometry_templates.py`
depends only on NumPy/stdlib. Its saved reader emits requested IDs without source,
generator or GEOS access. Save/load is a bounded, trusted experimental snapshot,
**not** the native asset cache format or its revision/content-validation contract.
Part indices are template-local; the owning Window/Source identity remains explicit.
No native Face-ID or GPU transform/binding contract is introduced.

Across five versioned original sources × two facade plans, all 2,936 Float32 vertex
sets match the original singleton construction. Worst bidirectional binary64 vertex
distance is 1.1718571004216928e-13 m. All emitted vertices remain inside the existing
#1072 bounds. Only **1,233/2,936** oriented Float32 triangle/material sets match after
normalizing cyclic vertex starts: triangulation really differs. Vertex equality
alone does not prove equal rasterization, normals, UVs or filtering. Visible windows
therefore retain the original complete draw builder; independent piers remain intact.

Camera-plane candidates are selected from logical bounds before detail construction.
Each diagnostic pose uses a fresh manager; every noncandidate window is emitted from
the saved templates. All geometry is materialized for this offline image comparison:
there is **no triangle-count reduction**, GPU instancing, measured GPU residency or
admission of camera-only geometry as sufficient shadow demand. Material roles are the
existing opaque, UV-independent fixture roles. Alpha, textured cutouts, transmission,
reflection and general PBR shadow equivalence remain unqualified.

## Actual image comparison

Same source, coarse host, object name, camera, ground, materials and Eevee settings as
#1076; 1280×720, 16 samples, software llvmpipe. Three sources, near/fourfold-distance
poses, plus the court distance pose with opposite sun. Reference repeats in #1076
were pixel-identical. Seven new mixed images and seven full physical-facade images
completed successfully; critical images and the complete comparison sheet were opened.
No image published. RGB8 statistics were crosschecked with `test/scripts/pixels.py`.

| Input/pose | Original visible / template outer windows | Mixed pixels changed | Full physical facade pixels >1/255 |
|---|---:|---:|---:|
| Courts near |140 /112|0|79049|
| Courts distance |183 /69|0|69833|
| Courts distance, opposite sun |183 /69|0|24223|
| Concave near |37 /260|0|29125|
| Concave distance |91 /206|0|49566|
| Compact near |41 /50|0|158317|
| Compact distance |60 /31|0|67119|

Each comparison has 921,600 pixels. All mixed-image maximum RGB8 channel errors are
zero, including the previous compact-near camera-only counterexample. This is a
finite same-renderer result, not a physical radiance bound or universal tolerance.

The additional Master-requested reference calls unchanged
`build_facade(prepared.mass, prepared=prepared)`, the existing full physical path,
then `facade_draw`. It includes floors/stairs/landing, opening CSG and legacy closed
trim; original Window IDs are checked. Its floor/trim/body silhouettes and shading
visibly differ from the compact host plus closed windows. Full-reference maximum
RGB8 differences range from 61 to 181/255. **The cheaper mixed scene is not admitted
as an equivalent replacement for that full physical facade.** Selective full-facade
detail still needs separate body/trim/contact ownership and quality qualification.

## Work, bytes and costs

Main cost/full-facade producer `b97e6e59184fddad18ce454d4a756fe743f04d0f`, based on
#1076 `2cf48669b` containing Master `15a948e18`. Mixed geometry producer `de3b15a04`;
renderers `54f7bb859`/`b97e6e591`. Intervening Python change only forwards optional
original-builder keyword arguments and adds controls. Fetch of Master `53946a622`
shows native-only changes; measured Python sources are unaffected.

Linux/i5-1135G7 shared host; Python 3.13.5, NumPy 2.5.4, Shapely 2.2.0/GEOS 3.14.1;
one BLAS/OMP thread. Three fresh ordinary processes alternate control order, plus one
separate allocation trace. Starting one-minute loads 1.55–2.54. Preparation of all
ten plans costs 1.20–1.27 s separately. Timings below are ordinary medians in ms;
tracing is not mixed into timings. Existing frame sharing caches only frame templates
within a single construction scope (limit 16), preserving every original raw word.
Batch construction has a whole-source part namespace, rather than independent
Window products. All methods build the same closed recipe, but only the mixed
image comparisons above qualify the candidate's finite use.

| Input (windows / unique measures) | Singleton | Frame sharing | Batch | Complete template build | Emit all positions |
|---|---:|---:|---:|---:|---:|
| Courts (252 /2) |642.521|170.165|148.143|6.368|14.502|
| Concave (297 /3) |708.019|175.485|114.537|9.169|12.958|
| Compact (91 /2) |219.679|63.140|53.083|5.743|4.145|

Across all ten plans, unique templates number 2–5 rather than 91–508 windows.
Template construction calls the existing recipe exactly that many times. Emission
does one position transform per requested window; topology/material/part arrays
share immutable template storage. There is no GEOS call per emitted window.

| Input | Original raw arrays B | Intrinsic template/placement arrays B | All new position arrays B | Original / candidate diagnostic files B |
|---|---:|---:|---:|---:|
| Courts |2491776|30912|1645056|247702 /23824|
| Concave |2630232|39684|1739232|260797 /28731|
| Compact |899808|23828|594048|90196 /11654|

Original per-window offset arrays add 2024/2384/736 B. Template sizes include core,
placements and indices, but exclude IDs/Python-object overhead. Emission still creates
position arrays; logical emitted byte sums repeat shared topology and are not physical
ownership or UMA residency. Forward transforms, validation and freezing also make
temporary arrays/copies; **total internal allocation count/copied bytes are not
instrumented**. Additional traced peaks (template build / emission / batch) are
129563/1798571/8534956,145711/1912584/7424400,69320/665890/2950556 B respectively.
Some native heaps are untraced. Whole-process RSS is 158.97–160.70 MB, holding all
controls/imports/diagnostics; it is not an application or target-device memory budget.

Candidate diagnostic NPZ+JSON median write/read costs are 2.464/3.448,
2.988/2.788,2.191/2.013 ms. OS-cache state is uncontrolled. Multiple small files,
compression, metadata, validation and source acquisition differ from the product;
these sizes/times do not qualify native Zstd/SSD/DB IO, upload or an end-to-end win.

**No-repeat counterexample:** 21 windows with distinct width values, reduced by
0.005 m per ordinal, require 21 templates. Median singleton/frame-sharing/batch/
template-build/emission costs are 53.311/55.029/46.819/50.470/0.993 ms. Candidate
arrays grow from 207648 to 208620 B; diagnostic files grow from 22463 to 44914 B.
Template-build traced peak is 467303 B versus singleton 270920 B. This is a raw
geometry control, not altered source/CSG/camera truth. Batch wins here: do not force
complete-template reuse when measures do not repeat.

Full physical-facade medians, separately measured in three fresh processes:

| Input | Physical build / draw conversion ms | Raw physical arrays B | Draw arrays B | Draw triangles |
|---|---:|---:|---:|---:|
| Courts |535.039 /24.964|5062972|11971780|41282|
| Concave |586.771 /28.861|5951288|13893320|47908|
| Compact |273.245 /6.798|2192584|4959000|17100|

Mixed compact-host scenes have 30119/32370/11139 triangles. Full physical raw counts
cover frame/shell/stair/landing arrays, not every metadata/native heap. Different
content makes these context costs, not matched-quality speedup ratios. No native
Fine-cell admission, smaller bound, device timing or whole-world loading claim.

## Verification and reproduction

13 focused tests pass (10.608 s): six new template tests and seven existing facade
product checks. They cover all original bounds/IDs, recipe calls only per unique
measure, closed positive-volume parts, immutable nonaliased arrays, forward shear/
large-origin controls, finite/layout failures and saved-reader source/recipe binding.
No tolerance/oracle weakened. All four main cost workers and all three full-facade
workers terminate 0. All 11,744 emitted candidate fields repeat bit-for-bit across
the four main processes. A fresh reader with source/full-geometry opens forbidden
and generators/GEOS absent reproduces those same 11,744 fields for 2,936 windows.
This proves candidate reproduction, not original triangle/UV equality.

Use existing `.venv`, with `OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1` and
`PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets`.
Run `python -B -W error -m unittest test_window_geometry_templates verify_terraced_facade_products -q`.
On frozen sources run `compare_window_geometry.py --output <fresh-system-temp> --verify`;
repeat normally with `--reverse`, and separately `--trace`. Its report contains all
ten inputs, fields, hashes, method orders, RSS and trace costs. `one()` also accepts
a prepared finite control for the no-repeat comparison.

Generate #1076 full controls with `window_detail_images.py` first. Then run
`probe_window_geometry.py --output <fresh-system-temp> --reference <controls>` and
`window_fine_reference.py` with the same arguments. Render each emitted geometry via
existing `render_window_details.py --geometry <scene-geometry.json> --output <temp>`;
add `--opposite-sun` for court distance. The existing bpy renderer emits its normal
API deprecation and is run without global warnings-as-errors; generators/tests use it.

The local starting references remain SIGGRAPH
[Procedural Modeling of Buildings (2006)](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
component reuse, and
[GPU-Driven Rendering Pipelines (2015)](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
page 15's mesh/instance transforms/bounds, visually checked. Neither source supplies
this recipe's material equivalence, conservative shadow demand or a target-device
performance guarantee. Private reports, saved inputs and viewed diagnostic pictures
remain in `/tmp/outshine-research-complete-window-2cf48669b/`.
