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

## Current step: actual camera-miss prefixes condition pending shadows

The previous convex-receiver fact fixes a known body's own shadow, but cannot
fix a known empty cell: it has no body. In the one-cell control, Camera-Ready/
Caster-Pending still gives S=0.15 and RGB 0.053468, versus S=1/RGB 0.243119 for the
same fully ready empty scene. The camera ray crossed the canonical footprint to
the ground. Its actual free prefix rules out a blocker there; ground seen through
a lot gap proves no such absence. Non-intersected neighboring cells remain unknown.

The optional `--camera-miss-witness` adds this actual free-prefix fact to the
existing own-receiver variant. The producer guarantees single grounded prisms;
matching pitch/footprint/lattice/basis/vertical frame and explicit complete camera
domains are required. The caller must additionally guarantee the same parent
world class/source and a real nearest-hit query. Geometry metadata alone proves
neither source identity nor Camera-/Caster-/World readiness. This study verifies
all 395 source records, owner/supporting source descriptors and the exact same
snapshot 5bba30af… in both existing manifests, rather than infer correspondence
from frame/XY overlap. Arbitrary/misaligned/fallback products supply no new proof.

The adapter's `column_geometry=single_grounded_prism` describes its construction,
not a persisted asset format or a World Empty certificate. `primary_cap` can
condition matching Camera-Ready footprints on the actual primary prefix; the
receiver limit uses the queried opaque stop even if primary traversal was fully
bypassed. An open opaque endpoint retains its equal-height hypothesis. The existing
conditional categorical algebra, known caster exclusion/external visibility and
own receiver proof remain. No whole camera domain becomes Caster-Ready, no global
fine field is built and no miss history/cache/view asset is introduced. Both
ordinary and sampled/forced-run-fallback consumers use the same proof contract.
Shared zeroed shadow counters and uniform-packet reading retain old defaults.

205 targeted tests pass in 24.930s, including eight new controls: real empty-cell
Ready equivalence; independent 81-world conditional-shadow enumeration with gap,
neighbor and oblique cases; lot-gap non-emptiness; open endpoint; sampled/forced-
run parity; unproved field/frame/coverage rejection; explicit invalid contracts;
packet invariance and immutable arrays. The oracle enumerates tiny test worlds
only; the source-parent compiler and runtime do not bake fine children.

Frozen runtime `b411738c7`: 36 fresh jobs terminate0, three lights/six Ready-
foreground states with both methods. Both retain explicit receiver ownership,
source16/M8+B16, the original247,731-mass field and per-light ROIs in1280x720 at
240km/75,894.663844m. All 36 actual PNG contents were decoded and optically reviewed
against dense references and shared-scale errors. No source/Fine rebuild occurs.

| Camera-Ready / Caster-Pending | Owner RGB RMSE | Free-prefix RGB RMSE |
| --- | ---: | ---: |
| day | 0.017909 | 0.015683 |
| back | 0.013046 | 0.011309 |
| night | 0.132614 | 0.097595 |

Errors use the same dense64 two-seed opaque-field mean; observed reference seed
disagreements remain0.001557/0.002117/0.014662. These are not certified bounds.
Each improvement direction holds against each reference seed separately.
Foreground is a different physical scene and has no bare-reference error.

Half-view->half-Ready jump RMS drops day 0.011198->0.003111,
back 0.008644->0.004209, night 0.074266->0.028307. Pending->half-view drops
0.017579->0.010984,0.010904->0.008460,0.099117->0.054701 respectively.
Other Ready edges remain exactly unchanged. Quality need not improve monotonically
with readiness: day half-view error is now slightly below half-Ready. This is not
an admission of seamless transitions or a globally superior representation.

All 18 owner controls match the preceding freeze bit-for-bit; all 15 non-half-view
candidate pairs preserve every raw field, foreground and full-Ready children.
Half-view B16/T/B moments remain exact. C is also bit-identical in all measured pairs; only receiver transport changes.
Raw RGB stays nonnegative, weights in[0,1], with unchanged moment packing and no
clamping, extra pixel/parent/background rays or sampler density/CDF change.

Observed half-view total integration seconds (owner/free-prefix) are
6.664/6.953 day,5.441/5.219 back,32.716/32.091 night, including both phases.
Times vary strongly with foreign parallel clang-tidy/clang++ load that starts
late in the matrix. Even bit-identical pending/foreground/control cases vary
substantially; these pairs do not establish speedups, isolated costs or native
performance. Own brief nice10 summaries/review-sheet assembly overlap some cases;
callback profiling runs afterward. Fixed order, one pair per light/state.

There are32,503/14,146/28,137 additional ready-footprint/prefix intersections
considered for day/back/night half-view. These are query counts, not distinct
empty cells or proven removed blockers. Primary/height/shadow visit counts remain
unchanged. No new ray fields/requests:99,840 model/399,360 background rays for
day/back,108,800/435,200 for night. The1700-ray callback traced peak is
1,531,118B owner/1,521,456B free-prefix; no general memory reduction is inferred.
Fresh-process RSS spans142,307,328..148,463,616B.

Pixel fields/packet capacity and camera/owner logical copy counts remain exact
between methods. The new proof is bounded scalar metadata plus existing regions
and current ray prefixes; no new per-world-cell table or per-ray history.
Source parent remains6,264B; original resolved control unchanged. RSS includes
imports/prepared arrays, not an app/device budget; traced callback peak excludes
preparation and describes Python/NumPy temporaries, not total allocation/traffic.
Native timing, source-height/occlusion, arbitrary field/source pairing, Earth
curvature, full World, fast motion/streaming and1280x720/60/p99 with all contents
on both devices remain open. No native mesh/World/Shader contract change follows.

Keep both witnesses optional. The next bounded model step addresses remaining
known-Caster/unknown-Camera correlation and unresolved prior/geometry error, with
these proven ownership/free-prefix facts retained. No source/camera/CDF sweep,
new native path or automatic profile/default change is justified by this study.
Primary view/light-correlation context remains Bruneton/Neyret PDF page3/Figure2,
re-read optically; the queried-prefix proof and independent oracle here are separate.
Complete commands/hashes, source correspondence, terminal jobs, raw arrays,
costs/negative cases and actual optical review:
`/tmp/outshine-research-camera-prefix-b411738c7/receipt.json`.
Previous ownership results, copy counterexamples and prior source alternatives:
[c708c20d8](https://github.com/Weichwolf/outshine/blob/c708c20d8/test/experiments/buildings/urban_parent_probe.md).

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
| Proven convex receiver ownership | Own-body prior shadows removed; half-view gains and lower Ready jumps, bounded typed lookup copies | [c708c20d8](https://github.com/Weichwolf/outshine/blob/c708c20d8/test/experiments/buildings/urban_parent_probe.md) |
| Finer geographic source evidence | Eight day/back errors improve, three night errors worsen; Ready jumps remain, direct16 retained | [19594adc0](https://github.com/Weichwolf/outshine/blob/19594adc0/test/experiments/buildings/urban_parent_probe.md) |
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
Add `--receiver-ownership` to the weighted pixel command for the proven-owner
variant; add `--camera-miss-witness` for the verified free-prefix experiment.
Omit either flag for its retained control. The dense background is unchanged.

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
