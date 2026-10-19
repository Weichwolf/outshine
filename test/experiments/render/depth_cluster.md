# Complete-cell depth sprites: short-motion counterprobe (#12)

This tests the small-cluster follow-up to [1/6/64 m planning](native_cell_far_products.md).
It implements one minimal **front-depth** sprite, not a full LDI, new native
pipeline or runtime product. Static geometry/material inputs are reused; two
current diagnostic lights are evaluated after visibility. Actual native PBR,
opacity, shadows, weather and device performance remain unproved.

## Input, representation and measured work

The original complete Vienna/Flensburg cells retain four/five sources. A third
complete Vienna cell (035e2150a3c1… / 58) retains 32 sources. These are 41 original
source addresses from two European cities, not 41 globally distinct buildings or
a worldwide sample. Anonymous IDs are never deduplicated; every original index,
summary, form and generated part stays available for fallback.

The capture rectangle follows actual projected group size at 1280×720 and the
original 1044.3315497801625 px focal length. Four visibility rays per presentation
pixel retain only front hits. Nothing is allocated as eight full-screen field
layers or as an image per house. A 40-byte record contains pixel index/depth,
geometric normal, native shader normal, program code/domain, support span, native
UV/metre inputs and RGBA8 colour factors. Missing metre inputs remain explicitly
unavailable, not zero-error evidence. A separate 16-byte triangle/Q30-barycentric
record binds each stored centre to Fine geometry. It does not bind its extended
support to source coverage. Descriptor and actual NumPy-file bytes are counted.

The query projects bounded tangent patches and depth-tests their capture-cell
supports; it reads no original mesh. Geometric and shader normals are separate.
The packed normals and Float32 depth are decoded into reusable CPU patch arrays;
those arrays cost 145 bytes per retained sample and are counted separately.
The query reports front samples, nominal two-triangle equivalents, tested
subpixel/support pairs, accepted supports and depth updates. Triangle equivalents
are not GPU invocations or a lower bound on other image-warp implementations.

Depth/visibility selection precedes attribute gathers. Only selected output
samples read normal/material inputs (27 logical operand bytes per visible
subpixel, excluding physical cache traffic). The comparison methods then
share colour-factor/Lambert evaluation at the presentation sample rate. Shader
inputs are not a baked final lighting result. Geometry may be reused across the
two sun states, but changed materials/pose/coverage require their own validity.
No fixed geometry, material or light update period is admitted.

The alternative merges four capture samples **only if all hit the same original
triangle**, intersecting its plane at the new centre. It does not mix depth across
triangles/IDs and selects density before gathering material fields. This preserves
the original support union in exact geometry; it does not certify silhouette
coverage or UV/material filtering. Native fine-detail triangles make such merges
rare here. The raw material inputs remain point samples, not filtered native PBR.

## Same poses, native alternatives and limits

Uniform and adaptive proposals each render the two small cells at distances
32/128 source extents and azimuths 0/44/90. Each capture is tested at 0/±1/±6 m,
a 10° camera turn in place, and an opposite-side positional failure control.
These are 168 pose/proposal comparisons, with two lights and identical 2×2
presentation sampling. The low eye is the earlier 1.7 m source-plane diagnostic,
not DEM-verified ground. Endpoints are not a continuous 60-frame motion gate.

At distance 32, small-cell serialized caches including trace/descriptor cost
7,706–12,416 B (adaptive); their raw runtime records cost less. Existing native
Shell payloads are 9,172/13,980 B. At distance 128 the complete cache files cost
1,880–2,390 B, before other directions, mips, native resource layout and fallback.
These file totals and native `Raised` bytes are different accounting scopes;
neither is a complete comparable GPU residency or whole-world cost.

The larger cell initially exposes two guardrails: the old normal-only debugger
correctly misses a projected emitter, and a coarse generation request labelled
512 still selects **projected Shell**, emitting zero triangles. That empty
control is rejected, not treated as Massed or a quality/performance success.
A generation-4 projected Shell has four mixed program-code triangles. Default
loading still rejects them; explicit geometry-only loading preserves all vertex
codes and marks the triangle program unknown (`-32768`). It never guesses a
provoking vertex for native PBR. A wrong-eye test is only an out-of-domain negative
control, not evidence against a projected product inside its reuse domain.

The valid larger-cell comparison therefore uses the **original native eye
position**, looking at the cluster, and a new private generation request at
0.5 px. Fine is 26,378 triangles / 1,972,456 B; actual projected Shell is 583 /
44,452 B with no mixed-code triangles. Output eye-radius admission is not exported
by this probe; observed ±1/±6 m results do not certify a motion volume or PBR.
Both projected outputs are checked through all six streams against independent
native JSON and identical whole/cell invocations, without a native build.

| Larger cell, one direction | Uniform | Same-triangle adaptive |
| --- | ---: | ---: |
| Retained records | 938 | 926 |
| Runtime record bytes | 37,520 B | 37,040 B |
| Sample provenance | 15,008 B | 14,816 B |
| Serialized files including descriptor | 55,577 B | 54,915 B |
| Prepared CPU patch arrays | 136,010 B | 134,270 B |

The merge saves only 1.28% of records here and changes no measured coverage.
It is not a reason for native integration. These point-support proposals also
need 1,852/1,876 nominal triangle equivalents if expanded as quads, compared with
583 native triangles; another compute/warp backend could have different costs.
Retaining complete Fine readiness adds at least its payload to cache storage;
fallback and refresh are never free. No oracle per-pixel fallback is supplied.

## Image errors and their cause

Matching the capture image at the same sample positions is expected by construction;
it is not novel-view validation. In the larger cell, +1/+6 m produces 9/10 missing
and 10/11 extra subpixels against 937 Fine-covered subpixels. Alpha L1 is
4.25/5.25 pixel-equivalents; the native Shell has zero alpha disagreement in these
poses. Native program inputs differ in 349/242 common samples. Those input
differences do not directly quantify native PBR image error. Opposite-side capture
loss is 947/947 subpixels, outside the supported short-reuse region; native
projected Shell also needs its own view-domain fallback there.

Shorter displacement is not monotonically better: −1 m has 23 missing subpixels
where −6 m has seven. Sample phase and reconstruction matter in addition to a
point-position bound. The same-triangle merge does not repair this. Panels for
both cells, both lights, the larger native-eye motion and the opposite side were
opened. All images remain local; no README/doc gallery is published.

`audit_depth_visibility` independently casts through current Fine hit points
from the capture eye. With a 1e-5 m numerical depth-equality tolerance it separates
previously visible points, occluded/backfacing points and points outside capture.
The renderer never uses these oracle queries. Four selected poses in each of the
three cells are audited; this is opaque geometry visibility, not material opacity.
For +1/+6 m and the in-place turn, **all missing subpixels in these audits were
already geometrically visible at capture**. The dense +6 m case additionally has
two newly visible Fine points for which the cache returns a hit; that alone does
not certify a correct source/material reconstruction.
Opposite-side misses are previously invisible geometry. Thus more depth layers
alone would not fix the measured short-motion holes: their reconstruction is
already wrong for visible surfaces. Pure camera rotation can also expose this
resampling error without geometric disocclusion.

## Decision and open integration

Do not port this forward tangent-patch proposal. Its small file estimate hides
readiness, reconstruction, attribute and refresh costs; the adaptive attempt has
no image gain and little work reduction. This does not reject every depth-image,
backward-warp or filtered-surface method. No resolution/layer sweep follows it.
The useful direction is to retain validated native topology for visibility and
compare **material/normal evaluation reuse separately**, with boundary-aware
filtering and actual native PBR evidence before proposing a GPU path. Source/part/
per-projected-face ownership, field validity, near/far continuity, all-direction
readiness, shadows and device budgets remain explicit open contracts.

Runtime/reference preparation, bake, visible-field gather, raster and light/error
blocks have separate Python times. `lighting_all_methods_s` includes metrics and
image assembly, not an isolated shader time. Projection, field-access and patch
array counts are logical work; allocations/library traffic are incomplete.
The larger native-eye bake takes about 27 s in Python/R-tree under parallel load,
and its whole process peaks near 1 GiB. Neither predicts native GPU bake/frame
cost or grants 1280×720/60 including p99. Hardware-specific shading features are
not assumed available on both A18 Pro and Iris Xe.

SIGGRAPH-first foundations are [LDI 1998](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf)
(pages 3–4, equations/warping discussion visually checked) and
[Decoupled Sampling 2011](https://people.csail.mit.edu/jrk/decoupledsampling/)
(authors' abstract checked; its complete architecture is not implemented here).
The latter separates visibility and shading sampling; it is not a device-speed
claim for Outshine. Existing native fields are in `ImpostorBaker`/`ImpostorAtlas`;
their presence does not prove this prototype's PBR or lifetime contracts.

## Reproduction

Use `.venv` with NumPy, Trimesh/Rtree and Pillow; existing SHA-pinned native
inputs/exporter/GDB controls follow [the complete-cell recipe](native_cell_far_products.md).
Fresh explicit system-temp outputs are required. No native build/cache writes.

```sh
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p test_depth_cluster.py
.venv/bin/python -W error test/experiments/render/compare_depth_cluster.py \
  --fine "$FINE" --shell "$SHELL" --mass "$MASS" --output "$TEMP_UNIFORM"
# Repeat --adaptive. Omit --mass when only Fine/Shell were captured.
.venv/bin/python -W error test/experiments/render/compare_depth_cluster.py \
  --fine "$DENSE_FINE" --shell "$DENSE_HALFPIXEL_SHELL" --native-eye \
  --output "$TEMP_NATIVE_EYE"
.venv/bin/python -W error test/experiments/render/audit_depth_visibility.py \
  --comparison "$TEMP_NATIVE_EYE/report.json" \
  --case native-eye-move-1 native-eye-move-6 native-eye-look-10 native-eye-opposite \
  --output "$TEMP_VISIBILITY"
```

Reports retain original memberships, input/code hashes, source ranges, raw
metrics and cost scope. Frozen pre-CLI snapshots reproduce the early small-cell
runs; selected native-eye results use the final driver. Old duplicate outputs
are removed only after bit-identical image checks; current inputs and meaningful
failure controls stay available. The #12 receipt links exact local evidence.
