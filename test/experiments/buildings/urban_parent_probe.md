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

## Current step: share first-hit geometry across surface draws

`urban_grouped_first_hit.py` walks each view ray once for all its inverse-CDF
surface draws. It evaluates density, intersections, height ordering and pre-cell
transmission once per visited cell, then finds the first crossed height for each
still-pending uniform. Labels in one cell remain mutually exclusive: all use the
same pre-cell T; T changes only after the cell. Closed roof/wall ties, open stops,
source corridors and primary replacement masks retain the original predicates.
Unsorted and duplicate uniforms work; output keeps the original ray-major order.

This shares only identical geometric rays, not neighboring pixels or shadow rays.
The original independent draw kernel remains the correctness/cost control. M=1
reuses it directly. The common saved parent and `ordered_events`/DDA definitions
remain single implementations; no fine children, per-cell table, cache, learned
model, new dependency or native interface is added.

`urban_sampled_parent.py` retains the estimator `T*B+(1-T)/M*sum(R_i)`, the separate
CDF RNG stream, full-prefix T, miss-conditioned background, source heights, GGX
material and exact conditional shadows. Sharing changes neither sample values nor
the model/variance target. Coincident shading retains the control's single urban
material assumption. Gamma/rounding and finite-sample limits remain separate;
this is no complete numeric unbiasedness, culling or temporal-stability certificate.

The Python sampled consumer selects sharing for M>=4 and repeated draws for M<4;
this is a measured research choice, not global optimality. `--repeat-first-hits`
selects the independent control; `--group-first-hits` forces sharing. The separate
`--first-hit-prefix` study disables automatic sharing; explicit combination is
rejected. Native rendering and the categorical non-sampled control are unchanged.
Extremely sparse/long paths still visit cells: sharing removes repeated work,
not the underlying per-ray far-work dependence. The prefix alternative and all
its slower dense/numerically ambiguous cases remain pinned below.

Frozen `7e26abdc1` includes feature `8a7c2ba9b` and master `7a8000922`.
Later reporting removes only an unused packet-width field; numerical code is unchanged.
138 focused tests pass in19.212s:81 independent box-world PMFs, reversed/duplicate
uniforms,60 nonuniform rays with arbitrary directions/stops and M=1/2/4/16/512,
roof/wall/CDF ties, replacement masks, exact RGB/batch equality and prior controls.
Nine clean frozen runs retain247,731 original masses/616x624, including day/back/
night/motion/elevated/dawn, M=16, a second night stream and the old default control.
Every array and27 actual PNGs are bit-identical to the previous first-hit delivery;
27 PNGs rechecked optically. Original source/model/silhouette/aliasing errors and
visible Monte Carlo grain therefore remain. Earlier night M=4/M=16 sampling errors
.017211/.006802 are preserved, not a new equal-time or convergence guarantee.

| Case / M | Repeated s | Shared s | Speedup |
| --- | ---: | ---: | ---: |
| ground-day | 8.392 | 7.049 | 1.19x |
| ground-night | 8.789 | 7.846 | 1.12x |
| ground-night16 | 17.256 | 9.676 | 1.78x |
| elevated-day | 2.300 | 1.833 | 1.25x |
| dawn-north | 4.301 | 3.805 | 1.13x |

Each pair uses the same complete stored field, prepared spatial rays/light and
uniforms on Linux/i5-1135G7 CPU. Two rounds reverse method order; report medians of
two calls, not a statistical/device qualification. No own tests/profiling overlap
normal timings; interactive load and thermal/frequency changes remain possible.
Only projected ROIs inside1280x720 are traced, not complete native frames. Ground
uses50km/2m; elevated/dawn240km/75,894.663844m in flat ENU, no curvature visibility.

| Case | Repeated height tests | Shared height tests | Shared threshold comparisons |
| --- | ---: | ---: | ---: |
| ground-day | 4,294,448 | 2,327,528 | 9,310,112 |
| ground-night | 4,338,716 | 2,348,600 | 9,394,400 |
| ground-night16 | 17,263,664 | 3,384,848 | 54,157,568 |
| elevated-day | 1,196,132 | 343,340 | 1,373,360 |
| dawn-north | 1,201,740 | 344,852 | 1,379,408 |

M4 day geometric visits1,073,612->581,882 and known geometry-input copies
1,170,176->292,544B. These are selected input-array copies, not all copied bytes.
The grouped implementation also performs9,310,112 threshold comparisons versus
4,294,448 in the repeated event matrices. Cheap comparison work grows while
expensive density/geometry work shrinks; do not treat cell visits as total work.
Shadow work, shaded surface draws and the full-prefix stage remain unchanged.
All finite work/copy counters stay in the reports; no hardware speedup is inferred
solely from Python vectorization or this operation mix.

| Surface draws / ray | Repeated peak B | Shared peak B | Repeated/shared live blocks |
| --- | ---: | ---: | ---: |
| 4 | 2,513,188 | 878,614 | 118/78 |
| 16 | 9,426,765 | 1,368,854 | 46/51 |

The separate traced queries use512 actual positive city rays spread across the
full original ray set. Geometry-input copies are inside the trace; model/T/light
preparation is outside and outputs stay retained. Peaks/live blocks are observed
Python/NumPy measurements, not all allocations, total renderer/app memory, GPU
storage or device budgets. The grouped boolean temporary scales as ray packet x
M x height labels; M is bounded at512 and renderer packets at2048. Normal timings
exclude these instrumentation runs. Full-process RSS also includes both controls,
resolved oracle and image storage; it is not a sampler allocation bound.

Decision: use shared traversal in the Python M>=4 sampled reference, retain explicit
independent and prefix controls, and pass the work/quality evidence onward. This
improves an existing full-city consumer without a new material/night technique.
Spatial/temporal filtering, broadly bounded far work, ready/opaque composition,
original heights, terrain/curvature and native A18/Iris/p99 remain unqualified.
Commands, source hashes, terminal logs, paired/memory data, image checks and bit-
identical duplicate redirects: `/tmp/outshine-research-grouped-hit-7e26abdc1/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
```

Set PARENT_NPZ/RESOLVED_NPZ to the saved original inputs pinned in the reports;
do not refresh the changed owner URI. Omit distance/altitude for50km/2m;
`--motion 8000` changes the camera;35000 gives the diagonal case.
Other retained drivers: `compare_urban_horizon.py`, `compare_urban_response.py`,
`compare_urban_poisson.py` and `compare_urban_layer_filter.py`; their pinned method
versions document sampling, independent controls and qualification limits.

## Primary sources read and visually inspected

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
