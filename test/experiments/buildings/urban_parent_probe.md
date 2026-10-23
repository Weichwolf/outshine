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

## Current step: nonnegative joint Ready transport moments

Raw `Qhigh(B)+Qlow(D)` produced negative composite RGB even though each pointwise
physical scene was nonnegative. Instead retain ten current-view fields per parent
query: C(3), R(3), B(3), T(1), where `B=Bdirect+Bother` and
`R=T*(S*Bdirect+Bother)`. T*S and shaded-background correlation remain joint.
For finite nonnegative radiance and T/S in[0,1], `0<=R<=B`. The filtered ratio is
`a=mean(R)/mean(B)` per colour channel; if sampled B is zero, use mean(T).
The candidate is `mean(C)+a*Bhigh`. No RGB/weight/negative-D clamp is used.

Nonnegative pointwise fields and positive averaging weights imply0<=a<=1 and
nonnegative composite. Equal grids recover `mean(C+R)` to floating precision.
Explicit full readiness has C=0/R=B/T=1, so a=1 even for black sampled B; the
implementation skips moment queries and preserves the dense child baseline exactly.
This is not an Empty/Ready certificate derived from data existence or source bounds.
When sampled B is black, mean(T) is a bounded fallback with no quality guarantee.
The current ambient-positive original cases do not exercise that fallback.

`urban_transport_moments.py` owns the pointwise packing/filtered composition.
`urban_ready_pixels.py` owns Ready scene queries and measured integration; the
CLI remains in `compare_urban_pixel_reference.py`, including existing helper import
routes. `integrate_fields` extends the bounded NumPy-only pixel kernel to1..32
channels; `integrate_pixels` remains its identical3-channel interface. Source
parents stay camera-independent; these light/Ready/view fields are transient,
not persisted camera variants or a source-asset format. No new dependency/native
binding/cache/provider/geometry/shading/height recipe or world rebuild is added.

Use `--background-samples 16 --weighted-transport` explicitly. Without the weighted
flag the preceding raw-D experiment remains a reproducible negative control; without
background samples the complete/default path stays. The direct16 source compiler
and runtime profiles are unchanged. All known children, caster roles and foreground
geometry remain; only the filtered composition representation changes.

Freeze `711849d6b`:188 focused tests terminal OK in27.343s. Seven new analytic/
scene controls check joint anti-correlation, positivity where raw D fails, all six
equal-grid Ready states and immutable inputs, black fallback/full-Ready identity
without model queries, ten-field means/crop/packet identity, invalid domains/bounds,
and an explicit bias counterexample. For B(x)=1+x,T=x,S=1,C=0, one midpoint with
exact dense mean B predicts3/4, while the true integral is5/6. Positivity does not
certify accuracy, unbiasedness or convergence. An additional targeted analytic
extension shows normalization can disturb an already exact coarse radiance mean:
R(x)=x,B(x)=1+x²,T=x/(1+x²) yields8/15 instead of1/2 when midpoint B is replaced
by its exact dense mean4/3. That extended test passes separately; runtime code and
measured matrices are unchanged. Existing81-world/source/Ready and
signed-difference controls remain. Separate checks validate whole-phase tracing
and cleanup after success/exception. No native/full-project lint is requested.

All54 matrix runs and one same-grid original-field control terminate0. All55 actual
PNG contents were optically inspected in three local comparison sheets and the
same-grid image. All composites are nonnegative and all effective weights in[0,1].
Fresh direct8 images remain bit-identical to the preceding freeze. All15 non-full-
Ready M8 parent-work dictionaries match direct8 exactly; full Ready needs zero
moment rays and preserves the old complete16 child image bit-for-bit. Original
same-grid night4 agrees within3.56e-15. Source hashes and247,731 masses/616x624 stay.

| Light / state | Direct8 RGB RMSE | M8+B16 | M4+B16 |
| --- | ---: | ---: | ---: |
| day / pending | 0.018849 | 0.018849 | 0.022896 |
| day / half_view | 0.026459 | 0.025077 | 0.027775 |
| day / half_caster | 0.027373 | 0.027651 | 0.030012 |
| day / half_ready | 0.017714 | 0.015807 | 0.018358 |
| day / all_ready | 0.015358 | 0.010716 | 0.010716 |
| back / pending | 0.011499 | 0.011499 | 0.016896 |
| back / half_view | 0.022775 | 0.020120 | 0.023095 |
| back / half_caster | 0.021702 | 0.022282 | 0.024591 |
| back / half_ready | 0.015705 | 0.011394 | 0.014170 |
| back / all_ready | 0.018440 | 0.011819 | 0.011819 |
| night / pending | 0.105889 | 0.105892 | 0.133966 |
| night / half_view | 0.203943 | 0.168109 | 0.209098 |
| night / half_caster | 0.184677 | 0.186547 | 0.145292 |
| night / half_ready | 0.150429 | 0.095858 | 0.116277 |
| night / all_ready | 0.158283 | 0.090680 | 0.090680 |

Errors are complete pixel linear-RGB RMSE against the same64x64 two-seed opaque-field
mean. Foreground adds a real plane and is excluded from bare-scene errors; its
same-model16 discrepancy, image and full costs remain. Reference seed differences
0.001557/0.002117/0.014662(day/back/night) remain observed uncertainty, not certified
bounds. Raw D8/D4 night half-view had2/11 negative pixels; M8/M4 have zero without
clamping, with raw minimum RGB0.004543/0.005060. Per-seed, same-model16, raw-D and
remaining negative quality/cost cases are retained in the receipt.

| Light / state | Direct8 total s | M8+B16 s | M4+B16 s |
| --- | ---: | ---: | ---: |
| day / half_caster | 10.631 | 11.719 | 4.149 |
| day / half_ready | 6.704 | 9.723 | 6.633 |
| day / all_ready | 1.693 | 5.061 | 3.796 |
| back / half_caster | 8.284 | 9.651 | 3.466 |
| back / half_ready | 5.167 | 8.317 | 4.627 |
| back / all_ready | 1.276 | 3.737 | 3.670 |
| night / half_caster | 27.043 | 31.638 | 11.215 |
| night / half_ready | 18.088 | 21.471 | 10.001 |
| night / all_ready | 1.723 | 5.068 | 5.278 |

There are three favorable observed M4 single pairs: back half-Ready, night caster-
only and night half-Ready. The last changes error0.150429->0.116277 and total
integration18.088->10.001s. No universal/equal-quality/native speedup follows:
pending/caster images can have larger errors, M8 often costs more, and full-Ready
improvement is denser child sampling. Shared interactive host with foreign CPU
builds, fixed method order and one call per case; own tests finish first, no own
profiling/image processing overlaps normal timing. Brief summaries run nice10.

M8 uses the same expensive parent rays/work as direct8; M4 one quarter of requested
parent rays. Ten fields add logical query-output bytes80/ray versus24 for RGB;
this is not physical bandwidth or total allocator traffic. For day/back, direct8
uses99,840 total rays, M8+B16 uses499,200 and M4+B16 uses424,320; night totals are
108,800/544,000/462,400. Full Ready uses only399,360/435,200 background rays. Camera
packet copies/index writes are48/8B per actual ray. Moment output storage is88B
per pixel including the unused kernel coverage channel, versus32B for RGB/coverage.
Returned10-field packet peak, source/child visits/tests, shadow work, phase/total
costs and scene-value bytes are recorded. Preparation is separate and includes
Ready lookup copies. Fresh-process RSS spans143,581,184..148,168,704B including
imports/prepared fields/native+NumPy allocations, not isolated temporary memory or
an app/device budget. Actual CPU/GPU/IO/native/device acceptance remains separate.

| Light, half-view->half-Ready | Direct8 jump RMSE | M8+B16 | M4+B16 |
| --- | ---: | ---: | ---: |
| day | 0.021826 | 0.021994 | 0.023192 |
| back | 0.018811 | 0.018881 | 0.020451 |
| night | 0.127283 | 0.135923 | 0.172228 |

Ready jumps persist and night remains difficult. The bounded candidate fixes the
negative-composite defect and offers useful measured tradeoffs, not a qualified
parent/temporal representation. Keep it optional; no source/profile/default change.
Next compare existing finer geographic source evidence at fixed M8/B16 and unchanged
resolved inputs, to target the remaining parent model rather than increase camera
sample density or tune CDF noise. Array/build/lookup costs and negative cases decide;
no automatic source-resolution promotion or new native contract follows.

Full contents/possible240km visibility and1280x720/60/p99 still apply from walking
to many hundreds of m/s, acceleration/turns/height changes (#5/#12). Static flat-ENU
ROIs at240km/75,894.663844m do not prove that movement, full World/curvature, device,
source-height/occlusion or frame contract. No full-world reset, enlarged capture
validity or view-moment asset cache is justified. Functional transport remains #11.
Complete commands/input hashes, terminal logs, unclamped arrays/weights/moments,
work/cost/negative tables, validation and actual visual review:
`/tmp/outshine-research-weighted-moments-711849d6b/receipt.json`.

Raw difference controls, all favorable/negative cases and motion clarification stay
in [e198a046d](https://github.com/Weichwolf/outshine/blob/e198a046d/test/experiments/buildings/urban_parent_probe.md).
Earlier joint/reference/source evidence remains pinned below.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
