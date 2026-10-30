# Raw first-hit cells in a different World raster

Owner #1797, Python reference only. The existing registered-point composition is
still the point-ordering operation; this experiment supplies a concrete geometric
resampling opponent before it. It does not approve a new LOD/view domain.

SIGGRAPH first: Shade et al., *Layered Depth Images* (1998), local
`doc/references/vegetation/siggraph/1998-layered-depth-images.pdf`, sections 2–4.
PDF pages 3 and 5, equations and figures were opened. Sprites with depth explain
parallax; additional layers address hidden surfaces. We introduce no LDI format,
extra capture views or hidden geometry. A single first-hit capture cannot recover
a newly visible back surface. An explicit two-square unit opponent demonstrates
that even a successfully evaluated model miss can hide a real original hit.

## Representation and bounded ray evaluation

An input pixel is a half-open square in capture raster coordinates at its
literal stored reverse-Z depth, carrying that same pixel's premultiplied linear
HDR C/coverage. Opaque point coverage is 0 or 1; zero-depth cells are clear in
this discrete model. No interpolation bridges depth discontinuities. This is a
declared approximation, not a continuous reconstruction of original triangles.

Capture and World matrices map the same field frame to clip space, with depth
in [0,1], near larger and zero clear. Raster origin is top-left. The transfer
uses the existing `depth_reprojection.transfer_matrix` implementation, preserving
the previous forward-point experiment and its consumer unchanged.

For one World sample (x,y), its homogeneous capture position is A+sB, where s is
World reverse-Z. At a capture depth d, solve

    s = (A.z - d*A.w) / (d*B.w - B.z).

Its capture coordinates follow by homogeneous division. The ray cuts the finite
represented min/max depth slab in a straight segment in capture NDC. This slab
bounds only represented cells; it is never a complete original-source bound.
Clip the segment to the capture raster and enumerate its grid crossings. For
each crossed opaque cell, solve at that cell's raw depth and check the actual
half-open cell, positive homogeneous w and World depth range. The largest valid
World reverse-Z wins. This also supports multiple intersections of the discrete
depth surface; a one-step inverse lookup need not find the nearest one.

Sample/texel/expansion/work limits precede variable expansion. A failed local
query has `evaluated=false`, not a proven clear result. `evaluated=true` means
only this cell model was evaluated. Neither value grants original coverage.
Inputs are borrowed synchronously and unchanged; outputs own their arrays.
The temporary scan is bounded per ray. No IO, worker, persistent resource,
cache, solver or additional package belongs to this numerical kernel.

Seven tests cover an orthographic-to-perspective plane/analytic ground positive,
different raster resolution and colour/depth coupling, 576 independent random
field-space rays against every explicit cell plane (orthographic/perspective
captures), half-open boundaries, disocclusion, invalid inputs and local work
exhaustion. An independent brute opponent constructs actual cell planes in
field coordinates rather than using the segment/grid traversal.

## Frozen native original opponent

`compare_field_depth_reprojection.py` consumes the existing versioned #1716
whole-original fixture and #1724 submitted capture operands. It checks both
receipts and source provenance. All supported originals and both unsupported
source IDs/archived material-opacity limitations remain explicit. A complete
source-bound crop is selected before allocating World samples. A separate
eight-corner capture-cell hull checks that the full represented model also fits
that crop, including pixel edges rather than just stored hit centres.

The 128×128 capture and 1280×720 perspective World view differ in projection,
eye, basis and raster. The capture matrix uses the actual packed coefficients
and rounded Flat model translation interpreted in real arithmetic. The World
matrix comes from the finite manifest camera; submitted World GPU operands are
not available in this fixture. This is not a replay of GPU rounding.

Compare depth cells with a flat card made from the same opaque C/coverage cells
at a midpoint depth. Both use the same requested rays and per-microcamera native
rotation convention. Held capture radiance is not relit for each microcamera.
Measure centre errors and finite native 16-microcamera mean errors separately.
The depth model reduces the observed HDR RGB error, but coverage/hidden-surface
errors remain. No declared acceptable image budget or automatic admission is
inferred from that improvement.

Actual native ordered/interleaved external World point images supply a second
opponent. Exact original points reproduce their native union. The reprojected
source plus the native external image is ordered at common World points before
a declared 2×2 filter. This filter is separate from native SS16; the fixture does
not contain all external SS16 point samples. Independent filtering followed by
global over loses ordering/correlation and is retained as a negative control.
These are actual whole-group geometry controls, not a native terrain union.

## Native consumer and local original fallback

1. Use existing complete metadata/geometric admission before fine IO (#1823).
   First-hit min/max is not substituted for the complete source hull.
2. Bind the actual executed request, source/frame, capture geometry, raw sample
   convention, material/light state and actual World PBR observer. Capture rays
   and lighting camera are independent. A changed pose is not rejected merely
   for differing from Capture; the approximation must be qualified for it.
3. Reproject coupled raw C/coverage/depth to the actual World sample domain.
   Qualify visibility/radiance and depth error there; model evaluation alone
   does not establish any of them. Unknown source coverage retains its parent.
4. Resolve source/terrain/other geometry on the same World samples. Compare
   conservative reverse-Z intervals; overlap is unresolved. The existing
   `field_external_depth.compose_registered` contract remains applicable once
   the points are truly registered and independently qualified.
5. Filter the jointly resolved C/T once with the same weights. An independently
   filtered foreground and varying background do not retain joint coverage.

The fallback owner is the affected whole source group, not the whole World.
Unknown pose/lighting/coverage keeps that group's existing original path. A
native consumer may restrict original evaluation to unresolved World samples
only if it provides exclusive sample ownership and fills them before filtering;
the field and parent must not both contribute to the same sample. Without that
native masking contract retain the group as a whole. A single unresolved pixel
may still require its entire fine asset, so local pixel fallback alone proves
no IO/residency saving. Main/shadow depth, other shading passes and GPU lifetime
remain native consumer obligations.

The concrete remaining native packet is requested in #1797/6527093219: actual
terrain/source/union World opponents for a small positive pose change and a
lateral/disoccluding negative, with submitted projection/frame/sample operands.
No live revision is fabricated and no CPU result lowers the native LOD bound.

## Reproduction and costs

Use the existing .venv, with `PYTHONPATH=lib/src:test/experiments/render:` plus
the buildings/transport/assets experiment directories. Set
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`. Output is a fresh host-temp path outside
the checkout, checked by the shared `experiment_output.create_output` helper.

```sh
python -B -m unittest test_field_depth_reprojection test_depth_reprojection \
  test_field_composition test_field_external_depth
python -B test/experiments/render/compare_field_depth_reprojection.py \
  --cost --output "$fresh_system_temp_output"
```

The report records commit/source/input hashes, matrices, crop, image errors,
work, host/load and seven centre-query CPU rounds plus separate native Memray
traces. Imports/input IO precede timing; validation and transfer inversion are
included. Raw capture payload is 327,680 B; 252 centre results own 6,300 B.
These are array payloads, not physical traffic, GPU residency or UMA peaks.
The array-sorting traversal's measured allocation overhead motivated a bounded
scalar crossing enumeration. Frozen baseline and current outputs are compared
exactly; allocation reductions do not imply a device/frame improvement. Both
image approximations remain unqualified, so timings do not select an
equal-quality production winner. Native World, Metal/Iris and p99/8-GB gates
remain open. Diagnostic images stay in system temp; no doc/README gallery.
