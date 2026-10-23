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

## Current step: jointly integrate deterministic Ready composition

The previous reference step left parent expectations at fixed4x4 camera rays.
Those ray-grid errors must not be mistaken for a better source model or transition.
The same direct16 parent, original resolved field, camera/lighting and six existing
Ready/foreground states are now integrated at fixed8x8 and16x16 spatial strata.
There is no added surface/CDF Monte Carlo, temporal history, changed geometry,
source refresh, new material/shadow path or fading of known children.

`compare_urban_pixel_reference.py --state` selects the existing complete composed
RGB callback. Every node re-queries actual child/foreground stops, normals,
materials and casters, then evaluates the joint C/T/S expression before averaging.
Separately averaged T/S/background products are not equivalent. Camera/caster Ready
regions remain independent, including known vacancies and complete Ready unions.
The resolved-only default still records child-hit coverage; composed states save
RGB only, not an invented child-coverage measurement. Their unused integrator
channel is one for the already composed opaque ground/environment background.
No dependency or native/world/cache change is introduced.

Work aggregation adds visits/tests/copies and query times across packets. Live
array/conditioning peaks use maxima, not sums. Output buffers, per-pixel camera
rays, packet copies and structural allocation counts retain the preceding bounded
integrator contract; these are not total allocator or physical bandwidth counts.
The original full field is an offline opponent, never a required fine build for
the source-only parent compiler/consumer. Preparing inputs remains outside queries.

Freeze `78d9149d1`:177 focused tests terminal OK in26.242s. Three new tests include
an anti-correlated T/S analytic case that fails separate filtering/clamping,
comparison with direct existing per-ray composition in all seven modes, full-Ready
opaque equivalence and parent/child-array immutability, plus real Ready-boundary
crop/packet identity. Existing81-world, finite-stop, independent Ready, conditional
shadow and source/cache tests remain. A pre-freeze original-field half-Ready fixed4 crosscheck reproduces
the preceding result within2.67e-15. All36 density/state/light jobs terminate0;
all36 new RGB PNGs are optically represented in three local comparison sheets.

| Light / state | Fixed4 RGB RMSE | Fixed8 | Fixed16 |
| --- | ---: | ---: | ---: |
| day / pending | 0.022896 | 0.018849 | 0.017574 |
| day / half_view | 0.031308 | 0.026459 | 0.024748 |
| day / half_caster | 0.030637 | 0.027373 | 0.025682 |
| day / half_ready | 0.026402 | 0.017714 | 0.015068 |
| day / all_ready | 0.028208 | 0.015358 | 0.010716 |
| back / pending | 0.016896 | 0.011499 | 0.010359 |
| back / half_view | 0.031051 | 0.022775 | 0.019964 |
| back / half_caster | 0.025733 | 0.021702 | 0.020761 |
| back / half_ready | 0.029924 | 0.015705 | 0.010953 |
| back / all_ready | 0.038833 | 0.018440 | 0.011819 |
| night / pending | 0.133905 | 0.105889 | 0.086854 |
| night / half_view | 0.212506 | 0.203943 | 0.180386 |
| night / half_caster | 0.148365 | 0.184677 | 0.165078 |
| night / half_ready | 0.207658 | 0.150429 | 0.094022 |
| night / all_ready | 0.263506 | 0.158283 | 0.090680 |

All errors are complete pixel linear-RGB RMSE against the mean of the same two
64x64 opaque-field references. This comparison uses the same physical scene:
foreground has an additional plane and is excluded from bare-reference errors,
while its geometry/requery/sampling controls and images remain. The all-Ready
row is finite child-ray integration error, not missing/altered child geometry.
Night full-Ready16 agrees with the previous original opaque fixed16 control to
exactly zero; this does not imply convergence to the64 mean.
Reference uncertainty is inherited: seed RGB disagreement at64 is0.001557 day,
0.002117 back and0.014662 night. These are observed discrepancies, not certified
bounds/confidence intervals. Remaining16-vs8 grid differences and reference error
are not zero. The residual at16 is not an exact isolated model-error estimate.

| Light / Ready change | Fixed4 jump RMSE | Fixed8 | Fixed16 |
| --- | ---: | ---: | ---: |
| day / pending->half_view | 0.030910 | 0.026979 | 0.026700 |
| day / half_view->half_ready | 0.024339 | 0.021826 | 0.022004 |
| day / half_caster->half_ready | 0.028527 | 0.023360 | 0.022664 |
| day / half_ready->all_ready | 0.019388 | 0.012768 | 0.010747 |
| back / pending->half_view | 0.028205 | 0.020881 | 0.018903 |
| back / half_view->half_ready | 0.021451 | 0.018811 | 0.018914 |
| back / half_caster->half_ready | 0.029642 | 0.021490 | 0.020552 |
| back / half_ready->all_ready | 0.024792 | 0.011874 | 0.007447 |
| night / pending->half_view | 0.225818 | 0.191458 | 0.166041 |
| night / half_view->half_ready | 0.185551 | 0.127283 | 0.147929 |
| night / half_caster->half_ready | 0.203779 | 0.141576 | 0.142335 |
| night / half_ready->all_ready | 0.173802 | 0.062249 | 0.046609 |

These are static image differences when readiness changes at fixed physical
inputs/camera, not a qualified temporal animation. Stable8/16 jumps in day/back
support a remaining representation mismatch; supersampling alone is not a qualified fix.
Night must be assessed with its larger remaining quadrature/reference uncertainty.
The exact observational identity is `(P4-R)^2 = (P16-R)^2 + (P4-P16)^2 + cross`,
averaged over pixels/channels, with `cross=2*mean((P16-R)*(P4-P16))`. The terms are
not independent error percentages. The night caster-only negative and cross-term
cancellation remain in the full table; denser sampling is not uniformly better.

| Light / state | Fixed8 integration s | Fixed16 s | Fixed16 process peak RSS B |
| --- | ---: | ---: | ---: |
| day / pending | 6.743 | 24.748 | 144,273,408 |
| day / half_ready | 6.511 | 26.184 | 144,326,656 |
| day / all_ready | 1.192 | 4.325 | 144,568,320 |
| back / pending | 4.480 | 18.102 | 144,080,896 |
| back / half_ready | 5.010 | 16.396 | 144,150,528 |
| back / all_ready | 1.274 | 3.981 | 144,027,648 |
| night / pending | 15.923 | 58.950 | 143,962,112 |
| night / half_ready | 14.629 | 50.767 | 144,515,072 |
| night / all_ready | 1.569 | 5.853 | 144,359,424 |

At16, day/back query399,360 rays per state, night435,200. Camera packet writes
are19,169,280/20,889,600B plus3,194,880/3,481,600B owner indices; all six states keep
the same original247,731 masses in616x624. Work also records parent/child visits,
height tests, shadow steps and array peaks. The unused alpha/output channel and
all queries count. Peak RSS includes imports, prepared fields and native/NumPy
allocations; it is neither isolated temporary memory nor an app/device budget.
Shared host with foreign CPU builds/shader tools, no own broad tests/profiling or
image processing during normal timings; single calls give descriptive costs.
Fixed16 is a diagnostic reference/control, not a proposed real-time sampler.

These projected ROIs inside1280x720 at flat ENU240km/75,894.663844m do not prove
complete World frames, curvature, devices, p99 or a filtered temporal transition.
Direct16 source and render defaults stay; all previous variants/negative cases are
preserved. No automatic source-resolution promotion follows from these new data.
Complete commands/input hashes, terminal logs, unclamped arrays, work/peaks,
algebraic attribution and optical review:
`/tmp/outshine-research-ready-pixels-78d9149d1/receipt.json`.

The prior bounded opaque reference, equal-ray190MB->5.5MB temporary-memory control,
independent seed/density evidence and limits remain in method pin
[8b00c2b84](https://github.com/Weichwolf/outshine/blob/8b00c2b84/test/experiments/buildings/urban_parent_probe.md).
Source clipping/provenance/compile-cost details remain in
[4c022df0b](https://github.com/Weichwolf/outshine/blob/4c022df0b/test/experiments/buildings/urban_parent_probe.md).

Next bounded candidate: reuse the existing signed correction D with a separately
denser physical Ready-background baseline, at unchanged expensive parent-query
count. Full readiness requires D=0; known children stay in the opaque baseline.
Compare quality, total background/parent work and failure cases before selecting it.
This is an experiment direction, not a new runtime/transition contract.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
