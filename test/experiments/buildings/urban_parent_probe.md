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

## Current step: absolute screen addresses across cropped queries

The existing pixel-coupled path used a crop-relative row stream. Changing only
its ROI changed CDF uniforms for otherwise identical rays, causing artificial
image changes. The new optional `pixel_origin=(x,y)` on `pixel_surface_packets`
anchors uniforms to uint32 screen coordinates. The query's width, height and ray
packet boundaries no longer choose a pixel's stream. Unchanged independent and
crop-relative controls remain available; the default has not changed.

For spatial axis count s and M draws, let D=M*s² and B=ceil(2D/4). Existing NumPy
Philox is seeded in namespace3 and addressed by counter `(x*B,y,s,M)`. Each pixel
uses exactly B four-uint64 blocks, including padding. Its first D words supply
random-key permutations within each draw's spatial rays; its next D words supply
independent53-bit jitter. The same original P/M strata and real-arithmetic target
remain. This replaces variable-consumption shuffles, without generating skipped
pixels or a full-image table. Equal uint64 keys use stable index order; finite
PRNG/tie/rounding bias is not certified. Earlier variance gains from a different
random sequence do not transfer automatically to this stream.

Storage remains `O(width*P*M + batch*M)` with one row band and a reusable packet;
Philox raw words, sorting and jitter copies add cost. No child/geometry, shading,
T/S or independent camera/caster readiness rule changes. A completely ready parent
still requests no samples. Caller contracts are absolute pixel origin, raster
subray order, seed, s and M; changing seed is an explicit stream change. These
addresses do not reproject geometry, accumulate history or reconstruct temporal
transitions, and they do not define a persistent random world.

`compare_urban_layers.py --pixel-strata --screen-strata` selects this mode.
Omit `--screen-strata` for the earlier crop-relative pixel control; omit both for
the independent default. Shared `layer_states` retains the previous six ready/
opaque cases and child vacancies. `compare_urban_sample_windows.py` tests six
changing windows containing the complete union of all required support, using
identical overlapping rays selected from a canonical ray grid. It also runs six
camera poses with independent camera-ready, caster-ready, foreground and all-ready
states; camera motion parameters are -4096,-2048,0,2048,4096,6144m on the existing
240km-distance orbit. Window changes never remove required support.

Measured freeze `23d655c63`:162 focused tests pass in23.001s. Four added tests cover
absolute-window/tile identity, packet changes, odd sample/block padding, uint32
bounds, retained strata and actual parent C/T/S identity for identical cropped
rays. All previous81-world, default, conditional shadow, finite-stop, opaque/ready
and source controls remain. Four full original-field layer runs and four six-frame
sequence runs terminate0. All retain the original247,731 masses in616×624; only
projected ROIs in1280×720 are traced. Source inputs were not refreshed.

The old crop-relative night's arrays are bit-identical to the preceding delivery;
all three fresh complete-expectation controls match their previous arrays. All-
ready images remain bit-identical to the supplied child baseline.124 diagnostic
PNGs are represented in seven optically inspected original/sequence sheets,
including byte-identical legacy contents. Known children and foreground survive;
noise, coarse-model mismatch and hard model/ready transitions remain visible.

For each frame, e is pixel-averaged sampled RGB minus that frame's complete
categorical expectation. Error-increment RMSE measures `e[t]-e[t-1]` at fixed screen
pixels; it does not reproject history or count the target's actual image change.
Medians below are query/composition seconds across six frames; preparation and
image output are outside, sample generation inside. One fixed seed1113,4×4 spatial
rays and M4 are used throughout; all24 frame pairs keep identical surface counts
and prefix/receiver-shadow counters. Methods run in fixed order on a shared
Linux/i5-1135G7 CPU; no own tests/instrumentation overlap normal runs. These are
not isolated/statistical latency or equal-error performance claims.

| Sequence | Crop increment RMSE | Screen increment RMSE | Crop median s | Screen median s |
| --- | ---: | ---: | ---: | ---: |
| static-night | 0.095024 | 0.000000 | 2.463 | 2.426 |
| moving-night | 0.077599 | 0.054741 | 1.945 | 1.941 |
| moving-day | 0.003435 | 0.003369 | 0.968 | 0.941 |
| moving-back | 0.010144 | 0.009986 | 0.853 | 0.897 |

The static-night increment is exactly zero with screen addresses; prior crop
addressing gives0.095024. This qualifies window invariance in that controlled
case, not general temporal stability. Moving ready cases still change strongly;
night improves, day/backlight only slightly. The three ordinary original-field
comparisons retain negative spatial-quality cases:

| Lighting | State | Old crop pixel RMSE | Screen pixel RMSE |
| --- | --- | ---: | ---: |
| day | pending | 0.002787 | 0.002940 |
| day | half_view | 0.001968 | 0.002134 |
| day | half_caster | 0.003710 | 0.003841 |
| day | half_ready | 0.001968 | 0.002134 |
| day | all_ready | 0.000000 | 0.000000 |
| day | foreground | 0.001426 | 0.001547 |
| back | pending | 0.009651 | 0.010017 |
| back | half_view | 0.006551 | 0.006911 |
| back | half_caster | 0.009527 | 0.009800 |
| back | half_ready | 0.006551 | 0.006911 |
| back | all_ready | 0.000000 | 0.000000 |
| back | foreground | 0.004625 | 0.004780 |
| night | pending | 0.088481 | 0.055823 |
| night | half_view | 0.068729 | 0.034159 |
| night | half_caster | 0.096921 | 0.052914 |
| night | half_ready | 0.068729 | 0.034159 |
| night | all_ready | 0.000000 | 0.000000 |
| night | foreground | 0.001759 | 0.001909 |

Each is against the same complete categorical expectation at the same spatial
rays. One seed is not a variance/convergence result: day and backlight errors
increase here, night foreground also worsens. No general spatial-quality or
speed recommendation follows from the eliminated window artifact.

Separate warm sampler-only tracemalloc, consuming packets without retaining a full
image, width1280/spatial4/batch2048:

| Draws | Height | Crop peak B | Screen peak B |
| --- | ---: | ---: | ---: |
| 4 | 1 | 1,445,528 | 2,689,568 |
| 4 | 720 | 1,445,792 | 2,699,168 |
| 16 | 1 | 5,574,368 | 10,750,496 |
| 16 | 720 | 5,574,656 | 10,760,096 |

This is bounded extra sampler storage, not a smaller footprint than crop sampling.
For M4/P16 the addressed path produces128 uint64 words per emitted pixel; for other
counts padding rounds2D up to a four-word block. It adds key ordering and jitter/
layout copies. Original mixed-query counters, normal times and exact commands
remain in reports; instrumented times are not normal timing evidence. These peaks
exclude renderer/app/GPU/device budgets and establish no A18 memory admission.

Decision: retain `--screen-strata` opt-in to avoid crop-induced resampling. Keep the
old streams as quality/cost controls. No native implementation, extra dependency,
learned model, cache/export framework or new material/night technique is added.
Flat ENU240km controls are not curved-Earth/native/frame/p99 evidence. Original
height/source/model errors, spatial aliasing and continuous physical/ready temporal
transitions remain open under #1113/#1102; native residual work stays #1127.
Full input hashes, commands, terminal results, work/error/memory, optical checks
and duplicate redirects: `/tmp/outshine-research-screen-samples-23d655c63/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
