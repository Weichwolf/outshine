# Bounded rooflight render sheets

Owner: [#500](https://github.com/Weichwolf/outshine/issues/500). This experiment
directly constructs reduced rooflight surfaces from the stable v2 fields and
retains the complete prepared source body. It does not rebuild source planning
or lightwell CSG when emitting a view/state-dependent render product. Body
construction is still required once; that work is separately measured.

The proven geometry lives in installed `rooflight_panels.sheet_template` and
`sheets`, documented in [the library contract](../../../lib/rooflight_panels.md).
The view policy remains here. Default v1, normal hall-v2 and the experimental
shared recipe kernel retain their previous semantics. No native/cache/material
ABI or automatic compact-world selection is changed.

## Geometry and projection

Full (44 triangles) versus sheets (20 triangles) keeps both frame caps and both glass caps in place.
Every removed vertical frame-side point is within 0.06 m of a retained same-XY
cap point; the glass-side bound is 0.003 m. Caps are subsets of the original
material surfaces. This is a restricted analytic correspondence, with changed
topology; its open geometry is rejected as an independent collision solid.
The complete physical pane/frame and source body remain independent of render
LOD. First-hit depth, normals, transparency, shadows and radiance have no general
bound from this geometric distance alone.

For each field, the eight actual outer corners at top and bottom enclose all its
surfaces. In camera coordinates let `n` be their minimum depth minus the movement
radius, and `R` their maximum Euclidean distance plus that radius. With focal
length `f` in pixels and geometric radius `r=0.06 m`,
`e = f*r*R / (n*(n-r))` bounds perspective displacement when `n>r`.
Otherwise a full part is required. A field uses sheets only if `e` is within
the view's declared geometry-error budget (default 0.5 px). Translation within the
declared movement radius is covered at the current orientation; rotation or
changed view parameters requires evaluation at that actual view. This is not a
temporal/radiance or shadow budget. Infinite near-plane cases are reported as
an unavailable finite bound, never serialized as nonfinite JSON.

Selection precedes template expansion. The final policy batches field bounds
through one camera basis/projection operation; its decisions agree with the
independent scalar reference. It never generates full panels just to discard
their sides. A capacity of 64 unique fields rejects before expansion. All fields
remain source-owned; source/physical products are unchanged.

## Independent evidence

All 117 fields on three actual provider halls (55/16/46) are checked with
Trimesh's independent material-aware nearest-triangle implementation in both
directions. Actual triangle-centre/edge samples satisfy the derived frame/glass
bounds. Shifted, missing and misbound panes fail. Movement/projection controls,
near-plane fallback, early capacity/type/mutation checks, open-surface collision
rejection and unchanged retained bodies without replanning/CSG are covered.
The final source 57 checks and installed 57 controls pass on library promotion;
terminal commits, times and final merged-package checks belong to the PR.

Six new actual 1920×1080/16-sample Eevee images are optically opened, plus an
actual 2400×1350 plan/mesh-section/2.5D diagnostic. Raster batch `2f93579f4` ends 0
with the documented flush/post-render teardown workaround; llvmpipe LLVM 19.1.7.
Existing full source 2 far/near images are reused only after exact
arrays/dtypes/palette/camera checks. The near adaptive product is exactly full.
All nine exported model inputs replay exactly after library promotion.

| Actual view | Changed pixels / 2073600 | Worst channel /255 | Mean channel error /255 |
|---|---|---|---|
|source0 far, admitted|1528|21|0.001073|
|source1 far, admitted|1082|15|0.001157|
|source 2 far, admitted|1549|15|0.001063|
|source 2 near, forced negative|6347|53|0.046095|

In that near camera all 46 fields reject sheets; the unsafe forced result is a
negative control, not an alternate accepted LOD. The actual distant buildings
are small: these images check distant panel preservation, not general facade
quality, architectural variety or a complete whole-city LOD. Individual total
triangle counts change 11094→9774,4974→4590, 10512→9408. A 54.5% field reduction
is not a 54.5% whole-building reduction.

## Equal-quality work and costs

The equal-output alternative builds the full local template, filters nonvertical
caps and repacks them. It has the same immutable typed-cache contract; all
output arrays/dtypes equal direct construction. Direct creation avoids discarded
triangles on cold construction. Warm medians do not establish a uniform winner.
Full (44 triangles) is also measured as the zero-error baseline; adaptive (20 triangles) includes selection.

Frozen `1656e753e`, normal priority on a shared four-logical-CPU Linux host, 21 warm
runs. The nice-10 source/wheel work was finishing around this run; no exclusive
host or native/device/frame result is claimed. Times are milliseconds:

| Fields | Method | Cold ms | Warm median ms | Unique array bytes | Traced warm peak bytes |
|---|---|---|---|---|---|
|55|full|3.829|1.431|377960|382232|
|55|full then filter|4.278|1.274|171800|190448|
|55|direct sheets|3.057|1.327|171800|190448|
|55|adaptive|4.570|2.730|171800|192288|
|16|full|1.239|0.373|113168|114152|
|16|full then filter|1.728|0.363|51440|57128|
|16|direct sheets|1.218|0.389|51440|57128|
|16|adaptive|1.778|0.881|51440|58200|
|46|full|2.442|1.164|309144|320360|
|46|full then filter|2.964|1.098|140520|159656|
|46|direct sheets|2.194|1.076|140520|159680|
|46|adaptive|3.545|2.220|140520|160968|

Body/source/CSG preparation separately costs 174.437/82.959/156.614 ms. Batched
selection medians 1.211/0.495/1.057 ms replace scalar 7.647/2.234/6.507 ms, while
its traced peak rises to 46711/15736/39358 bytes from 8648/8264/8552. Adaptive
emission still adds CPU over full construction: lower retained geometry and
peak do not prove a complete-pipeline win. Stable-detail reuse and native GPU
savings remain separate needs; export concatenation also copies the body.

Memray 1.20.0 records 24 separate cold/warm profiles, with native stacks and Python
allocator tracing; profiled times are excluded from the 21 normal runs. For 55
fields, cold full/filter/direct/adaptive allocation requests are 13759/16955/
10724/17991, cumulative requested bytes 2643008/2706608/1729648/2381321, tracked
peaks 463638/302898/226100/227541 bytes. Warm full versus adaptive requests are
6062 versus 13327 and requested bytes 1496520 versus 1767817, despite lower adaptive
peak 383136→193296 bytes. Every profiled output retains all arrays/dtypes exactly.
Requests include reallocations/allocator layers; cumulative bytes are not unique
or live storage. Full machine-readable cases/profile hashes remain in the local
cost artifact. Known final transforms and their intermediate vertex-matmul
buffers are counted; total internal copied bytes remain unobserved.

## Reproduction

Use `.venv`, a frozen checkout and explicit system-temp outputs. Optional allocation
profiling requires `pip install -r test/experiments/buildings/requirements-profile.txt`:

```sh
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python -m unittest verify_rooflight_lamina verify_recipe_rooflights verify_rooflight_surfaces verify_industrial_rooflights verify_industrial_halls verify_recipe_halls verify_recipe_graph -q
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python test/experiments/buildings/rooflight_lamina_probe.py --output /tmp/outshine-buildings-sheets --references /tmp/outshine-buildings-rooflights-cameras-output --profiles
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python test/experiments/buildings/rooflight_lamina_sections.py --output /tmp/outshine-buildings-sheets/sections.png
```

Original source/reference reproduction: [industrial_rooflights.md](industrial_rooflights.md).
[SIGGRAPH 1996, Simplification Envelopes](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf),
page 1 optically inspected, motivates explicit two-sided distance guarantees;
this restricted cap rule changes topology and does not implement the general
envelope algorithm. The existing local Esri reduceGeometry study supplies a
vertex-fraction/edge-collapse reference, not this distance contract or an oracle.
Complete compact bodies, whole-tile retained-asset delivery, stable temporal/
shadow/night behaviour and native admission remain open in #500.
