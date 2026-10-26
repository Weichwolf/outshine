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

## Current step: a rendered primary proxy from resident canonical caster data

The opposite readiness case still mixed incompatible source knowledge: a known
12 m caster with Camera-Pending gives RGB 0.204402 instead of0.649812 for the same
visible prism; a known empty caster cell gives virtual primary masses,
RGB 0.588808 instead of0.243119. These are unchanged-source controls, not native
mesh diagnoses. A representation alternative can consume already resident exact
caster geometry instead of inventing independent primary heights there.

`--caster-primary-proxy` opts into a narrowly typed coarse colour proxy. The caller
must establish the same source/world class, exact canonical prism geometry,
complete restricted caster domain and the existing uniform urban parent material.
Geometry tags or Caster-Ready alone prove none of that. No generic native colour/
material/resource readiness is inferred. The common renderer actually performs
nearest-hit/colour/shadow queries for this proxy in both moment and dense B phases.
Its rendered coverage replaces statistical primary mass; this is not masking an
unrendered source. The background intentionally changes in this explicit new
representation. Refined camera products and explicit known-empty camera domains
always win; unsupported/overlapping partial cases keep the previous path.

`urban_caster_proxy.py` selects a borrowed field descriptor. Its lookup is a view
bounded by the declared domain; columns/spans/offsets retain their original IDs
and buffers. Only small coordinate/shape metadata is new, no field-data copy,
new geometry, source acquisition or per-camera asset/cache. Unproven frame,
footprint, fallback or domain products decline. `has_prism_domain` now names the
shared structural predicate accurately; all internal consumers migrate. Source/
appearance equivalence remains an explicit caller obligation, not that predicate.
The existing conservative prior-eye guard remains, even in known-empty near
cells; the shared exact field trace also rejects interiors of known casters taller
than the prior maximum. No new inside-is-hit or near/interior admission follows.

213 focused tests pass in 25.884s, including eight new tests: independent tall/
empty box RGB, shared/viewed buffers, preserved camera/empty-domain precedence,
all other Ready states, split-filter exact composition, inside guards, rejection,
existing sampled consumer and packet/input invariance. After the runtime freeze,
the inside test additionally checks taller known casters and empty near cells;
all eight proxy tests pass again. No runtime change is made after the freeze.

Frozen runtime `919a4374b`: 36 fresh jobs terminate0 with three lights/six states,
same source16/M8+B16/original247,731 masses and per-light ROIs inside1280x720 at
240km/75,894.663844m. All 36 actual PNG contents were decoded and optically reviewed
with the same dense references and common error maps. Source correspondence is
retained from the same 395-record/snapshot 5bba30af… manifests; neither input changes.

| Caster-Ready / Camera-Pending | Previous RGB RMSE | Proxy RGB RMSE | CPU total s before/after |
| --- | ---: | ---: | ---: |
| day | 0.027651 | 0.015807 | 11.209/8.800 |
| back | 0.022282 | 0.011394 | 8.879/7.447 |
| night | 0.186547 | 0.095858 | 27.595/20.130 |

Errors use the same mean of two dense64 opaque references; both seeds have the
same improvement direction. Their0.001557/0.002117/0.014662 disagreements remain
observed uncertainty, not certified bounds. Foreground has no bare-scene error.
All 18 previous controls and all 15 unaffected proxy pairs are strict dtype/shape/
raw-byte identical. In each light, **every proxy half-caster raw field matches
the preceding half-Ready control exactly**, with only the state's output key
renamed. Both actual source and shaded parent appearance are unchanged.

Caster-only->half-Ready jump RMS falls from0.024888/0.022151/0.161795 to0
for day/back/night. Pending->caster-only also improves:0.022922->0.011182,
0.018883->0.009157,0.170503->0.061493. Other edges remain unchanged, including
remaining half-view->half-Ready and half-Ready->full-Ready jumps. This does not
qualify general seamless transitions or continuity under motion.

The gain is not free work: known geometry is now really queried for primary
visibility. Both phases add proxy queries and retain their costs. No extra pixel
samples/rays/fields are requested; ownership and free-prefix facts stay active.
Primary height tests fall1,806,844->904,204 and model cell visits451,711->427,770
in each light. Added real proxy primary visits are201,719 in M8 and803,829 in B16.
The bounded view needs54% fewer primary column visits than the old full-index
half-Ready control at identical raw images; it still shares the complete resident
caster allocation. All parent/known shadow visits are recorded per phase.
Recorded column visits across these different traversal classes decline for day/
night, but rise3,433,303->3,596,169 for back. They are not interchangeable costs:
fewer height hypotheses do not prove less total work on every backend.

The CPU table includes both phases and shows favorable observed pairs, not
statistical/equal-work/native speedups. Shared interactive Linux/i5 host, fixed
base-then-proxy order, single calls; brief nice10 summaries/sheet assembly and a
tiny guard probe overlap some cases, profiling afterward. Traced1700-ray callback
peaks1,558,685B previous/1,524,659B proxy; these are local temporaries only. Matrix
RSS144,265,216..148,340,736B. Actual field access, proxy/guard rays, source-byte
copies and phase times remain separate in the receipt.

The primary index view is zero-copy, but the complete original resident caster
buffer remains alive; it is not an app-memory reduction or an IO/device saving.
Source parent remains6,264B. Shape/offset descriptors and guard tests are bounded;
field-data buffers are shared and not double-counted. Camera packet/index copies
are unchanged, actual proxy traversal is recorded separately from original camera
queries. Preparation remains separate. Traced memory excludes prepared fields;
RSS includes imports/NumPy/preparation and is not an A18/Iris app budget.

Both the original and new role cases remain optional. Structural/code tests and
this canonical opaque/material family do not qualify arbitrary native shadow
LODs, biased bounds, alpha/transparent surfaces, textures or appearance, partial
role overlaps, interiors, full World/curvature, fast motion/streaming or full
contents at1280x720/60/p99 on either device. Their contracts remain open. No new
native binding/shader/World, global readiness, profile or default changes occur.
Next bounded cost comparison can apply the proven borrowed-domain idea to the
existing Ready subset preparation, whose full lookup masks still repeat work;
source/image identities and saved physical counterexamples remain controls.
SIGGRAPH2015 GPU-driven pipelines PDF page14 separates instance/LOD metadata from
material/render state (optically inspected); it does not license generic caster
geometry as colour. This typed source-reuse alternative is separately derived.
Full commands/input hashes, source correspondence, terminal jobs, raw arrays,
role-wise cost/work/copy/peak and negative evidence with actual optical review:
`/tmp/outshine-research-caster-proxy-919a4374b/receipt.json`.
Earlier source/ownership/free-prefix proofs and all prior limits remain pinned in
[6d09db0dc](https://github.com/Weichwolf/outshine/blob/6d09db0dc/test/experiments/buildings/urban_parent_probe.md).

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
| Actual free camera prefixes | Lower camera-ready errors and next Ready jumps; same-source/form/query obligations and remaining role/model limits | [6d09db0dc](https://github.com/Weichwolf/outshine/blob/6d09db0dc/test/experiments/buildings/urban_parent_probe.md) |
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
Omit either flag for its retained control. Add `--caster-primary-proxy` for the
explicit resident caster-as-primary experiment; its background intentionally
includes the newly queried coarse proxy. Without it B retains its prior meaning.

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

- [GPU-driven rendering pipelines, SIGGRAPH2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf), PDF page14: instance/LOD metadata versus material/render state. Optically inspected; no generic caster-as-colour guarantee is attributed to it.

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
