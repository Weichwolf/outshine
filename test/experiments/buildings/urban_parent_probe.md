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

## Current step: dense Ready background plus signed coarse correction

The existing per-ray identity is `F=B+D`, with known child/caster/foreground
geometry entirely in B. The candidate evaluates `Q16(B)+Q4(D)` or `Q16(B)+Q8(D)`
using the same pointwise correction and source16 parent. Every correction node
still re-queries the complete scene. No separate T/S means, unknown-height
certificate, child fading, changed CDF or new physical shading is introduced.
Equal grids reproduce the complete composition; differing quadrature grids can
oversubtract B even though all per-ray complete RGB values are nonnegative.

`compare_urban_pixel_reference.py --background-samples 16` enables this optional
experiment; without it the complete/reference path is unchanged. Baseline shading
uses `mixed()` with only its unknown-parent contribution disabled: known camera
and caster products, receiver normals/materials and the foreground plane remain.
Both phases and retained arrays count in costs. An explicitly complete Ready state
has D=0 and skips correction queries; asset existence alone does not grant this.
The output stores unclamped composite, background and correction arrays. Negative
D is expected; negative composite RGB is a failed physical approximation. The PNG
camera response is not used to conceal that distinction in measurements.

Freeze `eb575b678`:181 focused tests terminal OK in51.751s under shared-host load.
Four new tests cover all six equal-grid states, parent/child immutability, complete
Ready baseline identity with forbidden correction callback, signed output and
both-phase query counts/crop/packet identity, and invalid density/state requests.
Existing81-world/Ready/source and analytic integration controls remain. Separate
checks verify traced peaks and tracing cleanup on success/exception. No native
or full-project lint is requested.

All54 matrix cases plus one same-grid original-field control terminate0. All55
actual RGB PNG contents were optically inspected in three sheets and the same-grid
image. Direct8 images remain bit-identical to the preceding complete-path freeze;
all15 non-full-Ready D8 parent-work dictionaries match the fresh direct8 controls.
The same-grid night4 control matches its original image within3.56e-15. All full
Ready variants have zero correction rays/D=0 and preserve the old complete16 child
image bit-for-bit. Original247,731 masses/616x624, source hashes and every known
child remain; no original fixture/pin/source is updated.

| Light / state | Direct8 RGB RMSE | D8+B16 | D4+B16 |
| --- | ---: | ---: | ---: |
| day / pending | 0.018849 | 0.018849 | 0.022896 |
| day / half_view | 0.026459 | 0.024970 | 0.027505 |
| day / half_caster | 0.027373 | 0.028247 | 0.030584 |
| day / half_ready | 0.017714 | 0.015807 | 0.018358 |
| day / all_ready | 0.015358 | 0.010716 | 0.010716 |
| back / pending | 0.011499 | 0.011499 | 0.016896 |
| back / half_view | 0.022775 | 0.020158 | 0.023247 |
| back / half_caster | 0.021702 | 0.023380 | 0.025028 |
| back / half_ready | 0.015705 | 0.011394 | 0.014170 |
| back / all_ready | 0.018440 | 0.011819 | 0.011819 |
| night / pending | 0.105889 | 0.105879 | 0.133985 |
| night / half_view | 0.203943 | 0.168162 | 0.224924 |
| night / half_caster | 0.184677 | 0.190294 | 0.153374 |
| night / half_ready | 0.150429 | 0.095884 | 0.116464 |
| night / all_ready | 0.158283 | 0.090680 | 0.090680 |

Errors use complete linear pixel RGB against the same64x64 two-seed opaque-field
mean. Foreground has an extra physical plane and is excluded from bare-scene
errors; its same-model16 comparison, requery controls, work and image remain.
Per-seed errors and same-model16 discrepancies are in the receipt. Reference
seed disagreement0.001557/0.002117/0.014662 (day/back/night) remains observed
uncertainty, not a certified bound. No general convergence/model-error claim follows.

| Night half-view | Negative pixels/channels | Minimum linear RGB |
| --- | ---: | ---: |
| D8+B16 | 2 /6 | -0.002261 |
| D4+B16 | 11 /33 | -0.012014 |

These are genuine negative composite pixels, not negative D or roundoff. They
appear in the raw arrays; the optically inspected red defect maps identify them.
The unrestricted candidate is therefore rejected as a general physically valid
parent replacement. No clamp, new threshold, default/profile change or declared
quality reduction repairs it. Retain it as an explicit diagnostic/negative control.

| Light / state | Direct8 total s | D8+B16 s | D4+B16 s |
| --- | ---: | ---: | ---: |
| day / half_caster | 9.621 | 10.744 | 4.020 |
| day / half_ready | 6.445 | 9.510 | 4.749 |
| day / all_ready | 1.120 | 3.351 | 3.381 |
| back / half_caster | 7.189 | 8.349 | 3.346 |
| back / half_ready | 4.685 | 7.389 | 4.292 |
| back / all_ready | 1.154 | 3.268 | 3.216 |
| night / half_caster | 25.616 | 26.626 | 9.274 |
| night / half_ready | 14.603 | 24.018 | 11.368 |
| night / all_ready | 2.353 | 6.478 | 5.925 |

D4 lowers correction-query count to one quarter of direct8; D8 has identical parent
work. For day/back, direct8 uses99,840 rays/state; D8+B16 uses499,200 and D4+B16
424,320. Night counts are108,800 /544,000 /462,400. Full Ready uses only the
399,360/435,200 background rays. Camera packet copies and owner writes are48/8B
per actual ray; sampler buffers, parent/child visits/tests/shadow work and peaks
are also recorded. Inputs/preparation are separate from integration. Independent
baseline preparation copies Ready lookup data twice; its time/RSS are reported,
not a persistent cache or hidden free work. Fresh-process RSS spans
143,937,536..148,553,728B including imports/prepared fields/native+NumPy allocations,
not isolated temporary memory, total allocator traffic or an app/device budget.

There are three measured single-pair cost/error improvements (D4): back half-Ready,
night caster-only and night half-Ready. The latter changes error0.150429->0.116464
and total integration14.603->11.368s. These are retained favorable cases, not proof
of a universal/native speedup or equal quality elsewhere. Shared interactive host,
foreign CPU builds, fixed method order and no own profiling/image processing during
normal timings; brief numeric summaries ran nice10. Expensive tests finish before
measurements. More background rays often increase total cost despite fewer parent
queries; all-Ready improvement is simply denser physical child sampling.

| Light, half-view->half-Ready | Direct8 jump RMSE | D8+B16 | D4+B16 |
| --- | ---: | ---: | ---: |
| day | 0.021826 | 0.021727 | 0.022858 |
| back | 0.018811 | 0.018461 | 0.020188 |
| night | 0.127283 | 0.131750 | 0.187110 |

Ready jumps remain, including worse night differences. This is neither a temporal
transition method nor the missing qualified parent representation. Preserve the
complete default and source16. Stop promoting raw difference transfer; the next
bounded candidate is background-weighted joint radiance/transport moments, keeping
T*S joint, same-grid/full-Ready identities and nonnegative composite by construction.
Its ratio approximation/bias, fallback and total costs require their own proof and
images; it is not implemented or admitted here. No new sampler-density/CDF sweep.

Projected ROIs inside1280x720 at flat ENU240km/75,894.663844m do not prove complete
World frames, curvature, native/Metal/Iris/p99 or source-height/occlusion correctness.
Movement design remains #5/#12: walking through many hundreds of m/s, acceleration,
turns and height changes with complete content and1280x720/60 including p99. These
static probes do not qualify that contract. Source parents remain camera-independent
assets; pixel moments/Ready background are current-view/light results, not assets
reused after arbitrary movement. No whole-world rebuild or enlarged validity radius
is justified by these images.

All previous methods/cost negatives remain pinned. Complete commands, raw inputs,
terminal logs, unclamped arrays, phase counters/costs, validation and optical review:
`/tmp/outshine-research-control-pixels-eb575b678/receipt.json`.

The preceding joint deterministic composition and remaining alias/Ready evidence
remain in [d6f037c33](https://github.com/Weichwolf/outshine/blob/d6f037c33/test/experiments/buildings/urban_parent_probe.md).
Bounded opaque-reference controls remain in [8b00c2b84](https://github.com/Weichwolf/outshine/blob/8b00c2b84/test/experiments/buildings/urban_parent_probe.md);
source clipping/provenance in [4c022df0b](https://github.com/Weichwolf/outshine/blob/4c022df0b/test/experiments/buildings/urban_parent_probe.md).

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
