# Bounded object-space plane aggregate experiment (#883 / #844)

This is a Python comparison of a complete **known input surface group**, not a
native representation or admission. It follows the measured dense-city Shell
residency problem in #844. It includes both the first known far group and the
delivered actual 576-Shell Tokyo cell. Neither is a whole-city/device admission.

## Whole surfaces before image sampling

`object_plane_cloud.partition` chooses oriented planes from largest remaining
original faces. All three vertices of each accepted triangle must satisfy the
computed epsilon slab and compatible geometric orientation. An affine face then
stays inside that slab in exact arithmetic; these Float64 checks are not interval
certificates. Both sides remain independent. No eye position selects membership.
Every original face appears once in a chart or unchanged fallback, with a reason.
The original source/Part/Face table remains in the input; chart memberships point
into it. They do not invent an original identity or per-texel ownership.

A balanced spatial bisection reduces empty padded rectangles when their declared
12-byte base fields save more than an extra descriptor. It assigns whole faces,
never clips/discards overlapping geometry and claims no optimal packing. Limits
bound planes/charts (256), face-plane relations (4194304), chart side (1024),
base texels (4194304), original triangles (131072) and vertices (262144).
The prototype accepts coordinates within +/-1e9 m and slab/texel sizes >=1e-6 m;
these are numerical experiment domains, not new world-quality profiles.
Budget exhaustion preserves original triangles; it is not empty-space evidence.

`plane_cloud_fields` orthographically rasterizes all assigned faces at 2x2 samples
per texel. The oriented nearest face within a slab supplies fields. RGBA8 stores
unlit colour factor/alpha; RGBA16F stores unnormalised normal moments. CPU queries
intersect stored Float64 plane rectangles, bilinearly filter premultiplied data,
sort transparent layers and combine them with actual original fallback hits.
There are no camera-facing rotations or original-mesh queries for chart hits.
The reported two triangles per rectangle are equivalents, not GPU invocations or
an implemented Float32 vertex layout. Actual implicit geometry/arrays are saved.

Normal moments and colour do not preserve general normal/colour correlations or
metallic-roughness distributions. Native UV/program/metre shading, alpha/culling
qualification, PBR, shadows, changed materials, mips and continuous motion remain
unproved. Slab distance does not protect sampled coverage, ordering or appearance.
Mips are counted for a declared floor-halving layout, not generated/admitted.
Decoded CPU filter arrays are counted separately from stored fields; internal
allocation counts, physical bus traffic and GPU times are unavailable.

## First original control and negative result

The versioned far `native-cell.json.gz` has SHA256
`e29429e2fa670c2a5ce4b2357ee274c31fc6be7764866737b57933fb863a6c99`.
Its Full group contains 131 sources / 2951 triangles; Full is not relabelled as
a pure Shell view. With explicit epsilon=1 m and pitch=1 m, compact partitioning
assigns 1273 faces to 256 charts and retains 1678 fallback faces (2190 nominal
triangles in total). It uses 380665 face-plane relations / 1141995 vertex dots.

Actual base textures occupy 4872612 B; declared full mips 6433572 B before fallback.
Only 24421 of 406051 base texels have nonzero alpha (6.0%). Decoded CPU fields
add 22738856 B. The original group's positions/UV/packed-normal/colour/metre/index
NumPy arrays total 377928 B, excluding binding/decoded-normal diagnostics; that
is an explicit array scope, not a native GPU layout. All original arrays stay
resident in this diagnostic. Compressed file size is not residency.

Same 1280x720/native-focal projection and 2x2 presentation sampling use all rays
in the enclosing 57x7 or 58x7 pixel ROI. The recorded native eye is retained;
cell-centred diagnostic orientation and +6 m/+5-degree poses are test inputs,
not the native Tokyo 60-frame/360-degree sequence. Two synthetic above-horizon
light directions exercise the same colour-factor/Lambert model on both paths.

Still/+6 m/+6 m with turn yield alpha L1 errors 61.67/60.81/61.00 pixel units;
5/10/7 half-coverage pixels are lost and 40/35/32 gained. Largest common-hit depth
differences are 134.90/173.18/134.01 m, despite the small computed slab distance:
those rays can select different original surfaces. This is not point displacement.
The enlarged nearest-display diagnostic visibly broadens/breaks the thin skyline.
It was opened and inspected. 192 exhaustive original triangle-ray checks agree
with the shared R-tree reference. All 12 geometry/alpha arrays remain exact when
changing only light directions and static diagnostic code.

This first proposal is **not a port candidate**: texture residency grows while
coverage and depth fail. A smaller triangle count or compressed archive does not
repair it. No resolution/layer sweep or native API change follows. The dense original Tokyo control below independently rejects this recipe too.

## Actual dense Shell control

`full_view_reference` validates the delivered manifest and original native view,
retains all576 sources/14169 triangles and compares every reconstructed vertex
input byte against the native stream. Unknown triangle Part/Face stays unknown.
The original Double scene frame supplies120 actual static/moving benchmark poses.
The experiment selects the static pose nearest the cell's screen centre and the
same moving frame plus its immediate successor. It never recentres the camera,
changes focal length or changes the original1280x720 profile.

Selected frames are static20, moving20 and moving21: 691.5535657096198px focal,
6-degree angular steps, 6m/s, thus2m/2.1m translation at these sample times.
These are three actual poses, **not a complete120-frame/6m endpoint admission**.
At1m slab/pitch,256 charts replace3003 faces and retain11166 original triangles;
base fields cost7091664B, declared mips9377868B before fallback. Direct declared
48B vertex fields/UInt32 indices cost1546284B. Geometry/field scopes differ from
native GPU bindings; both comparisons already disfavor this fixed recipe.

Alpha L1 is30.09/29.62/32.31 pixel units;8/11/12 half-coverage pixels are lost,
11/8/11 gained. Largest common-hit depth differences128.48/82.96/155.43m reflect
surface selection, not point displacement. Two above-horizon diagnostic lights
and192 independent exhaustive original-ray checks share the same frames. The
1280x720 nearest-enlarged contact sheet was opened; the candidate breaks/softens
parts of the thin skyline. These observations do not certify a native1px limit.
No native port follows: texture storage increases while the image degrades.

## Reproduction

Existing `.venv` packages: NumPy, Trimesh/Rtree and Pillow. No reference-library
or native code changes; source/installed-library parity is inapplicable here.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -W error \
  -m unittest test_object_plane_cloud test_plane_cloud_fields
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -W error \
  test/experiments/render/compare_plane_cloud.py \
  --output "${TMPDIR:-/tmp}/outshine-research-plane-control"
```

The output must be new. It contains hashes, memberships/reasons, actual base
fields, per-pose raw images/depth/alpha, work, separate partition/bake/view costs,
traced peaks and Linux process RSS. Reference-index preparation is outside those
stage timers. View timers include original reference, candidate, metrics and
controls; they are not isolated render timings. The local checkpoint receipt
records terminal status and commit. Images stay in system temp, not README/doc.

SIGGRAPH-first: Décoret et al., *Billboard Clouds* (2003),
[author PDF](https://graphics.cs.yale.edu/sites/default/files/bc03_0.pdf), especially
whole-face association, compact texture regions and normal-field relighting.
Page6/Figures8,11,12 were optically inspected. This bounded greedy heuristic is
not their full plane-space optimisation, multi-projection crack treatment or a
claim to reproduce their results. The missing appearance/coverage contracts are
measured limitations, not paper-derived guarantees.
