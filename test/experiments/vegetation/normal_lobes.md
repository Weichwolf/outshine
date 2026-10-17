# Positive normal distributions on actual needle intervals

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
The old normalized mean normal overbrightens front light and loses side response.
This study preserves several positive normal moments instead, without changing
albedo, density, needle dimensions, source positions or material parameters.
It addresses **unshadowed opaque diffuse normal response only**, not full PBR.

## Method and source

Han et al., SIGGRAPH2007,
[Frequency Domain Normal Map Filtering](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf),
page5/Figure4 and equations11–12 were also inspected optically: normal distributions
and their convolution matter, not just a normalized average. Our positive bins
and finite-measure bounds are own reference mathematics, not that paper's complete
filtering method or a claim about SpeedTree internals. The earlier polynomial
[alternative](needle_response.md) remains rejected because of negative/leaking values.

Reuse the existing [projected-area integrator](needle_area_light.md), source
normals, GEOS clipping and camera visibility. Capture visible unit normals and
positive footprint weights after camera contacts, in packets≤8192 and≤2million
samples/query. No second raytracer, whole plant expansion or104-light ray pass.
The coaxial direct response independently equals the collector's first query.

Compare4 camera-frame quadrants,6 signed dominant-object-axis bins and their
nested24-bin refinement. Store mass and weighted first moment per bin; the
[installable library contract](../../../lib/normal_distribution.md) owns the exact
API, Jensen-gap proof, energy/quantization bounds and filtering restrictions.
A sum of clamped moment dots is nonnegative and a lower approximation for the
same finite samples. The mass-preserving variant rescales each bin to its mass;
it preserves isotropic energy but may overestimate individual light directions.
Neither representation is an absolute continuous-source visibility certificate.

The actual frozen #630 inputs are256needles/58,368triangles and35/7,980.
Their archive SHA256 values are
`9d2b131047ebef53ad38b2c13085c50e3d7cc0c9fcc4d4235bf5384928345a53` and
`e48373c0fe872b9f6c246d134dbe275b9b168bc679954b181bab664e2ebcb709`.
Only positions/faces/corner normals decode:6,328,320/865,200bytes.
Camera directions normalize(1,0,.35) and held-out(.2,.9,-.39).
For each camera query104 lights:camera/opposite, signed worldXYZ,96 fixed random
unit vectors (seed643). Compare native16² then raw-moment box MIPs8²/4²/2²/1.
No fit to those light directions. Quadrant frames belong to each product.

## Actual result

Maximum absolute response error, all104lights/native16² pixels, including gaps.
Unit incident diffuse; common Lambertian1/pi omitted on every side.
Original runs freeze9ef6e43df; held-out runs d4e963f2b. Promotion to lib retains
valid-input arithmetic. `unit` means mass-preserving, not unit-length output.

| Interval / camera | Mean normal | Lower4 | Unit4 | Lower24 | Unit24 |
|---|---:|---:|---:|---:|---:|
|256 / original|.191130|.019386|.019295|.010510|.010467|
|35 / original|.318634|.033745|.033387|.021775|.021539|
|256 / held-out|.283185|.077307|.075882|.029545|.027504|
|35 / held-out|.267532|.066412|.062463|.036512|.035528|

Full original RMSE falls .025065→.001668(lower4)/.001655(unit4).
Full held-out RMSE .043716→.005322/.005696. Frontal max errors for those
full intervals .191130→.000063/.007859 and .272230→.000162/.034836.
These are errors against the **same finite source measure**. Source quadrature
uncertainty remains separate; [held-out area controls](needle_area_directions.md)
show why first-view convergence cannot certify every view/shadow.

All lower/mass-preserving finite-measure bounds pass104directions at all5MIPs.
Lower24 is never below its nested6parent (up to roundoff);4vs6 has no ordering.
In these cases lower4 is more accurate than6 despite fewer bins. It is view
conditioned and cannot silently mix frames across camera products.

Lower4 loses isotropic energy: original full mean .02072184 versus source
.02131490; held-out .03603792 versus .03757994. Unit4 preserves those exact
source isotropic means, but restores some directional overbrightness. This is
an explicit tradeoff, not a free energy-preserving lower bound.

Four2560×1440 comparisons were actually opened: common[0,1] grayscale,
nearest native16² pixels, exact/mean/lower4/unit4/lower24 for three declared
lights. Front overbrightness improves visibly; side illumination is recovered.
These are response diagnostics, **not whole-tree/forest render acceptance**.
Original plots were replayed from unchanged arrays at d4e963f2b; their separate
plot-replay.json records plot commit and input/image hashes.

## Work and retained bytes

| Interval / camera | Visible samples | Camera rays | Ray-input copied B | Build wall s |
|---|---:|---:|---:|---:|
|256 / original|524852|589248|16498944|4.735|
|35 / original|96605|108272|3031616|.470|
|256 / held-out|392533|596384|16698752|28.781|
|35 / held-out|80698|108800|3046400|5.502|

CPU diagnostic timings, overlapping other host work; not isolated same-cost
ranking, GPU timing or RTX3060/A18 performance. Linux/WSL2 x86_64,4CPU affinity,
16,578,588,672B RAM, Python3.13.5/NumPy2.5.4/Shapely2.2.0/GEOS3.14.1,
one numerical thread. Visibility/GEOS temporaries and allocator peaks are not
fully instrumented. No frame/world budget accepted from these local numbers.

At16², lower4/6/24 direct vectors cost6,144/9,216/36,864B in FP16,
12,288/18,432/73,728B in FP32. Raw Float64(W,M) fields retained together by
the study cost278,528B; exact104-angle reference212,992B. Both are diagnostic,
not proposed per-world-area residency. Per query evaluation batches≤8directions;
its Float64 dot temporary is at most8*P*B*8bytes plus clamp/reduction outputs.
The reference collector additionally repeats bincount/array allocations for
all104directions; production queries would not retain that exact reference.

Direct FP16 vectors were evaluated and satisfy their explicit rounding bound.
Raw FP16(W,M) sizes in JSON are **unvalidated estimates**: rounding may violate
the moment cone and nonlinear renormalization needs its own codec controls.
For mass preservation across filtering, filter raw moments then resolve anew;
hardware MIP filtering of already normalized vectors is not equivalent.
No native texture packing, binding, world cache or material format is prescribed.

## Controls and reproduction

Five library controls plus5existing leaf-normal/8new distribution/27prior
source-area/joint-response controls pass. Independent scalar sums, same-mean
counterexamples, sharp Jensen gap, rotations, nested bins, mass/isotropic energy,
zero-resultant refusal, invalid moments/frames/encodings and source-hook unchanged
outputs are covered. The installable math module imports NumPy only; collector
and actual geometry remain experimental. No import from lib into experiment code.

Use a `.venv`, TMPDIR=/tmp and one numerical thread; output must explicitly be
under/tmp and not already exist:

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation .venv/bin/python \
  -m unittest test_normal_distribution test_leaf_normals normal_lobe_checks \
  needle_area_demand_checks needle_area_checks needle_joint_checks needle_response_checks
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/normal_lobe_study.py \
  --source /tmp/selected-needle-interval/physical.npz \
  --view .2 .9 -.39 --output /tmp/outshine-vegetation-positive-normals-new
```

Proof inputs/results are private under
`/tmp/outshine-vegetation-normal-lobes-proof-14a80e1e5/` and the retained #630
source directory. No server, public research gallery or generated binary asset.

## Decision and remaining work

Publish the small reusable positive-distribution math kernel and diagnostic
comparison. Four bins are a useful low-cost candidate;24 buys angular accuracy
with more storage. Neither is a new default asset/shader. Next apply a selected
candidate to actual canonical prototypes and whole-tree comparison, keeping
joint camera/light visibility coherent instead of multiplying independent
coverages. R/T, colour/normal correlation, specular/roughness, multiple scattering,
wind, continuous-source uncertainty and whole-tree/stand cost/quality remain open.
