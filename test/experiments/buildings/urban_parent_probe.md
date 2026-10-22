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

## Current step: affine primary-prefix transmission

`urban_prefix_runs.py` clips a view prefix to the actual3D parent bounds, then
projects its part within each categorical height slab. Canonical centres inside
the swept square form an integer interval in each minor-axis column. Original
box predicates trim padded endpoints, preserving roof/corner contacts and open
finite stops. Source interpolation knots split intervals into affine-density runs.
Their atomic log weights `mu[k]=log((1-q*tail[k+1])/(1-q*tail[k]))` sum to the same
categorical void probability as the per-cell control. Fixed streets/corridors and
the original bilinear q remain; no continuous-centre/Poisson smoothing is introduced.

`urban_lattice_runs.py` now owns shared centre-interval geometry and source/gamma
products for primary and conditional-shadow consumers. The previous math was moved,
not duplicated. `urban_parent_prefix.py` owns the unchanged DDA control, reexported
by its former diagnostic driver. The earlier rational counter remains an independent
geometric reference. Gamma domain/rounding limits and primary sources are documented
in the pinned affine-shadow method below:0<=q<=.85, counts<=2**31, no native
floating-error/culling certificate. Probability alone supplies no first-hit depth,
normal, radiance, opaque/ready-child composition, real-World visibility or Empty.

Initial shadow-style angle heuristics left a few long rays in DDA, retaining its
long packet loop. Final selection uses affine runs for estimated spans>16 cells;
short prefixes keep DDA. A cumulative131,072-column packet budget also falls back
rather than silently dropping rays. Forced-run timings retain the slower elevated
case. This is a measured Python work choice, not a globally optimal/native policy.
Construction/query never parses source, calls GEOS or generates fine children.
The shared parent remains6,264B; additional named retained arrays total400B.

Frozen `634df149c`, based on master `81cf4fc8a`: **122 focused tests OK (13.163s)**.
New180 nonuniform prefixes match independently intersected categorical boxes; tests
also cover closed roof/corner contacts, open stops, long/reversed varying-density
runs, exact free corridors, empty/invalid input, zero and cumulative packet budgets.
Existing source/phase/transport/opaque/shared-field and independent RGB box-world
controls remain. Ruff/format/function-size/diff/docs checks pass; no native gate.

Six clean frozen runs are terminal0. Five full-field prefix comparisons produce20
actual1280x720 grayscale PNGs; all were inspected through full/crop/error sheets.
All5 candidate Coverage PNGs are byte-identical to categorical DDA; maximum sampled
transmission difference5.55e-16. A separate shared-shadow consumer run preserves
all RGB arrays/3 PNGs bit for bit against `f25db1d43`, bringing inspected PNGs to23.
That is a refactor regression check, not primary-prefix RGB integration.

Every image control retains all247,731 original masses in the616x624 resolved field.
Only the projected ROI is sampled, not a complete frame; resolved preparation/query
is separate from saved-parent consumption. Reference Coverage is unfiltered point
sampling. Its remaining source/model/silhouette differences and the elevated lattice
aliasing remain visible. Flat2m/50-240km views establish no curved-Earth visibility.

| Case | Rays | DDA s | Selected runs/DDA s | Forced runs s | Max T difference |
| --- | ---: | ---: | ---: | ---: | ---: |
| elevated | 1560 | 0.021311 | 0.022820 | 0.045340 | 0.00e+00 |
| ground | 2022 | 0.850096 | 0.075662 | 0.063418 | 5.00e-16 |
| motion | 1986 | 0.959231 | 0.132604 | 0.117931 | 4.44e-16 |
| diagonal | 2412 | 1.209029 | 0.462306 | 0.434360 | 5.55e-16 |
| far-ground | 390 | 0.000646 | 0.000686 | 0.000552 | 0.00e+00 |

Elevated:240km target/75,894.663844m altitude. Ground/motion/diagonal:50km target/2m
altitude, respective motion0/8/35km. Far-ground:240km/2m. Shared Linux host under
interactive load, no concurrent own tests during normal timing. Gains11.2x/7.2x/2.6x
apply to this scalar query. Short-view selection has overhead; forced elevated runs
are slower. Python batching/vectorization timings are no device/frame/p99 evidence.

Ground replaces184,793 DDA cell visits/739,172 height tests with14,036 column
intervals,58,424 endpoint tests,34,476 source endpoints and24 remaining DDA visits.
17,238 affine runs represent138,654 height-mark/cell incidences, not unique cells.
Diagonal uses224,440 density endpoints versus238,910 DDA visits, plus443,220 endpoint
tests: its2.6x Python gain is not a claim of comparable hardware-independent work
reduction. Memory/locality and native evaluation still decide that tradeoff.

| Query-only tracemalloc observation | DDA peak B | Selected peak B | DDA / selected live blocks |
| --- | ---: | ---: | --- |
| ground | 434279 | 5941480 | 28 / 78 |
| diagonal | 515382 | 33007600 | 28 / 78 |

Memory profiling is a separate untimed query pass; counts are observed live blocks,
not total allocation count or a complete native allocator bound. Named geometry/run
arrays and known selected-input copies are reported independently: ground23,640B,
diagonal28,088B known input copies; other temporary copies are not counted by
these partial counters. Whole prefix-process RSS133-165MB includes resolved oracle/images;
construction is outside the traced query. The column budget does not certify an
app/device memory budget. In particular,33MB diagonal transient cost is material.

In the separate long-axis probe, potential cells grow375,156->95,736,440 while the
same coarse pattern is stretched. Two queries retain4 counted intervals/68 affine
runs/136 source endpoints at all3 sizes. The parallel free corridor remains T=1.
This is a complexity probe, not equal-quality global content expansion. Occupied
transmission underflows at the larger sizes, as in DDA; underflow supplies no World occluder certificate.
Far-ground point samples still miss the thin mass in all methods. The previous
pixel-area filter addresses that distinct aliasing case; no such filter is completed
by a faster scalar prefix alone.

Decision: retain the optional scalar query. Next need is to consume its cheap
transmission in true pixel-footprint integration and recover useful first-hit/light
response before an RGB/opaque/ready-child substitute. Terrain/curvature, source
height evidence, temporal transitions and devices remain open. Current source pins,
commands, terminal tests/reports, observed memory, arrays and inspected diagnostics:
`/tmp/outshine-research-primary-runs-634df149c/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
python -B -W error test/experiments/buildings/compare_urban_prefix.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --runs --trace-memory \
  --output /tmp/outshine-urban-primary --distance 50000 --altitude 2 --scaling-probe
```

Set PARENT_NPZ/RESOLVED_NPZ to the saved original inputs pinned in the reports;
do not refresh the changed owner URI. Omit distance/altitude for50km/2m;
`--motion 8000` changes the camera;35000 gives the diagonal case.
Other retained drivers: `compare_urban_horizon.py`, `compare_urban_response.py`,
`compare_urban_poisson.py` and `compare_urban_layer_filter.py`; their pinned method
versions document sampling, independent controls and qualification limits.

## Primary sources read and visually inspected

- [Bruneton/Neyret2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section3/Figure2/equations1-6: joint visible/lit proportions and projected overlap. The box/atomic derivations here are separate.
- [NIST DLMF5.11](https://dlmf.nist.gov/5.11), equation5.11.1 and section(ii): positive-real logGamma/Stirling expansion and first-neglected-term analytic remainder. Floating implementation accuracy is checked separately.
- [AtCoder floor_sum specification](https://atcoder.github.io/ac-library/production/document_en/math.html): floor-sum definition and bounded-integer logarithmic contract. Our signed/unbounded arithmetic uses an independently implemented Euclidean recurrence; no foreign library code is bundled or its native timing claim inherited.
- [Feibush/Levoy/Cook1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section2.4/Figure2, PDF pages3-4: clipping and weighting in the actual pixel footprint. Re-read optically for this step; the coarse support/strip quadrature here is separate.
- [Han et al.2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures1-2/section3: normal filtering excludes occlusion/geometry simplification/cast shadows.
- [Horizon cloudscapes2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF pages37/81 (slides35/79): coverage/height and alpha1 exit; our finite-error analysis is not attributed to these slides.
- [Sloan/Kautz/Snyder2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction/Figure2/PDF page2: transport/lighting separation, without a general SH/GI guarantee here.
- [Enderton et al.2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf), PDF page2/section3.1/equations1-2: premultiplied depth contributions, no GPU/OIT implementation adopted.
- [Mueller et al.2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page1/Figure1/equation1: baseline-plus-difference identity only; no learned model or unbiasedness claim.
