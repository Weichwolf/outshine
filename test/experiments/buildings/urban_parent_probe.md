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

## Current step: cheaper source areas and a bounded resolution comparison

Fixed-camera attribution separates the complete categorical model from additional
surface sampling: day pending model RMSE0.025980 versus sampling0.002940; the
half-view->half-ready expected-image jump is0.024339 with exactly zero sampling
change. Squared model/sampling errors include a cross term, so they must not be
reported as independent percentages. More sample coupling cannot remove that
model jump. Attribution uses the original4×4 rays and does not certify pixel
integration or original-height evidence.

The existing source compiler was compared at16²/32²/64² regions, using exactly the
same395 pinned records, setbacks, heights and vacancy recipe before any child
build. Global area means and provenance remain unchanged to floating precision.
Finer regions reduce smoothing of geographic evidence, with larger parent arrays.
No statistics were fitted to the resolved render or to a place.

Direct GEOS clipping repeats the whole offset source geometry for every region.
`urban_parent_areas.py` adds explicit `hierarchical` clipping: for a child rectangle
C inside parent P, `(G∩P)∩C = G∩C`. Split integer cell ranges recursively; intersect
children with already clipped local geometry. Empty subtrees fill zero; fully
covered subtrees use exact rectangle areas, retaining holes and disconnected parts.
Odd/non-square ranges are supported. Repeated double-precision intersections have
small numerical differences; no formal floating/topological error bound is claimed.
Source-area zero/full values are never World empty/ready certificates.

`compile_parent(..., area_method="hierarchical")` selects this alternative; the
existing direct16 default stays unchanged. `area_cells` counts requested outputs;
`area_intersections` now records actual geometric pairs, with covers/area queries,
nodes, rectangles, stack and input-coordinate multiplicities in `area_work`.
Coordinate multiplicity counts inputs supplied to GEOS, not internal vertex tests,
allocator traffic or copied/read bytes. The source-only CLI serializes the same
four parent arrays; consumer/import paths still require no source/GEOS/fine build.

Measured freeze `c431da988`:166 focused tests pass in26.670s. Four new tests compare
against direct GEOS intersections and analytic areas with holes, disconnected
parts, boundary contacts, odd rectangles, covered/empty pruning, large projected
coordinates, compiler provenance/recipe and invalid-method input. Existing source,
81-world, transport, ready/opaque and sampling controls remain. Five fresh-process
builds and nine full-original-field layer runs terminate0; no own tests/profiling
or image processing overlap their normal timing. Shared Linux/i5-1135G7 CPU,
one call per configuration: these are measured cases, not statistical or native
hardware speed guarantees. Input parsing and serialization are outside compile
seconds; domain preparation, offsets and statistics are inside.

| Source regions | Area method | Compile s | Parent array B | Process peak RSS B |
| --- | --- | ---: | ---: | ---: |
| 16×16 | direct | 7.206 | 6,264 | 142,848,000 |
| 16×16 | hierarchical | 10.684 | 6,264 | 143,347,712 |
| 32×32 | direct | 15.828 | 24,696 | 143,024,128 |
| 64×64 | direct | 50.492 | 98,424 | 143,634,432 |
| 64×64 | hierarchical | 13.892 | 98,424 | 143,380,480 |

At64², hierarchical compilation is3.63× faster than the same-quality direct
control. At16² it is slower, so no unconditional/default replacement is made.
Process peaks include imports, inputs, GEOS/native allocations and outputs; they
are not isolated clipping peaks, total allocations or app/device budgets. Peaks
remain around143MB; no memory reduction is established. Parent payload at64² is
98,424B versus6,264B at16²; metadata/provenance is additional.

At64², geometric intersections increase12,288->16,271 plus4,095 covers tests;
input-coordinate multiplicity falls1,392,918,528->7,411,288. This explains neither
an equivalent hardware traffic reduction nor global optimality. Original source,
height recipe and zero generated fine cells are checked for every build. Direct16
arrays remain bit-identical to the protected original. Maximum area-fraction
difference between direct/hierarchical is1.97e-13 at16² and6.10e-12 at64². For64²,
all21 direct/hierarchical PNGs are byte-identical, maximum per-ray RGB differences
are7.63e-12, and parent query-work counters match in every state/profile.

All63 generated PNGs are represented in three optically checked original/32²/64²
comparison sheets, including identical direct/hierarchical contents. Every run
retains247,731 original masses in616×624 and the same six camera/caster/foreground
states; all-ready images remain bit-identical to the supplied child baseline.
Only projected ROIs in1280×720 are traced. The images are flat240km/75,894.663844m
controls, not curved-Earth/world/native/frame/p99 evidence. No images are published.

The following pixel RGB RMSE uses complete expectations at the same4×4 spatial
rays against the supplied resolved field. There is no added surface-MC error in
these new comparisons. Foreground is excluded from this table because its plane
changes the physical scene relative to the bare resolved baseline; its geometry,
composition and method equivalence are still checked.

| Light | State | Regions16 | Regions32 | Regions64 |
| --- | --- | ---: | ---: | ---: |
| day | pending | 0.025980 | 0.024733 | 0.023490 |
| day | half_view | 0.031117 | 0.030589 | 0.029806 |
| day | half_caster | 0.034492 | 0.032486 | 0.031202 |
| day | half_ready | 0.019388 | 0.018397 | 0.017102 |
| day | all_ready | 0.000000 | 0.000000 | 0.000000 |
| back | pending | 0.035037 | 0.034910 | 0.034646 |
| back | half_view | 0.032784 | 0.032668 | 0.032378 |
| back | half_caster | 0.038643 | 0.037990 | 0.037451 |
| back | half_ready | 0.024792 | 0.024643 | 0.024326 |
| back | all_ready | 0.000000 | 0.000000 | 0.000000 |
| night | pending | 0.247537 | 0.244669 | 0.242365 |
| night | half_view | 0.254237 | 0.251465 | 0.247792 |
| night | half_caster | 0.267831 | 0.265227 | 0.260610 |
| night | half_ready | 0.173802 | 0.171366 | 0.168638 |
| night | all_ready | 0.000000 | 0.000000 | 0.000000 |

Day pending improves about9.6%; night/backlight improve little on this fixed-ray
control. This is not a generally qualified higher-quality representation. Reusing
the existing jittered16×16 resolved references on their common ROI gives:

| Light / overlap only | Regions16 RMSE | Regions64 RMSE |
| --- | ---: | ---: |
| day | 0.024049 | 0.021674 |
| back | 0.019064 | 0.019123 |
| night | 0.154318 | 0.163487 |

That check supports the day gain, but reverses the night/backlight conclusion.
The parent still uses4×4 rays, the older reference covers only the intersection,
and the existing night16/32 reference discrepancy is0.053445. No convergence,
certified image-error bound or general resolution increase follows. Original pins
are retained. Known model/height/correlation errors, aliasing and hard ready
transitions remain; known children were never blended out or omitted.

Decision: deliver the source-clipping alternative and reproducible source-only
builder, retaining the direct16 default. Use hierarchical64 only as the measured
high-resolution alternative/control; no World representation/profile or native
port is selected. Global source preparation and parent-array budgets still matter.
No new dependency, source/asset cache, export framework, native implementation,
learned model or material/night technique is introduced. Complete commands/input
hashes, attribution, quality/cost negatives, dense-reference checks, pixels and
byte-identical redirects: `/tmp/outshine-research-source-areas-c431da988/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
