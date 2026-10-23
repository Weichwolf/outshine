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

## Current step: proven receiver ownership during caster pending

The remaining Camera-/Caster-Ready jump contains a concrete missing geometric
fact. A known 6 m roof in a 16 m cell, with heights 6/12 m at probabilities 0.7/0.3 and
vacancy 0.15, gets S=0.745 while its caster product is pending: the model can shadow
it with its own taller prior. The independent one-box ray reference gives S=1.
Caster readiness changes S to 1. No source or physical geometry changed.

The optional `--receiver-ownership` candidate carries actual receiver ownership
into deterministic and sampled shadow consumers. It excludes only the known
convex receiver body from its own shadow hypotheses, using the existing own-cell
argument. For an outward face with positive normal/light dot product, the light
ray leaves the supporting halfspace and cannot re-enter that convex body. Neighbor
cells, external casters and primary parent contributions keep their paths. This
is a geometric fact from the actual hit, not a Caster-Ready/Empty certificate for
an entire camera domain. Generic opaque stops do not grant this fact.

`urban_opaque_receivers.py` owns opaque hits/foreground override and the conservative
producer: actual entry-face hit, matching pitch/footprint/XY lattice, equal vertical
origin, identity basis and exactly one bottom-zero positive-height prism in the
owning column. Fallback hits, multiple/raised spans, misaligned products, ground,
unknown and winning foreground hits yield no ownership. The common shadow inputs
validate integer cells, bounds, finite stops/normals and explicit Camera-Ready
coverage. An explicitly supplied owner is a caller proof of that geometry.
No inference from source-area means, arbitrary XY overlap or data existence occurs.

Before the frozen matrix, 195 targeted Python tests pass, including seven new
independent-box/proof/contract tests: false own-roof shadow removed, real neighbor
shadow retained, complete one-box Ready composition recovered, actual trace and
foreground clearing, unproven layouts/fallbacks, invalid input rejection, sampled
consumer parity, packet invariance and immutable child arrays. Earlier81-world/
source/Ready controls remain. A later guard rejects vertical-origin/basis mismatches. The final typed-query
lookup fix passes all 197 focused tests (25.046s), including a ninth test that
keeps preparation peak bounded when unqueried known columns grow128->32768.
No native/GPU contract, source/Fine build, dependency, default/profile change or
new camera sampling/normalization method is bundled.

At runtime freeze `1bb200c95`,36 fresh matrix jobs terminate0: source16,
M8/B16, three lights and six Ready/foreground states, same247,731-mass reference,
per-profile ROIs inside1280x720 at240km/75,894.663844m. All18 unflagged raw arrays
are bit-identical to the preceding freeze. All36 actual PNG contents were decoded
and optically reviewed with dense references/common-scale error maps. The source,
geometry, sampler, transport normalization and known caster/background inputs stay.
After vertical guarding (`35c620f20`) and the lookup copy fix (`ee442f9eb`), each
three original-field half-view runs terminates0 with every array/decoded PNG exact
to the matrix. The six additional exports do not constitute independent quality samples.
Only the conservative guard and dtype of selected lookup keys change afterward.

| Camera-Ready / Caster-Pending | Baseline RGB RMSE | Owner RGB RMSE |
| --- | ---: | ---: |
| day | 0.025077 | 0.017909 |
| back | 0.020120 | 0.013046 |
| night | 0.168109 | 0.132614 |

Errors use the same two dense64 opaque-field reference seeds; their disagreement
remains day/back/night 0.001557/0.002117/0.014662, observed uncertainty rather than
certified bounds. Each seed has the same improvement direction. Foreground is a
different physical scene and is excluded from bare-reference errors.
Half-view->half-Ready jump RMS drops day 0.021994->0.011198,
back 0.018881->0.008644, night 0.135923->0.074266. Pending->half-view drops
0.026433->0.017579,0.018842->0.010904,0.159240->0.099117 respectively.
Other Ready edges remain exactly unchanged; transitions are not eliminated.

Only half-view RGB/transport weights change. Its parent C/T and B moments are
bit-identical; B16 is exact everywhere. All other five states retain every raw
field bit-for-bit, including known foreground and full-Ready children. No extra
parent/background rays or pixel fields are requested. The old control remains
reproducible without the explicit flag. Default/source16 stays unchanged.

At the original paired matrix freeze, half-view total CPU integration seconds
(base/owner) are day7.176/7.209, back6.164/6.708, night13.768/13.905; no speedup
is claimed. This includes both moment/background phases, before the metadata-copy
fix. Each light has21,128 proven owner hits/column searches and676,096 selected
metadata logical bytes; max owner input array131,072B (16B/requested ray).
The current int32 trace-index gather plus uint32 key conversion copies8B/selected
hit,169,024B cumulatively here, excluding other temporaries/search probes.

A separate512-key lookup counterexample exposes NumPy promotion of the whole
247,732-element uint32 offset table when given int32 keys: traced peak1,990,560B.
Converting only selected nonnegative validated keys to the table dtype gives the
same owners with8,608B peak. The final callback trace (same1,700 night rays) changes
peak from1,502,891B baseline/2,128,184B pre-fix owner to1,502,891B/1,521,300B after
the fix. This is prepared-input-excluded traced memory, not total allocation or
normal runtime. Fresh-process matrix/postguard RSS144,023,552..148,193,280B.

The source parent remains6,264B and the247,731-mass offline control is unchanged.
Ownership consumes existing ready geometry only; it does not build unknown fine
cells. Metadata search count, selected metadata logical bytes, owner-array peak,
phase/total times and whole-process RSS are recorded. Selected metadata bytes
exclude search probes, allocator traffic and physical bandwidth. Owning cells are
passed by view across batches; the same owner array must not be counted twice.
Separate traced callback checks cover temporary peaks; RSS includes imports and
prepared fields, not an app budget. Shared interactive Linux/i5 CPU, fixed base-
then-owner order, one pair per case, brief nice10 summaries/review-sheet work and
foreign CPU jobs. Python times are not native/device or equal-quality speedups.

This fixes a demonstrated own-body inconsistency and improves these images/one
Ready edge; it does not eliminate remaining prior/known-geometry correlations,
source-height uncertainty, aliasing or physical/Ready transitions. No generic
convex-owner evidence exists yet for arbitrary native building/fallback meshes.
Full World, curved terrain, streaming/motion and1280x720/60/p99 with full contents
on both devices remain unproved. The next model step must retain this fact and
address remaining same-source Camera-/Caster-Ready correlation; no density/CDF
sweep or World/renderer rewrite follows.

The prior SIGGRAPH/EG forest joint visible/lit model is context, re-read optically
on PDF pages2-3/Figure2; the convex supporting-halfspace argument here is separate.
Full commands/hashes, terminal jobs, unclamped arrays, costs/counterexamples and
actual optical review: `/tmp/outshine-research-receiver-owner-1bb200c95/receipt.json`.
The preceding geographic alternatives, all negatives and references remain pinned
in [19594adc0](https://github.com/Weichwolf/outshine/blob/19594adc0/test/experiments/buildings/urban_parent_probe.md).

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
variant; omit it for the retained baseline. The dense background is unchanged.

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
