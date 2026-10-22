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

## Current step: atomic prefix reference

`lattice_footprint.py` counts integer centres in a closed convex rational polygon
without enumerating cells. Vertical decomposition sums floors of upper/lower edge
lines. Euclidean reciprocity evaluates these floor sums; a separately owned final
integer column handles vertices/vertical edges. Strict interior inequalities become
integer-equivalent closed inequalities. Footprints are exact rational Minkowski sums
of the height-clipped view segment and the recipe square, in canonical-centre units.
Python Fraction/unbounded integers are a geometric reference, not a native design.

`urban_parent_prefix.py` retains the original fixed lattice, height probabilities
and bilinear source-density model. Atomic log weights are
`mu[k]=log((1-q*tail[k+1])/(1-q*tail[k]))`. Their sums reproduce categorical void
probabilities; the prior125-case height-threshold enumeration checks that identity.
Disjoint coarse interpolation blocks bound q using their integer-rectangle corners.
Monotone mu bounds times strict/closed lattice counts give a transmission interval.
The counter is exact for its rational polygons; floating density/log/exp guards
have **not** been proved as native directed-rounding/culling bounds.

The parent-only constructor has289 blocks,1,156 density-corner evaluations and
32,368 named retained array bytes. No source parse, GEOS or fine-child generation is
needed. Fraction allocation peaks, index/dictionary overhead and full process/device
residency are not established by those named arrays or integer-bit counters.

Frozen code `d4bbacf61`, integrated with master `f6ab4d255`: **102 focused tests OK**
(10.514s), including the shared BoxField/column transition. Tests cover signed sums,
750 independent GEOS polygon/interior/rectangle point-membership comparisons,
degenerate/tangent boundaries, large integer arithmetic, a known clipped-boundary
counterexample, and70 nonuniform slanted prefixes against individual box expectation.
A trillion-cell-length corridor stays empty; a parallel occupied strip counts its
canonical centres. These are mathematical/functional controls, not image admission.

Four clean terminal0 diagnostics reuse the original stored parent and1280x720
viewport. All5,828 sampled DDA prefix probabilities lie in the reported intervals.
Sixteen actual grayscale coverage/width PNGs were inspected through four sheets.
**These are coverage-only diagnostics, not RGB renders or a pixel-footprint filter.**

| Case | Rays | DDA s | Rational interval s | Mean width | Maximum width |
| --- | ---: | ---: | ---: | ---: | ---: |
| Elevated240km |1472 |.0208 |9.981 |.05991 |.56579 |
| Ground50km |2004 |.9144 |8.643 |.00619 |.30573 |
| Ground50km/motion8km |1968 |.9328 |11.477 |.00488 |.29875 |
| Ground240km |384 |.00065 |.0969 |0 |0 |

The ground240km single-sample rays miss the subpixel mass in both methods. This is
an explicit aliasing counterexample, not proof of an empty/far-ready scene. Flat2m
observers at50/240km also establish no real-Earth terrain/curvature visibility.
55.77% of elevated intervals exceed .01 width; the high .56579 maximum precludes
using these data as a qualified image-error substitute. No first-hit depth, normal,
radiance, ready-child/foreign-opaque composition or native consumer is delivered.

The separate long-axis probe preserves the original free corridor with T=[1,1].
Potential canonical cells grow375,156->95,736,440 while the fixed256-bin pattern is
stretched. Two queries use272 counted polygons/272 Euclidean steps at all3 sizes;
reference DDA visits613/742/776, stopping when double transmission underflows to0.
Intervals take .119/.083/.098s versus .436/.465/.612s for that small probe, but the
pixel packets above are much slower. This is no equal-quality global scene expansion
or runtime speedup claim. Endpoint uncertainty and source-density ranges remain
explicit. Do not port the rational implementation as the proposed hot path.

Next work needs cheap packet queries with retained discrete support, tighter useful
source-density evaluation and valid pixel filtering before C/T/S/radiance integration.
The old categorical renderer/default remains unchanged. Raw terminal reports,
commands, all arrays, probes and diagnostics:
`/tmp/outshine-research-urban-prefix-d4bbacf61/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
| Coarse joint Poisson void | Correlation improves errors1.97-2.38x; GEOS slow and continuous centres destroy lattice corridors | [99fca1fec](https://github.com/Weichwolf/outshine/blob/99fca1fec/test/experiments/buildings/urban_parent_probe.md) |
| Projected M/J/D/S field | Cheap pixel sampling, cell-dependent construction, blurred boundaries/highlights and horizon bias; not default | [9e87685b8](https://github.com/Weichwolf/outshine/blob/9e87685b8/test/experiments/buildings/urban_parent_probe.md) |
| Direct conditioning/wall sharing | Exact colour/coverage preserved; stored history removed and coincident wall visibility reused | [823c31fe0](https://github.com/Weichwolf/outshine/blob/823c31fe0/test/experiments/buildings/urban_parent_probe.md) |
| Phase/shadow transfer/signed correction | Better joint filtering; still limited phase kernels and reference-sensitive night error | [3ba4d5a12](https://github.com/Weichwolf/outshine/blob/3ba4d5a12/test/experiments/buildings/urban_parent_probe.md) |

Prior raw receipts remain under `/tmp/outshine-research-urban-` with suffixes
`poisson-7cc596588`, `response-8282c30a3`, `wall-fdc98f113`, `horizon-3ba4d5a12`,
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
  --parent /tmp/outshine-urban-control/parent.npz \
  --output /tmp/outshine-urban-prefix --altitude 2 --distance 50000 --scaling-probe
```

Omit altitude for the elevated view; `--motion 8000` changes the camera.
Other retained drivers: `compare_urban_horizon.py`, `compare_urban_response.py`,
`compare_urban_poisson.py` and `compare_urban_layer_filter.py`; their pinned method
versions document sampling, independent controls and qualification limits.

## Primary sources read and visually inspected

- [Bruneton/Neyret2012](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf), section3/Figure2/equations1-6: joint visible/lit proportions and projected overlap. The box/atomic derivations here are separate.
- [AtCoder floor_sum specification](https://atcoder.github.io/ac-library/production/document_en/math.html): floor-sum definition and bounded-integer logarithmic contract. Our signed/unbounded arithmetic uses an independently implemented Euclidean recurrence; no foreign library code is bundled or its native timing claim inherited.
- [Feibush/Levoy/Cook1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section2.4/Figure2: joint edge/footprint filtering.
- [Han et al.2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures1-2/section3: normal filtering excludes occlusion/geometry simplification/cast shadows.
- [Horizon cloudscapes2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF pages37/81 (slides35/79): coverage/height and alpha1 exit; our finite-error analysis is not attributed to these slides.
- [Sloan/Kautz/Snyder2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction/Figure2/PDF page2: transport/lighting separation, without a general SH/GI guarantee here.
- [Enderton et al.2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf), PDF page2/section3.1/equations1-2: premultiplied depth contributions, no GPU/OIT implementation adopted.
- [Mueller et al.2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page1/Figure1/equation1: baseline-plus-difference identity only; no learned model or unbiasedness claim.
