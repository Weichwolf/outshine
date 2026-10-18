# Finite GGX corner response: rejected centroid residuals

Owner #422, parent #6, goal #189. This follows the rejected
[point-normal groups](relief_distribution.md). It changes no product shader,
material recipe, binding or light loop. Both finite candidates fail selection.

## Fixed comparison and result

Reuse the original [independent GGX study](relief_shading.md): the same 24
footprints, 288 curves, 65 light directions, 2mm height/bevel and native guarded
BRDF. Roughness0.9 is the authored matte fixture;0.2/0.5 remain counterexamples.
The per-curve selection limits were recorded before running: maximum2% and
RMS0.5% of that curve's own reference peak. Weak curves are retained.

| Finite tiles per corner | Failed curves /288 | Worst maximum /peak | Worst RMS /peak |
|---|---:|---:|---:|
|1|14|22.187021%|17.409291%|
|4|10|13.175470%|10.366810%|

With four tiles, sheared-corner-0.003 still fails9 curves, including authored
roughness0.9. Its weak transverse glossy curve at view50degrees has the worst
maximum and RMS. Sheared-corner-0.0002 fails one glossy curve: maximum2.665685%,
RMS0.512841%. The3mm case's matte/view0/azimuth0 curve retains2.928406% maximum
and2.149876% RMS. A favourable far-footprint average cannot accept those errors.
Flat, zero-depth, constant-slope and all seven moving straight-joint responses
remain exact within1e−11. Exact area-weighted gradient moments independently
match the retained quadrature inputs within1e−10.

The local four-page PDF was optically opened: fixed selection limits, weak
glossy/matte counterexamples, all response bands and work. Response bands are
CPU component plots normalized per curve, not native lit images or a world
image improvement. No candidate advances to a production/native cost trial.

## Finite kernel and residual

Brick height is depth*u(x)*v(y), with the existing piecewise-linear edge ramps.
Straight bevels have a constant gradient and use the unchanged point BRDF.
On corners the slope q=(qx,qy) is affine in screen position. A clipped convex
pixel piece therefore maps to a finite convex slope polygon with constant
density screen_area/slope_area. This preserves the whole distribution inside
the corner rather than replacing it by a point normal.

For n=(-qx,-qy,1)/sqrt(1+q.q), normalized half-vector h, a2=roughness^4:

```text
Q(q) = 1 + q.q + (a2-1) * (hz-hxy.q)^2
W(q) = a2 / (pi * Q(q)^2)
native_response(q) = W(q) * R(q)
```

W is an auxiliary positive quadratic kernel, not the full GGX BRDF or its NDF.
R is computed as the unchanged guarded point response divided by W, retaining
nl>=0, nv>=1e−6 and nh>=0. In particular, back-facing shading normals are not
discarded, nor is a negative unclamped nh silently squared as if positive.
When raw nh>=0, R contains (1+q.q)^2 times Fresnel, visibility and nl.
Its variation and guard boundaries remain approximation errors.

Write Q=(q-mu)'A(q-mu)+delta, where A=I+(a2-1)hxy*hxy',
mu=-A^-1*((1-a2)*hz*hxy), g=hz²+a2*(1-hz²), delta=a2/g. Using the stable Schur
complement avoids subtracting nearly equal constants. For A=L*L', transform
y=L'*(q-mu)/sqrt(delta). Then W dq = sqrt(g)/pi * (1+y.y)^−2 dy.

For each CCW transformed polygon edge a->b, e=b-a, cross=ax*by-ay*bx,
d=sqrt(e.e+cross²), t=atan2(d,1+a.b)/d:

```text
M0 =  0.5 * sum(cross*t)
M1 = -0.5 * sum((ey,-ex)*t)
mass = sqrt(g)/pi * M0
centroid_q = mu + sqrt(delta)*inverse(L')*(M1/M0)
response ~= density * mass * R(centroid_q)
```

These boundary moments follow divergence of y/(1+y.y) and the gradient of
1/(1+y.y). M0 is the cosine form-factor edge integral. It integrates the narrow
kernel exactly; centroid evaluation of the residual is still approximate.
The four-tile candidate partitions each corner at the slope bounding-box
midpoints. Each tile retains its exact kernel mass and centroid. This is not a
fitted LTC BRDF, a Toksvig broadening, or another adaptive point-normal split.
Do not claim an exact full GGX convolution from exact kernel integration.

Independent positive triangle quadrature of the original quadratic checks
90 polygon/roughness/light combinations, including a translated thin triangle.
Four midpoint subtriangles stream per original triangle; orders128/256/512
converge within1e−10 absolute mass/first-moment change. Maximum analytic mass
error1.73e−13, centroid error6.22e−11; limits remain2e−11+2e−9*mass and2e−9
centroid. Tangent-axis permutation preserves mass/centroid; clockwise inputs
are rejected. Centroids must stay inside the convex domain without clamping.
The first unsplit512-vs256 reference did not converge and was rejected; refining
the independent integration changed neither candidate nor selection limits.

## Work and limits

Geometry enumerates original bricks and clips bevel pieces. It is ideal offline
geometry, not bounded runtime statistics; rank-degenerate varying-slope polygons
are explicitly unsupported. Traced build peaks do not include all GEOS/native
allocations. Allocation counts, transient copy bytes and native/device peaks
remain unknown. No whole-frame, load, Place, Metal or A18 proof follows.

Across24 inputs, one/four tiles retain mean1.791667/7.166667 finite domains,
maximum20/80. Edges mean7.166667/28.666667, maximum80/320 per footprint. Numeric
retained arrays alone mean193/580B, maximum1560/5880B; object/allocator overhead
is excluded. Every light/domain evaluates a residual BRDF; every light/edge
uses sqrt and atan2. Kernel transforms additionally solve and factor a2x2
matrix per light. These are different costs from a point-normal BRDF group.
No GPU speedup or memory-budget claim is inferred from Python timings.

Exact narrow-kernel mass is insufficient; increasing the tile count again is
not the next selection step. A new model must address residual curvature and
guard/visibility behaviour with a justified error/cost bound before native
integration. Current guarded shading-normal response remains the oracle;
displaced micro-visibility would be a separate explicit specification change.

## Reproduce and sources

Dependencies: Python, NumPy, Shapely and Matplotlib, plus lib/src on PYTHONPATH.
Generate the existing reference via relief_shading.py --output if unavailable.
Reference report and every input archive are SHA256-bound; all original source
hashes are checked before reuse. Outputs and PDF stay in the system temp tree.

```sh
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 python3 \
  test/experiments/render/relief_finite.py \
  --reference /tmp/outshine-material-relief-shading \
  --output /tmp/outshine-material-relief-finite
PYTHONPATH=lib/src python3 test/experiments/render/relief_finite_report.py \
  --source /tmp/outshine-material-relief-finite \
  --output /tmp/outshine-material-relief-finite/comparison.pdf
```

SIGGRAPH2007 Han et al., Frequency Domain Normal Map Filtering, existing local
PDF pages1/3 optically inspected: preserve the normal distribution. SIGGRAPH2016
Heitz/Dupuy/Hill/Neubelt, [Real-Time Polygonal-Light Shading with Linearly
Transformed Cosines](https://eheitzresearch.wordpress.com/415-2/), author PDF
page4/Fig5/Eq11 optically inspected: analytic cosine boundary integration.
Its fitted full-BRDF approximation is not used. No third-party code or fitting
table is copied. LEAN's Beckmann moments remain a distinct prior alternative.
