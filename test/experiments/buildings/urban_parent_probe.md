# Direct urban parent research (#1113)

Active #1083 / Draft #1115. The goal remains a filtered, relightable coarse parent
before fine-child generation. None of the experiments below admits a native/World
replacement, Empty certificate, device/p99 result or temporal transition guarantee.
All diagnostic images and raw outputs remain in system temp.

## Source and exact controls

`urban_parent_stats.py` compiles coarse region/exclusion evidence directly: zero
children,6,264 array bytes in the original case. Centre-area setbacks and inferred
height probabilities are approximations; missing original heights are unknown.
Zero probability, XY support and asset existence grant no replacement readiness.
The original resolved owner arrays remain unchanged (247,731 masses/5,962 patches).
Their original source pins differ from the newer payload now returned at the same
owner URI; comparisons here retain the original155,990B/d1f2e006… input.

`urban_parent_conditional.py` is the categorical recipe control. Heights in one cell
are mutually exclusive. Conditional shadows preserve primary misses, exclude the
known own hit cell, and keep camera/caster replacement domains independent. Direct
prefix intersections avoid miss histories; coincident wall hits share exact shadow
visibility while retaining separate height labels. Original box/phase/source
controls remain in the focused tests and prior evidence linked below.

`urban_parent_layer.py` composes urban C/T/S at each actual opaque stop as
`C + T*(S*B_direct+B_other)`. Existing non-parent visibility belongs in B_direct;
ambient is not multiplied by S. Camera/caster readiness includes complete empty
child cells. The filtered signed correction to a preserved opaque baseline is
`D=C+(T*S-1)*B_direct+(T-1)*B_other`; do not clamp negative D or independently average
C/T/S. Full readiness gives D=0 and bypasses parent traversal. Native World bounds,
terrain/curvature, original-height evidence and readiness remain #1102.

## Current step: finer geographic evidence at fixed M8/B16

The existing direct16/direct64 source parents were compared without rebuilding
sources or children. Both retain395 pinned records, identical frame/heights/
probabilities and the same global area means (difference below2e-12). Only the
geographic distribution read by bilinear density lookup changes. This isolates
source geography from camera sample density, the transport algebra and known
geometry. No lookup preparation, new dependency or runtime change was introduced.

At runtime freeze `01030019b`,36 fresh cases cover both parents, six Ready/
foreground states and day/back/night, with the same full247,731-mass resolved
control and per-profile ROIs inside1280x720,240km/75,894.663844m camera. Every run
exits0. All16er raw arrays match the preceding M8/B16 freeze bit-for-bit; both
parents preserve B16 and full-Ready child RGB exactly. Raw composites remain
nonnegative with weights in[0,1]. No input/source/recipe/profile/default changed.
All36 actual PNG contents are decoded/verified and optically reviewed in three
local sheets, alongside dense references and common-scale error maps. Day detail
is less uniformly blurred; the half-Ready split and remaining night errors persist.

| Light / state |16er RGB RMSE |64er RGB RMSE |16er total s |64er total s |
| --- | ---: | ---: | ---: | ---: |
| day / pending | 0.018849 | 0.015883 | 6.951 | 7.208 |
| day / half_view | 0.025077 | 0.023978 | 7.051 | 7.047 |
| day / half_caster | 0.027651 | 0.023903 | 10.727 | 11.254 |
| day / half_ready | 0.015807 | 0.013640 | 9.225 | 9.503 |
| back / pending | 0.011499 | 0.011317 | 4.954 | 5.142 |
| back / half_view | 0.020120 | 0.020033 | 5.283 | 5.455 |
| back / half_caster | 0.022282 | 0.020652 | 8.566 | 8.575 |
| back / half_ready | 0.011394 | 0.011270 | 7.465 | 7.447 |
| night / pending | 0.105892 | 0.115359 | 15.450 | 16.143 |
| night / half_view | 0.168109 | 0.167233 | 12.596 | 13.398 |
| night / half_caster | 0.186547 | 0.187881 | 25.138 | 26.450 |
| night / half_ready | 0.095858 | 0.099447 | 18.384 | 19.178 |

Errors are whole ROI pixel linear-RGB RMSE against the same mean of two64x64
opaque reference seeds; foreground is a different physical scene and has no bare
reference error. Reference seed disagreements day/back/night remain
0.001557/0.002117/0.014662, observed uncertainty rather than certified error bars.
Per-seed errors and exact current controls are retained. Small gains do not prove
convergence or an equal-quality speedup. Full-Ready errors are unchanged.

Half-view->half-Ready jump RMSE16er/64er is day0.021994/0.022222,
back0.018881/0.018845 and night0.135923/0.134627. Other Ready edges remain
mixed: night pending->half-view0.159240->0.164068 and half-Ready->full-Ready
0.075545->0.077905 get worse. Eight day/back non-full-Ready errors improve;
three of four night errors get worse. Every direction also holds against each
reference seed separately, without establishing a general error bound.

A signed algebraic diagnostic separates each half-view->half-Ready jump:
`delta F = delta C + mean(a)*delta B + mean(B)*delta a`.
Here delta C is exactly zero. The two remaining terms are correlated and partially
cancel; their RMS values must not be added or interpreted as independent errors.
For night16er, background/weight term RMS is0.092042/0.214408,
while total jump RMS is0.135923. The identity residual is below8.9e-16 across
both parents/all three lights. This locates a transport/readiness discrepancy,
not a geometry repair or proof of its general cause.
Existing moment arrays also recover the same-node complete8 image as mean(C+R),
without another query. Its16er RGB agrees with the original direct8 control within
floating precision; this distinguishes source-model and normalization changes.
Night pending source16->64 already worsens complete8 error
0.105889->0.115349; the transfer change is only about9e-5 RMS. Half-Ready
complete8 also worsens0.150429->0.152736, although dense-background transfer
reduces both errors. Finer geography is therefore not rejected merely because
of an isolated normalization artifact; neither effect fixes the Ready mismatch.

Source construction costs are reused from the pinned `c431da988` study, not freshly
measured: direct16/direct64 use6,264/98,424 array bytes and6,637/83,366 compressed
file bytes; direct compilation7.206/50.492s. The already verified recursive64
compiler produces equivalent imagery in13.892s, but does not qualify64er quality.
Both source variants generate zero child cells. No GEOS/source preparation occurs
in the current pixel queries. Actual traversal work, logical query/packet copy
bytes, output arrays, preparation and RSS are retained per phase. Finer q can
change zero-density shadow work; geometry-work dictionaries need not be identical.
Fresh-process RSS spans144,310,272..148,123,648B including imports and
prepared arrays, not an app/device budget. Pixel rays, ten-field/output capacity
and logical packet/camera-copy counts stay identical between source resolutions;
source64 does not reduce parent ray demand. Shared interactive Linux/i5 CPU host,
fixed16-then64 order and one call per case; brief nice10 numeric summaries and
small review-sheet assembly overlap some cases. The separate profiler runs before
the matrix. These costs are observational, not isolated/statistical/native gains.
Even identical full-Ready images show timing variation, with no parent queries.

A separate preflight profile of1,700 night half-Ready rays observes317 whole-plane
midpoint reductions per scene query. For16/64, centre_fraction consumes
0.0093/0.0411s of instrumented0.4546/0.5135s (about2/8%). The mean output alone
allocates317*256*8/317*4096*8 logical bytes (649,216/10,387,456B); this excludes
other arrays and is not physical traffic, process peak or native time. Lookup is
measurable but not the dominant cost. No caching/preparation rewrite is justified
as a solution to these image/Ready errors by this profile alone.

Keep direct16 as the source default;64 remains an optional measured alternative.
Further source-resolution or camera/CDF sweeps are not the next action. The next
bounded model investigation targets the camera/caster Ready transport mismatch,
with identical physical inputs and all existing child/foreground identities. No
smoothing/clamping may erase known geometry or hide unresolved failure cases.
The native world, fast movement/streaming, curvature, full contents and device/
p99 contracts remain unproved by static flat-ENU ROIs. No source-height/occluder,
Empty/Ready certificate or new persistent view-moment asset follows.

Complete commands, immutable inputs, phase costs/work, unclamped arrays, per-seed
errors, jump decomposition, negative controls and actual optical review:
`/tmp/outshine-research-geographic-parent-01030019b/receipt.json`.
The preceding joint-moment derivation, tests, sources, all cost/error/bias cases and
reproduction remain pinned in [01030019b](https://github.com/Weichwolf/outshine/blob/01030019b/test/experiments/buildings/urban_parent_probe.md).

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
| Joint weighted Ready moments | Nonnegative composites and exact equal-grid/full-Ready identities; ratio bias, cost and Ready jumps remain | [01030019b](https://github.com/Weichwolf/outshine/blob/01030019b/test/experiments/buildings/urban_parent_probe.md) |
| Dense Ready baseline plus raw D | Same-grid/Ready identities and favorable cases, but negative composite pixels; general use rejected | [e198a046d](https://github.com/Weichwolf/outshine/blob/e198a046d/test/experiments/buildings/urban_parent_probe.md) |
| Joint deterministic Ready pixel integration | Lower aliasing/error, child baseline preserved; costly finer grids and remaining Ready jumps | [d6f037c33](https://github.com/Weichwolf/outshine/blob/d6f037c33/test/experiments/buildings/urban_parent_probe.md) |
| Bounded opaque pixel reference | Same fixed rays with lower temporary memory; seed/density uncertainty and fixed4 parent limits | [8b00c2b84](https://github.com/Weichwolf/outshine/blob/8b00c2b84/test/experiments/buildings/urban_parent_probe.md) |
| Recursive source areas | Same64er images,3.63x faster compilation;16er slower, no general resolution promotion | [4c022df0b](https://github.com/Weichwolf/outshine/blob/4c022df0b/test/experiments/buildings/urban_parent_probe.md) |
| Absolute screen CDF addresses | Cropped/tiled queries share samples; static window artifact removed, higher memory and remaining temporal/model errors | [06bcac0e3](https://github.com/Weichwolf/outshine/blob/06bcac0e3/test/experiments/buildings/urban_parent_probe.md) |
| Pixel-wise CDF coupling | Lower mean error in fixed-seed sets, negative seeds/time/memory cases; crop-relative streams | [05ea51345](https://github.com/Weichwolf/outshine/blob/05ea51345/test/experiments/buildings/urban_parent_probe.md) |
| Sampled finite-stop/ready composition | Joint C/T/S, exact full-ready unions and explicit contained-shadow query; night noise/peak costs remain | [004983530](https://github.com/Weichwolf/outshine/blob/004983530/test/experiments/buildings/urban_parent_probe.md) |
| Shared first-hit geometry | Same pixels with1.12-1.78x CPU-query gain; geometry memory lower, extra threshold comparisons | [0f5f2e2ef](https://github.com/Weichwolf/outshine/blob/0f5f2e2ef/test/experiments/buildings/urban_parent_probe.md) |
| Independent sampled first-hit RGB | Deterministic T/background, finite surface sampling; ground gain and night variance, elevated regressions | [efe416498](https://github.com/Weichwolf/outshine/blob/efe416498/test/experiments/buildings/urban_parent_probe.md) |
| Sparse prefix inversion/scout | Equal events,7.1x sparse subcase gain; full-city and allocation regressions, wide/numeric fallbacks linear | [a191eac03](https://github.com/Weichwolf/outshine/blob/a191eac03/test/experiments/buildings/urban_parent_probe.md) |
| Affine primary prefixes |11.2x/7.2x/2.6x scalar-query gain with unchanged Coverage; higher transient memory, no RGB alone | [932ae177e](https://github.com/Weichwolf/outshine/blob/932ae177e/test/experiments/buildings/urban_parent_probe.md) |
| Pixel-area support filter | Far-day correction error7.4x lower; elevated/night remain unqualified, extra work | [eab999e26](https://github.com/Weichwolf/outshine/blob/eab999e26/test/experiments/buildings/urban_parent_probe.md) |
| Affine categorical shadows |1.85-1.94x low-sun transport gain with unchanged PNGs; primary traversal/model errors remain | [f6c5727bc](https://github.com/Weichwolf/outshine/blob/f6c5727bc/test/experiments/buildings/urban_parent_probe.md) |
| Rational atomic prefix | Exact lattice counts; slow pixel packets and broad intervals, retained as geometric oracle | [425f1d199](https://github.com/Weichwolf/outshine/blob/425f1d199/test/experiments/buildings/urban_parent_probe.md) |
| Coarse joint Poisson void | Correlation improves errors1.97-2.38x; GEOS slow and continuous centres destroy lattice corridors | [99fca1fec](https://github.com/Weichwolf/outshine/blob/99fca1fec/test/experiments/buildings/urban_parent_probe.md) |
| Projected M/J/D/S field | Cheap pixel sampling, cell-dependent construction, blurred boundaries/highlights and horizon bias; not default | [9e87685b8](https://github.com/Weichwolf/outshine/blob/9e87685b8/test/experiments/buildings/urban_parent_probe.md) |
| Direct conditioning/wall sharing | Exact colour/coverage preserved; stored history removed and coincident wall visibility reused | [823c31fe0](https://github.com/Weichwolf/outshine/blob/823c31fe0/test/experiments/buildings/urban_parent_probe.md) |
| Phase/shadow transfer/signed correction | Better joint filtering; still limited phase kernels and reference-sensitive night error | [3ba4d5a12](https://github.com/Weichwolf/outshine/blob/3ba4d5a12/test/experiments/buildings/urban_parent_probe.md) |

Prior raw receipts remain under `/tmp/outshine-research-urban-` with suffixes
`runs-f25db1d43`, `prefix-d4bbacf61`, `poisson-7cc596588`, `response-8282c30a3`, `wall-fdc98f113`, `horizon-3ba4d5a12`,
`layer-filter-86969ff1e`, `layer-897451cc6`, `adaptive-cf6a67e95`,
`transfer-c2bc91d25`, `phase-6f84f25f3` and `parent-1edb4404d`.

## Reproduction

Use the existing .venv with
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`.
The source compiler runs before the independent resolved bake; subsequent probes
consume only the saved parent. Choose fresh system-temp output directories.

```sh
python -B -W error test/experiments/buildings/compare_urban_pixel_reference.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --lighting night \
  --state half_view --samples 8 --background-samples 16 --weighted-transport \
  --batch 8192 --output /tmp/outshine-weighted-moments
python -B -W error test/experiments/buildings/compare_urban_pixel_reference.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --lighting night \
  --state half_ready --samples 8 --background-samples 16 --batch 8192 \
  --output /tmp/outshine-control-pixels
python -B -W error test/experiments/buildings/compare_urban_pixel_reference.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --lighting night \
  --state half_ready --samples 16 --batch 8192 --output /tmp/outshine-ready-pixels
python -B -W error test/experiments/buildings/compare_urban_pixel_reference.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --lighting night \
  --samples 64 --seed 1113 --batch 8192 --output /tmp/outshine-dense-reference
python -B -W error test/experiments/buildings/urban_parent_stats.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --divisions 64 --area-method hierarchical --output /tmp/outshine-parent-areas
python -B -W error test/experiments/buildings/compare_urban_parents.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-control --samples 16 --jitter-seed 1113
python -B -W error test/experiments/buildings/compare_urban_horizon.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --shadow-runs --paired-exact \
  --surface-samples 4 --sparse-probe --lighting day \
  --output /tmp/outshine-urban-first-hit --distance 50000 --altitude 2
python -B -W error test/experiments/buildings/compare_urban_layers.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --surface-samples 4 \
  --paired-sampled --pixel-strata --screen-strata --foreground-fraction 0.75 --distance 50000 --altitude 2000 \
  --output /tmp/outshine-urban-sampled-layers
```

Set PARENT_NPZ/RESOLVED_NPZ to the saved original inputs pinned in the reports;
do not refresh the changed owner URI. Omit distance/altitude for50km/2m;
`--motion 8000` changes the camera;35000 gives the diagonal case.
Other retained drivers: `compare_urban_horizon.py`, `compare_urban_response.py`,
`compare_urban_poisson.py` and `compare_urban_layer_filter.py`; their pinned method
versions document sampling, independent controls and qualification limits.

## Primary sources read and visually inspected

- [Shapely intersection](https://shapely.readthedocs.io/en/stable/reference/shapely.intersection.html) and [covers](https://shapely.readthedocs.io/en/stable/reference/shapely.covers.html): existing GEOS-backed set/predicate semantics. The recursive partition and counter model are independently implemented; no vendor code is bundled.

- [NumPy Philox reference](https://numpy.org/doc/stable/reference/random/bit_generators/philox.html), counter/state and four uint64 words per block; checked against installed docstrings. We use the existing library generator, not a new RNG implementation.

- [Cook/Porter/Carpenter1984, SIGGRAPH](https://people.computing.clemson.edu/~ekp/courses/dpa8090/assets/papers/Cook1984DistributedRayTracing.pdf), pages137/141, section6/Figure2: distribute existing samples across integration dimensions. Both pages optically inspected. The pixel CDF coupling/proof here is separate.
- [PBRT4 section8.5](https://www.pbr-book.org/4ed/Sampling_and_Reconstruction/Stratified_Sampler), padding and random association of stratified dimensions; independent implementation using existing NumPy, no PBRT code bundled.

- [Bruneton/Neyret2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section3/Figure2/equations1-6: joint visible/lit proportions and projected overlap. The box/atomic derivations here are separate.
- [PBRT4 section2.3.1](https://www.pbr-book.org/4ed/Monte_Carlo_Integration/Sampling_Using_the_Inversion_Method): standard discrete-CDF inversion. Our vectorized categorical sampler and conditioning are independently implemented; no PBRT code is bundled.
- [NIST DLMF5.11](https://dlmf.nist.gov/5.11), equation5.11.1 and section(ii): positive-real logGamma/Stirling expansion and first-neglected-term analytic remainder. Floating implementation accuracy is checked separately.
- [AtCoder floor_sum specification](https://atcoder.github.io/ac-library/production/document_en/math.html): floor-sum definition and bounded-integer logarithmic contract. Our signed/unbounded arithmetic uses an independently implemented Euclidean recurrence; no foreign library code is bundled or its native timing claim inherited.
- [Feibush/Levoy/Cook1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section2.4/Figure2, PDF pages3-4: clipping and weighting in the actual pixel footprint. Re-read optically for this step; the coarse support/strip quadrature here is separate.
- [Han et al.2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures1-2/section3: normal filtering excludes occlusion/geometry simplification/cast shadows.
- [Horizon cloudscapes2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF pages37/81 (slides35/79): coverage/height and alpha1 exit; our finite-error analysis is not attributed to these slides.
- [Sloan/Kautz/Snyder2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction/Figure2/PDF page2: transport/lighting separation, without a general SH/GI guarantee here.
- [Enderton et al.2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf), PDF page2/section3.1/equations1-2: premultiplied depth contributions, no GPU/OIT implementation adopted.
- [Mueller et al.2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page1/Figure1/equation1: baseline-plus-difference identity only; no learned model or unbiasedness claim.
