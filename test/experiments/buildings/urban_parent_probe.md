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

## Sampled first-hit RGB control

`urban_first_hit.py` samples the first geometric event from the original categorical
CDF. DDA visits implicit cells in order and stops at the selected event. Height
labels in one cell are mutually exclusive: sort their actual entry depths, keep
one pre-cell transmission for all labels, and update it only after the whole cell.
Coincident roof/wall events retain their separate labels/normals. This does not
instantiate a random world or prepare/cache fine geometry.

`urban_sampled_parent.py` combines this sample with cheap full-prefix transmission.
Let T be the miss probability, F=1-T, B the miss-conditioned background radiance and
R the hit-conditioned surface radiance. It evaluates `T*B + F/M*sum(R[i])` with
jittered strata in the hit CDF. Coverage and background remain deterministic; only
the surface expectation is sampled. In real arithmetic this targets the existing
independent-cell categorical expectation, not the particular resolved population.
Gamma/rounding limits and finite-sample variance remain separate; no full numeric
unbiasedness/culling certificate is claimed.

The surface uses the same geometry, GGX shader, albedo, height priors and exact
view-conditioned shadow query as its control. Repeated identical points/normals
within a ray share shading; distinct heights remain in the sampled geometry.
Sharing currently assumes the control's single urban material. Background is
conditioned on the complete missed view prefix. A separate SeedSequence stream
keeps CDF randomness separate from spatial jitter, and stream consumption is
independent of batch partition. This is not temporal reprojection or a stability
proof. No native interface, source/asset format or default renderer changes.

Frozen `de291413a` includes master `a9fca5d27`; **126 focused tests OK (15.948s)**.
Independent first-event PMFs enumerate81 four-box worlds for three ray directions.
Exact rational strata reproduce independently weighted RGB/coverage for3 views x4
lights, including background/known-miss shadow conditioning. Roof/wall contacts,
open stops, height labels, batch invariance and free corridors are checked. Prior
source/transport/opaque/shared-field controls remain. Ruff/format/function-size,
diff and docs checks pass; no native/device gate is implied.

Nine clean frozen runs are terminal0;27 actual1280x720 RGB PNGs were inspected through
full/crop/error sheets. Each keeps all247,731 masses of the original616x624 resolved
field. Only its projected ROI is traced, not an entire frame. Same4x4 spatial rays,
seed1113, profiles and tolerance1e-8 are used in the paired categorical control;
its existing radiance termination bound is negligible beside sampling noise.
The old default's arrays/PNGs remain bit-identical to `f25db1d43`'s day control.

The table separates added sampling error from the unchanged model's error against
resolved geometry. Linear RGB RMSE is over the ROI after spatial averaging. A noisy
sample can accidentally lie closer to one resolved population; that does not prove
better model accuracy. Coverage differences stay within the control's1e-8 early-
termination bound plus3e-12 numeric tolerance; this is not a World/Empty certificate.

| Case / M | Control s | Sampled s | Added sampling RMSE | Control→resolved | Sampled→resolved |
| --- | ---: | ---: | ---: | ---: | ---: |
| ground-day /4 | 21.737 | 8.525 | 0.002482 | 0.009875 | 0.010003 |
| ground-back /4 | 13.539 | 8.722 | 0.002247 | 0.005051 | 0.005646 |
| ground-night /4 | 38.910 | 9.065 | 0.017211 | 0.052657 | 0.052665 |
| ground-motion /4 | 27.060 | 13.584 | 0.000515 | 0.007829 | 0.007822 |
| elevated-day /4 | 2.374 | 2.898 | 0.003303 | 0.025638 | 0.025790 |
| dawn-north /4 | 3.499 | 4.897 | 0.001173 | 0.006249 | 0.006326 |
| ground-night16 /16 | 35.435 | 17.477 | 0.006802 | 0.052657 | 0.053858 |
| ground-night-seed2 /4 | 35.378 | 8.451 | 0.022203 | 0.052657 | 0.055832 |

Ground cases:50km target/2m altitude, motion8km where named. Elevated/dawn:240km
and75,894.663844m altitude in flat ENU; none proves curved-Earth visibility. Shared
Linux host under interactive load; no own tests/profiling overlap normal timings.
M=4 ground gains are1.55-4.29x, but elevated/dawn are slower. No universal speedup,
equal-RGB claim, whole-frame/p99 result or ready-child/foreign-surface admission.
Night M=16 reduces measured sampling error.017211->.006802 at higher cost. Two M=4
streams differ.030344 RMS (two-stream noise estimate.021456); neither this pair nor
one M=16 run proves convergence or temporal stability. Visible grain remains.

Ground-day work:1,688,556->1,073,612 primary cell visits, plus445,397 prefix affine
runs/890,794 density endpoints and660 prefix DDA visits. This is extra prefix work,
not free acceleration.20,896 surface samples produce20,484 unique shading queries;
urban caster visits2,772,110->46,963, plus183 background caster visits. Named sampled
surface arrays peak524,288B; prefix named run arrays peak3,432,656B. A derived lower
bound on selected sample-input copies is56B x20,896=1,170,176B; other copies are not
counted. The unchanged parent is6,264B and prefix metadata400 named array bytes.

A separate untimed full-ray ground-day tracemalloc pass observes peak allocations
3,150,903B control versus11,617,712B sampled; live blocks1930 versus963.

Prepared input arrays are outside that trace and outputs remain retained. These are
observed peaks/live blocks, not total allocations/copies or native memory bounds.
Whole-process RSS includes both controls, full images and the resolved oracle;
shared memory/device budgets and actual A18/Iris execution remain unmeasured.

The retained sparse-ray counterexample scales the same coarse pattern's density
by1e-4 and its span by.25/1/4. Four hit-conditioned draws visit311/1,225/4,790 cells
and take.103/.416/1.627s. The full-T prefix stays coarse, but direct first-hit walking
still scales with sparse ray length. Thus this does not solve global far work
independently of recipe-cell count. That gap cannot be hidden by dense urban gains.

Decision: keep optional `--surface-samples` research mode. It is a valid first-hit
consumer with measured RGB/work tradeoffs, not a native port/replacement release.
Next need is bounded sparse traversal and useful spatial/temporal integration while
preserving corridors, conditional lighting and the ready/opaque contract. No separate
material/night technique was added. Original-height evidence, terrain/curvature and
devices remain open. Raw commands, source hashes, test/memory logs, arrays, image
checks and counterexamples: `/tmp/outshine-research-first-hit-de291413a/receipt.json`.

## Sparse first-hit prefix inversion

`urban_prefix_first_hit.py` adds optional `--first-hit-prefix` to the sampled path.
It preserves uniforms, event ordering, heights/normals, coverage, conditional light
and the shader. The prefix uses the same saved coarse parent, never generated fine
children or a per-cell CDF/alias table. Default rendering remains unchanged.

Long rays spanning more than256 major-axis slabs and at most4 minor-axis cells
first run up to64 original packet-DDA steps. Early hits/misses finish directly.
Only remaining samples invert full-prefix transmission by binary search at whole
major-axis cell boundaries. The final slab resumes the original geometric events
with its pre-slab transmission; a finite stop remains open. This relies on gaps
between footprints; footprint fraction1 and primary replacement masks retain DDA.
Short/wide rays retain DDA. `PrefixFirstHits(..., force=True)` bypasses the scout
and geometric cost selection only for comparative tests, not the renderer.

A transmission comparison within1e-10 of the sampled threshold, or a lost tail
hit, restarts original DDA. This is an empirical numerical guard, not a certified
floating-error bound. Extremely small densities deliberately expose its fallback;
zero/near-zero probability is not a native Empty/occluder certificate. Binary
search also does not remove the prefix's minor-axis/source-knot/height costs.

Frozen `65c7f40bf` includes code `fdea3af9b` and master `aecc9e7a5`.
132 focused tests pass in16.105s: independent81-world PMFs,120 nonuniform forced
rays with arbitrary angles/heights/misses/stops, reversed long sparse rays, roof/
wall contacts, CDF-boundary fallback, exact RGB/batch equality and prior controls.
Four clean full-field runs (day/night/elevated/dawn) retain247,731 masses and the
original616x624 oracle. All arrays and12 actual PNGs are bit-identical to the
previous `de291413a` first-hit delivery;12 PNGs rechecked optically. This preserves
its noise and source/model/silhouette deficits; it supplies no new quality claim.
They still trace only the projected ROI inside1280x720, not an admitted world frame.

| Span / density / XY slope | DDA s | Selected s | Forced s |
| --- | ---: | ---: | ---: |
| 0.25 / 0.0001 / 0 | 0.0932 | 0.0954 | 0.1032 |
| 1 / 0.0001 / 0 | 0.3562 | 0.1874 | 0.1767 |
| 4 / 0.0001 / 0 | 1.4685 | 0.2055 | 0.1883 |
| 1 / 1 / 0 | 0.0040 | 0.0066 | 0.0970 |
| 1 / 0.0001 / 0.5 | 0.5604 | 0.5362 | 0.2323 |
| 1 / 0.0001 / 1 | 0.3941 | 0.3552 | 0.1810 |
| 4 / 1e-12 / 0 | 1.4819 | 1.5470 | 1.6267 |

These are four identical hit-conditioned draws in density-scaled versions of the
same coarse pattern, not reduced-content substitutes for the complete-city gate.
The prefix and direct consumers receive the same inputs/uniforms and produce
identical depth, cell, height label and normal arrays. The .25-span case retains
DDA. At spans1/4, scout+tail visit260 cells instead of1,225/4,790; search uses37/45
prefix comparisons, plus4 full-T queries. Total source-density endpoints are
3,240/3,736; those are additional work, not free skipping. For fixed coarse layout
and minor extent, search rounds grow logarithmically with major extent; neither
all directions nor the ambiguous fallback has that bound.

Paired full-scene sampler timings: ground-day: 8.443->9.587s (+13.5%); ground-night: 8.600->9.109s (+5.9%).
They use identical prepared rays/light and run sequentially at normal priority.
No own tests/profiling overlap; interactive host load remains. This is overhead,
not a complete-city acceleration. Only224/208 surface draws use inversion; the
day direct visits fall1,073,612->1,054,141 with1,987 additional prefix comparisons.
The separate elevated/dawn runs choose no inversion; differences versus older
process timings are not algorithmic gains.

A separate256-ray,4-span sparse allocation profile observes direct peak371,657B
versus selected1,338,944B, live blocks119/166. Prepared inputs/model stay outside
tracing; outputs are retained. Instrumented times are not normal timing evidence.
Selected-input copy counters are lower bounds, not all copies/allocations; source
endpoints, scouting, tail and fallback work remain separate. No new retained
per-cell arrays are added; device allocation and actual GPU costs remain unmeasured.

Decision: retain this narrow optional alternative and its negative cases. Dense
forced search is much worse; diagonal/wide rays remain linear under selection,
and tiny-density numeric fallback remains linear. The original sampled RGB noise,
pixel/temporal integration and ready/opaque composition are still open. No native
port request, material/night technique, source refresh or world/device admission.
Reproduce the previous image command with `--first-hit-prefix --sparse-probe`;
full commands, hashes, terminal states, memory/paired measurements and duplicate
redirects: `/tmp/outshine-research-prefix-hit-65c7f40bf/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
