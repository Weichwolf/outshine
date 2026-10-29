# Held building field views

Owner/decision: #1663; parent #1167; native consumer #1570. Python research only.

The existing first-hit material/depth/normal field is lit per texel before spatial
filtering. This experiment integrates its coupled linear premultiplied RGB and
coverage directly over a screen pixel. An exclusive `(N+1)²` Float32 summed-area
table replaces the intermediate small filtered image and its second bilinear
card reconstruction. There is one layer and one selected view, not a new atlas bank.
Transparent texels contain zero RGB. Transmittance is `1-coverage` for this opaque
source experiment; this is not a layered glass/transmission model.

Four bilinear prefix evaluations integrate an axis-aligned source rectangle
exactly in real arithmetic. Each evaluation can use four explicit texel fetches;
native integration must not assume Float32 hardware filtering support. Floating
point error is measured separately. Prefix coordinates clamp to the source edges;
the integration denominator retains the complete pixel footprint outside them.
Raster support extends one screen pixel beyond the projected capture rectangle.
Otherwise edge pixels whose centres miss the card lose valid filtered coverage.
Small negative Float32 cancellation is explicitly projected back to nonnegative
RGB, coverage `[0,1]` and transparent black; raw minima remain in diagnostics.

`field_view_integrals.py` owns integration, `field_view_sampling.py` owns projection
and support. Parallel camera/capture axes have a rectangular footprint. Other
axes require an explicit bounding-rectangle approximation and report its excess
area; they do not inherit the parallel admission. No normals/materials are averaged
before lighting. The original geometry and its source IDs remain the opponent.

## Bounded empirical use

The positive complete-cell control contains 508 source IDs and 12,327 original
triangles. At 1280×720, focal length900px and the existing capture-frame axes,
the declared domain is distance60–68km and capture-relative yaw/pitch±0.5°.
Eight boundary combinations and the centre use native original SS32×32 averages.
The proposed research profile requires relative L1 error≤5% for linear RGB and≤3%
for coverage. These are explicitly finite-reference thresholds, not an analytical
bound on image error or a replacement for the project's runtime image gates.
The nine controls measure at most4.491% RGB and2.052% coverage with a128² field.
The existing256² native filtered-card path reaches10.949% and7.344% respectively.
Additional SS64×64 originals at the centre and worst SS32 RGB pose give2.784% and
4.542% RGB error,1.237% and1.374% coverage. SS32→SS64 originals differ by up to
1.593% RGB/0.951% coverage. The finite-reference profile still passes these probes;
this variation is not a rigorously bounded reference tail.

An independent13-pose path measures at most3.995% RGB and1.872% coverage.
A120s path, sampled60times/s at up to102.49m/s, evaluates7,201 held-field views:
one prefix build, zero pose rebuilds,4,201 field and3,000 parent selections.
The two handoffs use exclusive ownership, not opacity fading or two render owners.
Exact adjacent60Hz native SS32×32 opponents give additional transition error
≤3.772% RGB/1.520% coverage. Original parent readiness is assumed in this isolated
test; it is not a streaming/prefetch or whole-world movement qualification.

`field_view_admission.py` enters inside61–67km/±0.4° and stays until the declared
outer boundary. Changed source/light/material/shader binding, original remainder,
unqualified camera profile or external depth interleave preserves the parent.
Its error record is an explicitly supplied empirical calibration, not a mechanism
that proves or generates one. The source/view and calibrated profile must match;
neither nearest-view lookup nor a cache hit manufactures quality metadata.

A second complete urban control has474 sources/11,980 triangles. Its nine SS32
region samples reach5.540% RGB/2.421% coverage. At that worst RGB pose, an SS64
original changes the field error to4.096%/2.156%; the two original references differ
by2.996% RGB/0.800% coverage. Thus the apparent SS32 failure is not proof of a bad
representation. Its parent conservatively stays because a trustworthy whole-region
calibration is still missing; the threshold is not raised to admit it.
An8° control reaches16.277%/11.076% there,13.291%/10.426% in the first control.
The32km control is not intrinsically bad; being outside this tested scale domain
does not establish a global distance threshold. Cell41 is refused with original
source3825/3827 and their actual unsupported-footprint reasons retained.
Four additional camera yaw/pitch corner probes at±0.5° measure4.374%/1.573% and
≤0.000154 extra footprint area. They do not qualify the combined moving-eye and
rotating-camera domain; the admission helper deliberately retains the fixed axes.

## Geometric authority and reference limits

`field_view_projection.py` outward-rounds an AABB-to-capture-plane displacement
bound for exact finite input coordinates. Source and flattened-point forward
distances stay positive. Independent extrema of height and projection numerators
over AABB corners, with positive denominator lower bounds, enclose the interior.
The eye region is a bounded parallelepiped; retaining the algebraic correlation
of eye offsets avoids a loose independent-XYZ estimate along the capture axis.
For the complete positive control, a superset of the continuous domain gives
L1 displacement≤0.056122px. This bounds geometric flattening only, not capture
quantization, visibility/disocclusion, material/shading or image integration error.
Tests countercheck using exact rational projections of corners and interior points.

Actual native base depth and Float4 shading normals are evaluated separately from
SS colour, using #1667's same-frame export. At the centre:8 original-covered pixel
centres,2 without a covered nearest capture texel; flat-plane forward-depth error
up to122.43m. Among6 matched samples, stored first-layer depth reaches2.56m error
and normal angle91.95°. Across11 poses:99 original centre hits/13 unmatched.
These are explicit failures for main-depth/normal replacement, even with good C/T.
Normals are normalized only for the angular metric, never to change the readback.
No shadow, reflection, interleaved object or terrain authority is inferred.

`field_view_visibility.py` independently intersects the same Float32 original
triangles at the viewer and the orthographic capture direction. At8×8 subpixel
sampling, the boundary/centre have0/520 and0/457 newly exposed hits; the8° opponent
has14/468. This distinguishes geometry revealed by movement from atlas quantization.
CPU centre rays disagree with native masks at1/10,1/8 and0/7 native hits; common
depths differ by up to0.290m. CPU rays remain a separate geometric diagnostic and
never overwrite the authoritative native colour/depth/normal arrays.
Original reference convergence is measured, not assumed. Native finite shifted-
camera averages are not continuous-pixel-integral certificates; their uncertainty
is separate from the field error. Existing pins/oracles are not regenerated.

## Cost scopes and native handoff

For the positive128² field, a Float32 prefix is266,256B; the existing single raw
unlit view is655,612B; the diagnostic lit Float32 readback is262,144B. The original
three-role geometry exports1,537,116B of attributes/indices. These are payload
scopes, not total GPU/CPU/UMA allocations, process peaks or device reserves.
The256² prefix/raw view require1,056,784B/2,621,692B. One view suffices here.
All11 original128² per-query lit fields are byte-identical at fixed light; native
diagnostics still perform11 preparations because their current key includes eye.

For64 output pixels, direct equal-box enumeration visits17,709 source cells versus
1,024 logical RGBA prefix fetches. Seven final local CPU rounds under foreign host
work measure4.977ms versus0.704ms for pure integration; held full projection+
sampling1.774ms, prefix build1.824ms. Separate native-allocation Memray traces:
enumeration8,804 allocations,
816,277B requested/17,083B scoped peak; lookups781/191,792B/11,376B; prefix build
412/1,725,931B/536,151B. Inputs and already held tables precede those scoped peaks.
Actual Python/NumPy overhead remains included. These are no native device timings.
Known owned-prefix copying is266,256B; temporary/gather copying is not fully counted.
`field_view_cost.py` also records256² and Float32-vs-direct numerical deltas.

Native integration needs existing source/view/recipe and material/light/shader
bindings, capture centre/direction/extent, complete source coverage/remainders,
calibrated camera/pose/error metadata, prefix format/stride and pixel-support rule.
Reject unknown/failed metadata before loading the fine product. Retain the existing
qualified parent or request another valid view/refinement when outside the domain;
missing parts never become an empty valid replacement. Pose/focal presentation work
does not rebuild an invariant shaded source. Actual source shading-view changes,
source/material/light/shader changes invalidate the derived table; changed-light
scheduling, resource leases and retained peaks belong to native owners. Other scene
depth must be demonstrably separated or use an independently qualified depth/parent
product. No new scheduler/cache or transparent main-depth policy is introduced.
Depth separation alone does not qualify subpixel composition: averaging foreground
coverage and a correlated varying background separately loses their covariance.
The independent four-sample counterexample in the admission tests has0.25 absolute
RGB error despite completely ordered depths. `composition_qualified` must therefore
also be explicit: these probes use an isolated group/constant backdrop. General
overlapping filtered groups or varying terrain/background require their own
composition/error qualification or retain the parent; this is not inferred from
the field's good isolated C/T or a depth gap.

## Reproduction

Use the repository`.venv` with NumPy/Shapely/trimesh/rtree and system libzstd;
Memray is needed only for allocation costs. Existing fixed controls live in
`test/outshine/data/render/building-full-view/`. `field_view_fixture.prepare_cell`
accepts that data directory, a native source recipe, cell and fresh temp output;
it validates original manifests/IDs and exports unchanged role/UV/metre/normal/
colour/index streams, refusing any original remainder. The raw source recipe uses
the fixed`source-plane-request.json`, an explicit extracted source`input.json` and
the existing`source-world-controls.json`. It does not depend on private worktrees.
Run native`--radiance-world` on the candidate and`--source-mesh` on the original;
original`geometry/0` references that complete geometry and`catalogue=[[0,0,1]]`.
Use an explicit system-temp output for every invocation below.

```sh
export PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings
.venv/bin/python -m unittest test_field_view_projection test_field_view_integrals \
  test_field_view_sampling test_field_view_geometry test_field_view_inputs \
  test_field_view_admission test_field_view_motion
.venv/bin/python test/experiments/render/compare_field_views.py \
  --field "$field" --held-query "$query" --original "$original" --output "$fresh"
.venv/bin/python test/experiments/render/replay_field_view_motion.py \
  --field "$field" --held-query "$query" --trajectory "$path_json" --output "$motion"
.venv/bin/python test/experiments/render/field_view_cost.py \
  --field "$field" --held-query "$query" --output "$cost"
```

Timed paths explicitly supply bindings, complete/depth/composition status, calibrated
domain, fixed camera and increasing timed eye knots. Handoff queries are exported
for the same original renderer. Query-prefix selection permits reusing existing
original outputs; raw or losslessly gzipped readbacks are accepted. Diagnostic
images remain local system-temp crops of verified full1280×720 native images.

References: SIGGRAPH1998
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf)
(projection/depth warps; page3 optically checked); Crow, SIGGRAPH1984,
[Summed-Area Tables for Texture Mapping](https://doi.org/10.1145/800031.808600)
(bibliographic primary reference; original PDF access failed); Green/NVIDIA2003,
[Summed Area Tables using Graphics Hardware](https://www.nvidia.com/docs/io/8227/gdc2003_summedareatables.pdf)
(primary hardware exposition, pages4/8 optically checked; rectangle/bilinear-prefix
and edge-clamp figures). No third-party algorithm implementation is bundled.
