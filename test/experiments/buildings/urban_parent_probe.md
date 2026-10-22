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

## Current step: affine categorical shadow runs

`urban_shadow_runs.py` retains fixed centres and the original bilinear density q.
For each categorical height, the projected light footprint minus the known-missed
view footprint leaves at most two intervals per canonical column. Actual box/cap
predicates trim padded integer endpoints, including closed lower-roof contacts.
Own-hit cells are removed. Source knots split intervals into affine-density runs;
two source endpoint samples determine each run. Summing the atomic mark weights
`mu[k]=log((1-q*tail[k+1])/(1-q*tail[k]))` over the surviving intervals reproduces the
categorical conditional void probability without enumerating all those cells.
This aggregates long shadows; primary first-hit transport still traverses cells.

`affine_transmission.py` evaluates each affine log product. For ascending void values
`a+i*d`, its sum is `n*log(d)+logGamma(a/d+n)-logGamma(a/d)`. Cancellation-safe log1p
terms evaluate that difference. Short runs use at most16 direct terms; nearly flat
runs use midpoint/second-moment evaluation. Other runs use at most8 recurrence terms
and the positive-real Stirling correction through inverse power11, with arguments
at least8. Domain:0<=q<=.85,0<=tail<=1, integer0<=n<=2**31. NIST's analytic remainder
result does not certify our complete floating evaluation or a native culling bound.
No SciPy/ML/new dependency is introduced.

`select_shadow_query` and packet work estimates retain original DDA for short or
diagonal beams. Masks, unsupported/grazing configurations and run-budget overflow
also fall back; no probability is discarded. The optional trailing `shadow_query`
Python hook uses the original query arguments. Existing callers/defaults and shared
BoxField/actual3D-clipped DDA contracts remain intact. No native API change.

Frozen code `f25db1d43`, incorporating master `b7bc12683`: **110 focused tests OK**
(17.920s). Added independent affine products versus math.fsum, variable-density
X/Y-major shadows, free corridors, closed-roof contacts, mask/budget fallbacks and
full RGB versus81 independently enumerated box worlds for each of4 light profiles.
All prior source/transport/composition/BoxField tests remain. Ruff/format/function
size checks pass. This is local Python validation, not device admission.

Eight clean terminal0 comparisons retain all247,731 original masses in the616x624
resolved control. The same4x4 jittered samples/seed1113/tolerance1e-8 compare original
categorical DDA, run candidate and resolved geometry. **Only the projected region
is traced**, embedded in1280x720 diagnostic images; this is not a complete frame
benchmark. All24 actual RGB PNGs were inspected, including enlarged crops. All8
candidate PNGs are byte-identical to categorical DDA, coverage is bit-identical,
and maximum per-sample linear RGB difference is6.94e-17. These are different from
the still-material model errors against resolved geometry shown in the last column.

| Case | Rays | DDA s | Candidate s | Runs used | Resolved RGB RMSE |
| --- | ---: | ---: | ---: | --- | ---: |
| dawn-north | 27040 | 9.405 | 4.847 | yes | 0.006249 |
| dawn-slant | 27456 | 11.730 | 6.324 | yes | 0.006897 |
| dawn-diagonal | 29120 | 15.483 | 15.333 | no | 0.003961 |
| day | 24960 | 1.968 | 2.021 | no | 0.025638 |
| back | 24960 | 1.385 | 1.465 | no | 0.034251 |
| night | 27200 | 4.927 | 4.994 | no | 0.285815 |
| ground-north | 33696 | 11.081 | 9.482 | no | 0.001095 |
| ground-motion | 33024 | 11.361 | 9.955 | no | 0.001201 |

The first6 cases use target distance240km and altitude75,894.663844m in flat ENU;
ground cases use50km/2m, the last also moves8km. These distances establish no curved
Earth visibility. Shared Linux host under interactive/other-process load; focused
nice10 tests overlapped the first case. Paired sequential timings support the large
1.85-1.94x low-sun improvement, not small differences in identical DDA paths.
Ground queries all fall back; primary work dominates. No general ground speedup.

For dawn-north, shadow DDA visits2,033,776->25,654; additionally the candidate performs
1,461,740 light endpoint tests and449,902 primary-cap tests. It samples366,734 source
endpoints for183,367 affine runs representing3,051,401 height-mark/cell incidences
(not unique physical cells);2,109,706 incidences use the long-product path. This is
work substitution, not a claim that all operations disappeared. Construction uses
400 named retained array bytes in addition to the shared6,264B parent; measured
named transient peak400,208B excludes untracked NumPy/Python temporaries. Whole
process RSS373.5-538.1MB includes the resolved oracle and image arrays. No full peak
allocation/copied-byte/device-memory bound is claimed. No fine table/GEOS/source
parse is used by run construction or queries; resolved preparation stays separate.

The earlier forced-run day probe was4.0x slower (.433/.107s). Packet dispatch still
added4.3% in one full day pair, motivating the profile-level bypass. Those negative
controls remain in the compact receipt. The final day/back/night/diagonal profiles
now select the original query directly, with no per-packet run dispatch.

Decision: retain as an optional categorical shadow optimization, not a qualified
parent representation. Resolved images still show missing structure/silhouette
contrast and a large night error. Pixel-footprint filtering, cheaper primary
transport, ready-child/foreign-opaque composition, terrain/curvature, transitions
and devices remain open. The earlier far single-sample aliasing case remains valid.
Raw reports, commands, arrays and inspected diagnostics:
`/tmp/outshine-research-urban-runs-f25db1d43/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
| Rational atomic prefix | Exact lattice counts; slow pixel packets and broad intervals, retained as geometric oracle | [425f1d199](https://github.com/Weichwolf/outshine/blob/425f1d199/test/experiments/buildings/urban_parent_probe.md) |
| Coarse joint Poisson void | Correlation improves errors1.97-2.38x; GEOS slow and continuous centres destroy lattice corridors | [99fca1fec](https://github.com/Weichwolf/outshine/blob/99fca1fec/test/experiments/buildings/urban_parent_probe.md) |
| Projected M/J/D/S field | Cheap pixel sampling, cell-dependent construction, blurred boundaries/highlights and horizon bias; not default | [9e87685b8](https://github.com/Weichwolf/outshine/blob/9e87685b8/test/experiments/buildings/urban_parent_probe.md) |
| Direct conditioning/wall sharing | Exact colour/coverage preserved; stored history removed and coincident wall visibility reused | [823c31fe0](https://github.com/Weichwolf/outshine/blob/823c31fe0/test/experiments/buildings/urban_parent_probe.md) |
| Phase/shadow transfer/signed correction | Better joint filtering; still limited phase kernels and reference-sensitive night error | [3ba4d5a12](https://github.com/Weichwolf/outshine/blob/3ba4d5a12/test/experiments/buildings/urban_parent_probe.md) |

Prior raw receipts remain under `/tmp/outshine-research-urban-` with suffixes
`prefix-d4bbacf61`, `poisson-7cc596588`, `response-8282c30a3`, `wall-fdc98f113`, `horizon-3ba4d5a12`,
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
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" \
  --output /tmp/outshine-urban-runs --shadow-runs --paired-exact \
  --distance 240000 --altitude 75894.6638440411 --lighting dawn-north
```

Set PARENT_NPZ/RESOLVED_NPZ to the saved original inputs pinned in the reports;
do not refresh the changed owner URI. Omit distance/altitude for50km/2m;
`--motion 8000` changes the camera. See lighting choices in the driver.
Other retained drivers: `compare_urban_horizon.py`, `compare_urban_response.py`,
`compare_urban_poisson.py` and `compare_urban_layer_filter.py`; their pinned method
versions document sampling, independent controls and qualification limits.

## Primary sources read and visually inspected

- [Bruneton/Neyret2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section3/Figure2/equations1-6: joint visible/lit proportions and projected overlap. The box/atomic derivations here are separate.
- [NIST DLMF5.11](https://dlmf.nist.gov/5.11), equation5.11.1 and section(ii): positive-real logGamma/Stirling expansion and first-neglected-term analytic remainder. Floating implementation accuracy is checked separately.
- [AtCoder floor_sum specification](https://atcoder.github.io/ac-library/production/document_en/math.html): floor-sum definition and bounded-integer logarithmic contract. Our signed/unbounded arithmetic uses an independently implemented Euclidean recurrence; no foreign library code is bundled or its native timing claim inherited.
- [Feibush/Levoy/Cook1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section2.4/Figure2: joint edge/footprint filtering.
- [Han et al.2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures1-2/section3: normal filtering excludes occlusion/geometry simplification/cast shadows.
- [Horizon cloudscapes2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF pages37/81 (slides35/79): coverage/height and alpha1 exit; our finite-error analysis is not attributed to these slides.
- [Sloan/Kautz/Snyder2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction/Figure2/PDF page2: transport/lighting separation, without a general SH/GI guarantee here.
- [Enderton et al.2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf), PDF page2/section3.1/equations1-2: premultiplied depth contributions, no GPU/OIT implementation adopted.
- [Mueller et al.2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page1/Figure1/equation1: baseline-plus-difference identity only; no learned model or unbiasedness claim.
