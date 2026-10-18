# Filtered needle diffuse response: first angular counterstudy

Owner: `outshine_vegetation`; [#643](https://github.com/Weichwolf/outshine/issues/643),
continuing [#630](https://github.com/Weichwolf/outshine/issues/630).
This is an unshadowed normal-distribution experiment, not a replacement shader.
No recipe, cached plant, native binding, material or published image changes.

## Reference and independently specified model

Han, Sun, Ramamoorthi and Grinspun, SIGGRAPH2007,
[Frequency Domain Normal Map Filtering](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf).
Page3 was also inspected optically: equations2–5 explicitly average the response
over source normals. The paper's normal-map convolution does not supply the
joint camera/light visibility of a three-dimensional needle cluster.

For unit source normals `n`, nonnegative projected-footprint weights `w`, and
unit light direction `l`, the reference is `sum(w * max(dot(n,l),0))`.
Unit albedo/irradiance and a common omitted Lambertian `1/pi` factor apply.
Uncovered samples have zero weight; no coverage is multiplied a second time.
The old comparison shades the normalized weighted mean normal and multiplies
by total coverage. No albedo, needle size, density or exposure is fitted.

The degree2 Legendre projection of `max(t,0)` is
`p(t)=3/32 + t/2 + 15*t*t/32`.
Store nine linearly aggregatable scalars: mass, three first moments, three
cross moments, `E[x*x-y*y]`, and `E[3*z*z-1]`. Unit-normal trace recovers the
three diagonal second moments. This is the degree0–2 SH function space without
requiring a particular normalized SH basis convention.

It is an approximation: `p(0)=3/32`, `p(1)=17/16`, `p(-1)=1/16`, and
`p(-8/15)=-19/480`. The maximum scalar absolute error is `3/32` on `[-1,1]`;
the weighted error is bounded by `3/32 * sum(w)`.
The source polynomial, moment evaluation and rigidly rotated exact responses
have independent scalar checks. Same-mean/different-distribution, zero/gaps,
linear aggregation, invalid input and negative-output controls are included.
Negative values are retained in saved arrays and counted, not silently clamped.
The comparison figure uses a common `[0,1]` display range, so it cannot show
negative excursions; raw fields and counters are authoritative for those.

## Actual bounded source and result

Study code frozen at `1a200009b`; both runs use the already frozen actual
`physical.npz` intervals from #630. No whole plant/axis/window plan is loaded.
Full interval:256needles/58,368triangles; tail:35needles/7,980triangles.
Their source SHA256 values are respectively
`9d2b131047ebef53ad38b2c13085c50e3d7cc0c9fcc4d4235bf5384928345a53` and
`e48373c0fe872b9f6c246d134dbe275b9b168bc679954b181bab664e2ebcb709`.

One fixed camera `[1,0,.35]` normalized; eight lights:camera/opposite and
the six signed world axes. Existing bounded `raster_batch.rasterize` supplies
frontmost interpolated smooth normals on a256×256 orthographic grid; normalize
each covered source sample before shading. Spatial block averages produce
native64²/16²/4²/1pixel fields. These are exact for those finite samples,
not a convergence proof or an exact continuous surface integral. Finite source
sampling and its aliasing are shared by all three compared paths.

| Native16² field | Mean-normal max error | Degree2 max error | Mean RMSE | Degree2 RMSE |
|---|---:|---:|---:|---:|
|256needle interval|.199352|.011436|.030330|.002994|
|35needle tail|.334171|.024747|.041431|.004780|

Errors include all eight directions and all pixels, including gaps.
Degree2 yields291/275negative pixel/light values respectively at native16².
Both2560×1440 comparison figures were opened: mean-normal frontal overbrightness
is visible; degree2 is closer, but these plots do not establish shadow, material,
whole-tree silhouette, species or forest quality. One view is not angular coverage.

## Work, memory and reproduction

Only three NPZ arrays are decoded once:positions/faces/corner normals,
6,328,320B full or865,200B tail. The selected file must be≤16MiB before decode.
Source hashing streams the file separately for provenance; this is diagnostic IO,
not required for each light query. Fixed256² normals/mask retain1,638,400B.
Full/tail have5,571/8,147covered samples. The same samples are reused for all
lights and footprint sizes. There is no repeated plant expansion or BVH build.

Native16² degree2 payload is18,432B Float64 versus8,192B for mass+mean normal;
Float32 would be9,216B versus4,096B but was not validated here. These are
candidate numeric payloads, not accepted per-world-area residency budgets.
Construction visits covered samples once per footprint; the reference additionally
evaluates every sample/light. The deliberately simple comparison repeats input
validation and allocates temporary dot/clamp arrays; its Python wall time does
not predict native cost. Raster temporaries, allocator peaks, GPU memory and
device timing are not measured. No runtime/device performance claim follows.

Private inputs/results/logs are under
`/tmp/outshine-vegetation-needle-response-proof-3a8448f51/` and the #630 input
directory. Full/tail source results and the fresh installed tail replay retain
the same source hashes, actual arrays, implementation hashes and commit.
Use `.venv`, `TMPDIR=/tmp`, one NumPy thread and explicit fresh `/tmp` outputs:

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  -m unittest needle_response_checks -v
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/needle_response_study.py \
  --source /tmp/selected-needle-interval/physical.npz \
  --output /tmp/outshine-vegetation-needle-response-new
```

## Decision and remaining work

Retain the exact finite-sample evaluator as an independent angular reference.
Do not adopt degree2 as a physical shader: its negative/leaking responses are
explicit counterexamples despite lower aggregate error. #643 remains active.
Next evaluate joint source visibility using existing bounded Embree/contact
procedures and one prepared selected source, then compare suitable nonnegative
angular representations under the same support/error/cost conditions. Further
camera directions, sampling convergence, actual shadows, transmission, specular
roughness, multiple scattering, wind and whole-tree/stand images remain open.
