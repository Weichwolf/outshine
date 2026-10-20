# Exact lit support does not select the finite GGX filter

Owner [#931](https://github.com/Weichwolf/outshine/issues/931), parent
[#422](https://github.com/Weichwolf/outshine/issues/422), material contract #6.
The existing [finite-kernel comparison](relief_finite.md) remains unchanged.
This is a separate rejected support hypothesis, not another fixed tile-count sweep.
No product shader, material, normal, binding, light loop or native recipe changes.

## Hypothesis and preserved contract

For slope q and unit shading normal n=(-qx,-qy,1)/sqrt(1+q.q), direct lighting
has exactly zero contribution when `Lz-Lxy.q <= 0`. Intersect the convex finite
slope polygon with `Lz-Lxy.q >= 0`, then reuse its analytic positive kernel mass
and centroid. The original screen-area/slope-area density remains unchanged;
the retained distribution is never normalized back to full coverage.
Uncut support reuses its original vertices, empty support contributes zero.

The rest of the guarded BRDF still uses `nv=max(n.v,1e-6)`, clamped nh and the
original Fresnel/Smith response. Negative n.v is **not** silently discarded.
There is no new displaced micro-visibility or backface model. Exact zero support
and exact auxiliary kernel moments do not make the centroid residual exact.

SIGGRAPH-first sources remain Han et al. 2007 normal-distribution filtering and
Heitz et al. 2016 [LTC](../../../doc/references/lighting/siggraph/2016-linearly-transformed-cosines.pdf).
LTC PDF page 4, Figure 5 and Equation 11/upper-hemisphere clipping were optically
reopened. Its fitted BRDF tables are not used; this light-slope support derivation
is separate from its polygonal-light application.

## Unchanged full comparison

Regenerate the original independent positive triangle integration in a fresh
temporary directory: 24 declared 2 mm height/bevel footprints, 288 curves,
65 light angles, roughness 0.2/0.5/0.9, view 0°/50° and azimuth 0°/90°.
Reference orders 16/32/64/128 retain the existing convergence test and exact
mean-gradient control. No pins, inputs, weak curves or limits are changed.
Every curve keeps its own original reference peak. Limits are 2% maximum and
0.5% RMS. The original one/four-tile negative controls reproduce exactly.

| Estimator | Failed curves / 288 | Worst maximum / peak | Worst RMS / peak |
|---|---:|---:|---:|
| Original 1 tile | 14 | 22.187021% | 17.409291% |
| Original 4 tiles | 10 | 13.175470% | 10.366810% |
| Exact light support, 1 tile | 14 | 22.187021% | 17.437448% |

The same 14 curve identities fail as in the original one-tile estimator;
six are authored matte curves. Flat, zero-depth, constant-slope and the seven
moving straight-joint responses remain within 1e-11 of the reference.
All curves, response bands and retained glossy/matte counterexamples were
actually opened. These are CPU response diagnostics, not native lit world images.

The worst case is `sheared-corner-0.003`, roughness 0.2, view 50°, light azimuth
90°, light angle -35°. Its single finite corner is entirely lit: the linear
light numerator ranges from 0.819152 to 1.392728 at its vertices, hence throughout
the convex domain. Original and clipped estimates are identically
0.0001010161853 versus reference 0.0001407608029 at that sample.
The original n.v values at its vertices range from -0.087156 to 0.642788,
crossing the current view guard. This isolates why clipping the light support
cannot fix this dominant error; it does not prove the guard is its only cause.
Residual curvature and actual guard behaviour need a different justified model.
Dropping negative-view normals would change the oracle rather than solve it.

## Work and limits

Across all curves, 33,540 support tests visit 134,160 slope vertices: 26,514 full,
6,786 partial and 240 empty. There are 6,786 returned clipped arrays / 434,688
bytes, excluding intermediate arrays and internal GEOS allocations. Analytic
kernel edges fall from 134,160 to 133,224; residual BRDF calls fall only from
33,540 to 33,300. Constant-response evaluations remain 49,920. Support tests,
rotations, polygon intersections, allocations and scalar dispatch are additional
work; stored finite model size remains the original one-tile size.

Query timing runs without tracemalloc; an additional identical replay measures
traced allocation peak and must reproduce response words and work counters.
Tracemalloc excludes some GEOS/native allocations and is not a device peak.
Python query time includes scalar dispatch and is not a GPU speed estimate.
Runs use OPENBLAS_NUM_THREADS=1 and nice 10 with other native work present.
There is no native timing, upload, world-memory, A18/Metal or Iris/Vulkan admission.
Quality already rejects the candidate; no clipping-path optimization or native
consumer follows from this experiment. #422 and the shared far-filter need remain.

## Reproduce

Requires the existing Python NumPy/Shapely/Matplotlib environment and lib/src.
Use fresh system-temp output directories. Normal tests never generate the oracle.

```sh
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error -m unittest discover -s test/experiments/render -p test_relief_lit_kernel.py
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 nice -n 10 .venv/bin/python -B test/experiments/render/relief_shading.py --output /tmp/outshine-relief-lit-reference
PYTHONPATH=lib/src OPENBLAS_NUM_THREADS=1 nice -n 10 .venv/bin/python -B test/experiments/render/relief_lit_compare.py --reference /tmp/outshine-relief-lit-reference --output /tmp/outshine-relief-lit-comparison
```

Six controls cover exact/complementary support, independently converged clipped
kernel integration, axis exchange, full/empty support, unchanged negative-view
response and the fully lit mixed-view-guard counterexample. Source hashes and
reference NPZ digests are checked/recorded. Terminal outputs, original reference
arrays, diagnosis and opened local graphics remain in the delivery's system-temp
evidence directory linked by the PR; no gallery is published.
