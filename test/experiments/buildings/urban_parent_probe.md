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

## Current step: bounded dense pixel references

The source-resolution comparison changed sign between fixed4x4 and the earlier
jittered16x16 reference at night/backlight. The earlier night16/32 discrepancy was
0.053445. That uncertainty must be measured before choosing finer source products.
Original pins, geometry, lighting and the default direct16 source compiler remain.

`render/pixel_reference.py` integrates complete linear RGB and coverage from camera
and scene callbacks. It uses existing camera rays and opaque GGX/shadow evaluation,
without averaging C/T/S separately or clamping signed RGB. Fixed strata use pixel
centres within each subcell; optional jitter uses NumPy SeedSequence namespace4,
absolute pixel(x,y) and the requested seed. Crop/packet changes preserve addresses.
The two replicate seeds are independent; equal seed labels at different densities
are not asserted independent. Pixel-box integration is an offline control, not a
new reconstruction filter or exact geometric oracle.

Only one pixel's S² camera rays and one bounded packet are live. S is1..128 per
axis and packet capacity1..8192; the CLI default is2048. The measured runs use8192.
Reported logical storage is32P output bytes,56B packet bytes and48S² camera-output
bytes; normalization/validation and scene temporaries are additional. A broadcast
camera origin's logical size is not its allocated backing size. Packet copies are
48 bytes/ray plus8 owner-index bytes/ray; these are explicit writes, not complete
allocator traffic or physical memory-bandwidth measurements. Scene counters retain
visits/tests and per-packet loop steps, whose sum depends on packet partitioning.
The integrator itself requires only NumPy; this scene driver retains its existing
render/import dependencies and loads the prepared original field outside queries.

Freeze `893160bf5`:174 focused tests pass in26.649s, including eight new analytic
area/moment, signed-RGB, fixed-camera, crop/packet, bounded-query, empty/invalid and
import-isolation controls. Full original-field fixed4 matches the previous stored
RGB to2.67e-15. Fixed16 packet/whole-image RGB differs at most2.49e-14 and coverage
is identical; trace instrumentation preserves both outputs.

| Fixed16 night, same rays | Whole-image arrays | Packet8192 |
| --- | ---: | ---: |
| Uninstrumented integration s | 4.793 | 5.272 |
| Separately traced temporary peak B | 190,150,816 | 5,502,867 |
| Uninstrumented fresh-process peak RSS B | 372,822,016 | 132,636,672 |

Traced peaks cover integration including scene temporaries, excluding imports and
prepared input. Fresh-process RSS includes both; it is not an app/device budget.
Uninstrumented times and separately instrumented peaks must not be conflated.
One paired timing is descriptive, under shared-host load, not a speed guarantee.
The packet path pays query/accumulation overhead; its benefit here is bounded
memory for denser references, not fewer rays or a native/frame speedup.

| Light | Seed RMSE16 | Seed RMSE32 | Seed RMSE64 | Old fixed4 vs64 seed mean |
| --- | ---: | ---: | ---: | ---: |
| day | 0.009585 | 0.004007 | 0.001557 | 0.028208 |
| back | 0.011822 | 0.005694 | 0.002117 | 0.038833 |
| night | 0.087215 | 0.036327 | 0.014662 | 0.263506 |

At64, day/back query6,389,760 rays and night6,963,200. Camera-to-packet writes
are306,708,480/334,233,600B plus51,118,080/55,705,600B owner indices. Query times
are47.874..80.884s and fresh-process RSS133,677,056..136,007,680B. This includes
per-pixel camera requests and complete shading; preparation and serialization are
outside. Two output arrays and three packet buffers are allocated per integration;
one normalized direction array is allocated per pixel. Camera/scene internals and
total allocator counts are not captured by these structural counts.

Each density/light has two independent seeds1113/1729 over the same complete ROI.
Seed difference is observed disagreement, not a confidence interval or certified
error bound; agreement of two seeds cannot exclude bias or rare bright features.
There are18 terminal successful density runs and36 optically inspected RGB/coverage
PNGs in three local sheets. Original247,731 masses in616x624 are preserved. These
are projected ROIs inside1280x720 at240km/75,894.663844m, flat ENU, not curved-Earth
World/native/Metal/Iris/p99 evidence. PNGs contain only the measured ROI.

| Light, dense64 resolved seed mean | Model regions16 | Regions32 | Regions64 |
| --- | ---: | ---: | ---: |
| day | 0.022896 | 0.021514 | 0.020331 |
| back | 0.016896 | 0.016840 | 0.016961 |
| night | 0.133905 | 0.136133 | 0.139265 |

The table re-evaluates existing complete categorical expectations at fixed4x4
model rays against the mean of the two new64x64 resolved references. The model is
not densely integrated here: remaining model spatial aliasing and height/recipe,
correlation and readiness errors remain. No general source-resolution promotion,
new learned model, material/night method, pin update or native port follows.

Previous source-area delivery `c431da988` / method pin
[4c022df0b](https://github.com/Weichwolf/outshine/blob/4c022df0b/test/experiments/buildings/urban_parent_probe.md)
retains all detailed attribution, costs and negatives: recursive GEOS clipping
preserves source/provenance/recipe and64er images, with3.63x faster64er compilation;
at16er it is slower. Direct16 remains default. Original inputs and raw evidence
remain protected; source-only reproducibility and dense-reference uncertainty are
separate concerns. Complete commands, hashes, timings, counters, comparisons and
optical review: `/tmp/outshine-research-pixel-reference-893160bf5/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
