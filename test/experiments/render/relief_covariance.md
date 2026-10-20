# Finite kernel covariance does not repair the centroid residual

Owner #938, parent #422/#6; a bounded follow-up to [#931](relief_lit.md).
The moment identity passes independent integration. The single-centre quadratic
residual candidate is rejected at the original known counterexample. No native
shader, material, roughness, visibility guard, fixture or error limit changes.

## Finite moments and approximation

Reuse the [existing transform](relief_finite.md): `q=centre+inverse*y` and
`W(q)dq=factor*(1+y.y)^-2 dy`, over the same finite convex slope polygon.
These are moments of a finite auxiliary kernel, **not an infinite GGX variance**.
Let M0/M1 be its existing mass/first moment, and `J=integral 1/(1+y.y) dy`.
For counterclockwise edges `a→b`, `e=b-a`, outward `normal*ds=(ey,-ex)dt`:

```text
div[y*log(1+r²)/(2*r²)] = 1/(1+r²)
J = 1/2 * sum cross(a,b) * integral_0^1 log1p(|a+t*e|²)/|a+t*e|² dt
M2_ij = 1/2 * delta_ij * J - 1/2 * boundary_integral y_i*n_j/(1+r²) ds
partial_j[y_i/(1+r²)] = delta_ij/(1+r²) - 2*y_i*y_j/(1+r²)^2
```

The radial integrand has limit1 at zero. Its one-dimensional edge integrals use
SciPy QUADPACK with epsabs/epsrel1e-12 and at most50 subintervals, not an unbounded
2D subdivision or new fixed tile count. The boundary term uses existing atan2
edge integrals and logarithmic first moments. Transform central covariance back
with `inverse*C_y*inverse'`. Numerical negative eigenvalues are checked; no PSD clamp.
QUADPACK error estimates cover only the numerical J term, not floating-point
cancellation in all boundary moments, and are not rigorous whole-integral bounds.

For residual `R=unchanged_native_response/W`, the candidate is
`density*mass*(R(mean)+1/2*Hessian(R):C_q)`. The linear term vanishes at the
kernel centroid. Nine native residual evaluations estimate the Hessian with an
initial1e-3 slope step;3e-4/1e-4 are derivative sensitivity controls, not extra
spatial partitions or fitted quality parameters. Both actual nl/nh clamps and
nv=max(n.v,1e-6) remain; negative-view normals are not discarded.

This Taylor model has no general remainder bound across the actual guard branches.
Even on a smooth domain, a quadratic correction needs a bound on higher residual
derivatives and the support extent. Accurate covariance alone provides neither.

## Counterexample and stop decision

Reuse unchanged `sheared-corner-0.003`: all12 roughness/view/azimuth curves with
65 lights, authored roughness0.9 and glossy0.2/0.5, against the independently
converged original integration. Its archive SHA256 is
`d7b305c0cbac4ca0a6b7a8ebc7c935c2c3931d79e91db19bdec22b5f365d9397`, also retained
by #934. All24 original footprints remain declared; the remaining276 curves are
not newly evaluated because the known counterexample already rejects the candidate.
Original per-curve limits remain2% maximum/0.5% RMS of that curve's own reference peak.

| Model, on these12 curves | Limit-failing curves | Worst max/peak | Worst RMS/peak | Negative samples |
|---|---:|---:|---:|---:|
| Original centroid | 9 | 22.187021% | 17.409291% | 0 |
| Covariance/Hessian,1e-3 | 7 | 25.642923% | 14.373164% | 5 |
| Derivative control,3e-4 | 7 | 25.642757% | 14.373092% | 5 |
| Derivative control,1e-4 | 7 | 25.642742% | 14.373085% | 5 |

All four matte curves still fail. Five negative responses additionally violate
the nonnegative direct-light invariant; three of their curves pass the peak-scaled
limits only because their peaks are large. Error limits alone cannot accept those
curves. Negative estimates are exposed, never silently clamped back to zero.

At the original fully lit glossy mixed-view sample (view50°, azimuth90°, light−35°),
the estimate improves from0.0001010162 to0.0001086633, versus reference0.0001407608.
Its normalized point error remains17.9181%. The worst matte max/peak is instead
at light67.5°:0.0006444962 versus0.0019765721. This domain spans both raw n.v
−0.087156…0.642788 and raw n.l−0.382683…0.382683; a single local Hessian does not
represent its guarded response. Correct second moments do not select this filter.
No higher fixed moment/order/tile sweep or native cost trial follows this rejection.

## Independent checks, work and limits

Six new strict controls cover12 positive triangle-integral comparisons (square,
sheared quadrilateral and translated thin triangle; roughness0.2/0.9; two azimuths),
tangent-axis permutation, quadratic expectations, unchanged constant distributions,
actual negative-view/half guards and rejection of clockwise polygons. Positive
midpoint-subtriangle Gauss integration at64/128/256/512 must converge independently.
Largest mass/mean/covariance errors are6.54e-14/1.16e-11/8.62e-12; fixed limits are
2e-11+2e-9*mass,2e-9 mean and2e-9+2e-7*covariance. Existing six lit-support controls
also pass. Normal tests neither create oracles nor alter source pins.

This one footprint retains its original144B numeric model. Per12-curve query:
780 light/domain evaluations; residual BRDF calls780→7020, unchanged2340 constant
BRDF calls,6240 edge visits,3120 scalar QUADPACK calls/111216 integrand evaluations,
4208 subintervals and6240 additional boundary log1p operations. Boundary matrix
arithmetic, transforms and every scalar callback remain paid work. The measured
query takes0.744s uninstrumented on this Linux/Python host under concurrent load;
a separate word-identical allocation replay peaks at54,504 bytes Python-traced memory.
That is neither complete native QUADPACK/GEOS allocation accounting nor a process,
GPU, device-memory or frame-time result. Unknown allocation counts/copy bytes stay
unmeasured. No world image improvement, native filter or far-field winner is claimed.

## Reproduce and sources

Existing NumPy, Shapely, SciPy and Matplotlib environment; lib/src on PYTHONPATH.
Generate the unchanged original reference with relief_shading.py if unavailable.
Source hashes and the selected original archive digest are retained in the report.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render \
  .venv/bin/python -B -W error -m unittest test_relief_kernel_covariance test_relief_lit_kernel
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render nice -n 10 \
  .venv/bin/python -B -W error test/experiments/render/compare_relief_covariance.py \
  --reference /tmp/outshine-original-relief-reference \
  --output /tmp/outshine-research-kernel-covariance-<commit>/release
```

SIGGRAPH-first: Han et al.2007, Frequency Domain Normal Map Filtering, original PDF
p3/effective BRDF/NDF opened; Heitz et al.2016, [LTC](https://eheitzresearch.wordpress.com/415-2/),
original PDF p4/Fig5/Eq11 opened. Their filtering and boundary-integration context
does not prove this separate second-moment identity or Taylor candidate. No fitted
LTC tables or third-party code are copied. [SciPy's primary quad documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.integrate.quad.html)
defines numerical error estimates/work counters. Diagnostics remain local in the
system-temp proof directory; no images published. Full #422/#6/#899 admission stays open.
