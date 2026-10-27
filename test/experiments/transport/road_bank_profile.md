# Whole-width road bank profiles before meshing

[#1533](https://github.com/Weichwolf/outshine/issues/1533), under
[#20](https://github.com/Weichwolf/outshine/issues/20), follows the actual short
bank transition exposed by [#1532](https://github.com/Weichwolf/outshine/pull/1532).
`road_bank_profile.py` constructs a bounded piecewise cubic **crossfall** `q`,
where `bank=atan(q)`. This is an explicit alternative procedural recipe, not the
old cubic-angle model or a native default change. Original source data remains.

## Fixed data, feasible interior

Retain original axis, height cubics, width and outer `q/q'` profiles. For horizontal
offset `u`, tangent `T`, normal `N` and height axis `Z`, the surface is
`P=C+u*(N-q*Z)` and its longitudinal grade is

```text
g(s,u) = (h'(s)-u*q'(s)) / (1-u*k(s)).
```

Require positive denominator over the whole width. With explicit grade limit `G`,
crossfall limit `Q` and allowed control change `D`, constrain `|q|<=Q` and
`|h'-u*q'|<=G*(1-u*k)` at `u=+width` and `u=-width`. These signed inequalities
are affine in `u`; the two edges therefore cover the complete transverse range.
On each original span, `h'` and `q'` are quadratic, and `k` is linear. Nonnegative
quadratic Bernstein coefficients give a sufficient whole-interval certificate.
Convex control bounds cover `q` and its difference from the prior cubic-crossfall
recipe. This is conservative: a separate valid-profile counterexample is deferred
by the unsplit coefficient program. LP infeasibility does not prove infeasibility
of every physical profile, finer basis or changed source geometry.

The prior cubic-crossfall controls interpolate the original seam `tan(bank)` and
`bank'/cos(bank)^2`. They are distinct from `tan(cubic_bank)` in the interior.
`D` bounds changes to those cubic controls; it is **not** an unproved distance
bound to the old angular surface. Actual old/new displacements are measured.
Crossfall is C1 across internal knots; height/alignment and their numerical
continuity remain inherited. Full native body/contact C1 admission is not claimed.

Before constructing a sparse program, reject oversized/folded input, unalterable
center-grade or outer-profile violations and impossible fixed edge displacement.
An edge's available horizontal run is `L-u*integral(k ds)`. Its required height
change is `delta_h-u*delta_q`; if its magnitude exceeds `G*run`, fixed ends cannot
fit. This necessary condition rejects actual record32370 (about5mm) before a
solver. A larger neighborhood or adjusted shared end profiles is required there;
do not silently flatten one end, omit its source, or reduce rendering accuracy.

## Selection and work limits

1. Certify the prior directly with array arithmetic; no sparse matrix or LP.
2. Try one whole-curve cubic restricted to the original spans, under the same
   physical and prior-change bounds.
3. Otherwise HiGHS minimizes the maximum cubic-control adjustment. The sparse
   program enforces C0/C1, fixed outer controls and the same whole-span limits.

Default work bounds are512 original spans,20000 solver iterations and1s solver
limit. Preparation is separately bounded by input size; the HiGHS limit is not a
hard wall-clock deadline for Python setup/return. Unfinished solves remain Pending
even if they carry a candidate. Output is rechecked independently of solver rows,
including actual unscaled C1 rates; outer controls are restored exactly before
rechecking. Residual checks allow1e-9 in each constraint's own units (grade/q and
q-rate in1/m). This numerical model is not an IEEE interval-arithmetic proof.

## Actual input and comparison

Use the unchanged1462-record `road_curve_requests` archive, SHA256
`df68c2ff306fe5115db4daf7b1efd366caa96338bf673634bd9c5bba9f9d2bf8`.
Generation reads only analytic recipes, not emitted Fine geometry/normals.
The test explicitly requests `G=0.35`, `Q=0.12`, `D=0.025`; none is a recovered
source class or global default. Engine confirmed that the old capture ordinals
have no bound class/grade/crossfall dataset in
[#47](https://github.com/Weichwolf/outshine/issues/47#issuecomment-6450887828).
An earlier0.5-grade pilot also exposed conservative adjustments of already valid
profiles; the stricter explicit test exercises actual grade corrections.

Actual original maximum sampled edge grades change from0.4450 to0.3500 for55171,
and0.3642 to about0.3004 for4013. Original XY, center height and outer sections
remain. Saved unexaggerated old/new geometry and grade plots show the changes;
the whole-view geometric displacement is modest. This is a functional profile
improvement, not a claimed native scene or frame-quality gain. The whole-span
conditions qualify the new cubic model; sampling diagnoses the original model.
Full actual source/port identity, classes, topology and native consumers remain
outside this isolated pre-port fixture.

The adaptive selector and always-LP control share these bounds. Their ready sets
and retained controls can differ because of conservative/numerical qualification
and permitted bounded prior changes; report both. Faster selection is not a claim
of a shared global optimum. Costs include rows, LP calls, CPU time, output bytes
and optional separately traced allocations; traced bytes are not copied bytes.
No native device, full-world, warm-load or8GB App admission follows.

## Reproduce

Use `.venv`, existing NumPy/SciPy/Matplotlib; optional trace reuses existing Memray.
Set `PYTHONPATH=lib/src:test/experiments/transport`, `OPENBLAS_NUM_THREADS=1` and
`OMP_NUM_THREADS=1`. Choose a fresh explicit system-temp output:

```sh
.venv/bin/python -B -W error -m unittest test_road_bank_profile test_road_surface_frames test_road_surface_lod test_road_surface_uniform test_road_surface_quadrature -q
.venv/bin/python -B -W error test/experiments/transport/compare_road_bank.py --output /tmp/outshine-bank-profiles --grade .35 --crossfall .12 --change .025 --rounds 3
.venv/bin/python -B -W error test/experiments/transport/plot_road_bank.py /tmp/outshine-bank-profiles/profiles.npz --output /tmp/outshine-bank-profiles/profiles.png --grade .35
```

`--trace` adds separately timed-outside-planning Memray captures on prepared input;
the three ordinary round times are measured before tracing. Images stay local.
The shared evaluator exposes `crossfall()` so the old angular recipe and the new
crossfall recipe use the same geometry/normal consumers. Old frame/geometry
parity and original tests remain required; no old tolerance or pin is changed.

Starting references remain visually inspected SIGGRAPH2008
[street modeling, p2/Fig2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
and [Galin2010, p8/Figs17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf).
They motivate separating graph/geometry and using whole road sections. The
crossfall polynomial program and edge-run derivation above are our own model.
