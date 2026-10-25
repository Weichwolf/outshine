# Sparse relightable parent response

Owner #1167, shader-work counterexample #1315. This offline material compiler
preserves source node assets and fits their roughness measures into one or two
positive components of the same owned palette. Albedo, geometry, coverage and
original-source remainder do not change. It never averages roughness or normals.

The existing F0=.04/F90=1 dielectric model has roughness-independent diffuse and
grey specular response. At unit mass, only specular needs approximation. For each
material, the training objective is the largest relative direct-response channel
error over fixed light/view directions. Its denominator uses the minimum albedo
channel, which is the worst relative channel in exact arithmetic at equal mass.
Emitted Float32 validation recomputes all RGB channels, including mass roundoff.
No camera, current light, Place or source geometry is a fitting input.

The one-component control checks every palette value. The two-component path
enumerates all palette pairs; each is a convex maximum of affine residuals on a
weight interval. The sign of an active residual's slope brackets a minimizer.
Bounded bisection avoids reciprocals of nearly equal responses. Already sparse
measures retain their original words without fitting. An optional two-variable
HiGHS LP is an independent challenger via the existing SciPy dependency; no
highspy installation. Its forwarded threads option is checked explicitly; other
warnings/failures are not ignored. Solvers never execute in loading/rendering.

Training has1377 fixed cosine/azimuth samples; validation uses1088 shifted samples
including additional mirror directions. Both are finite approximations, not
global response bounds. All pairs are tested, but no globally optimal quantized
material or physical-world error certificate is claimed. One/two/full tradeoffs
remain explicit; no acceptance threshold is inferred from the observed errors.

Only four material arrays are decoded. Full raw node containers are still read
for identity hashes and charged separately. Fit sidecars bind to exact source
files and keep the dense original weight-array shape; they save no field/IO bytes.
They are experimental inputs, not a native cache/binding format. Missing or wrong
fits fail the comparison; loading a valid sidecar performs no fitting. Original
assets remain available. Native cache/version/fallback integration is separate.

The image check changes only material weights, at the existing four root-boundary
views and1280×720/SS16. Opaque/fractional and finer/coarser endpoints are checked;
T must remain word-equal. Pixel differences and GGX evaluations are measured,
not extrapolated into device times. A small error in these elevated day views
does not qualify other highlights: the separate angular validation remains.
Original parent geometry/coverage/normal/contact defects persist. No weather,
night, shadow, native transition, world or device admission follows.

Use `.venv` Python, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`, with
source products from `building_transition.md`:

```sh
python -B -W error test/experiments/render/fit_building_brdf.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --method interval --max-lobes 2 --output /tmp/outshine-brdf-two
python -B -W error test/experiments/render/fit_building_brdf.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --method interval --max-lobes 1 --output /tmp/outshine-brdf-one
python -B -W error test/experiments/render/compare_building_brdf.py \
  --input /tmp/outshine-transition-products \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --one /tmp/outshine-brdf-one --two /tmp/outshine-brdf-two --cells 33 44 \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --native-cell 33 --output /tmp/outshine-brdf-views
```

SIGGRAPH-first review: [Heitz et al.,2016](../../../doc/references/lighting/siggraph/2016-linearly-transformed-cosines.pdf),
p.4/Fig.5 and fitting/storage equations optically inspected. It motivates offline
BRDF-response fitting, but this experiment retains the original GGX basis instead
of fitting LTC transforms; neither its area-light integration nor error metric
is imported. Existing direct-GGX/energy provenance remains `material_filter.md`.
