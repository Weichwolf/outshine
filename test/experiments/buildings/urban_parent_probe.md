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

## Current step: sampled transport at opaque stops and ready children

`urban_sampled_layer.py` returns parent-only premultiplied C, transmission T and
miss-conditioned receiver shadow S for unit pixel rays. Finite stops are open;
infinity means sky. Receiver normals may be supplied. Camera and caster coverage
are independent caller declarations of complete ready cells, including vacancies;
source/asset presence alone grants neither declaration. The caller retains its
known opaque surface and non-parent direct-light visibility.

The existing grouped first-hit estimator supplies only C. Complete-prefix T and
receiver S remain deterministic, conditioned on the same missed view prefix.
Additional deterministic caster visibility modulates direct parent lighting while
ambient stays unchanged. Compose per ray as `C+T*(S*B_direct+B_other)` or filter the
existing signed correction jointly. Do not clamp negative corrections or average
T and S independently. Coincident shading keeps the existing single urban material
assumption; this is the control's single direct-light profile, not general GI.

Shared `transport_inputs` validates the old/new layer domains. Masked transmission
uses the existing exact categorical DDA; affine runs are not applied through ready
masks. Unmasked queries retain the previous selector. Known child cells are removed
only from their declared camera/caster role. Both fully ready domains return0/1/1
before sampling/model/traversal. Complete unions now work as well as one whole-grid
rectangle: a clipped boundary sweep uses O(R) memory and O(R^2 log R) worst-case work
in rectangle count R, not a world-sized cell table. Holes/overlaps/outside portions
are checked against200 independent small cell masks and huge symbolic grids.
Partial multi-rectangle costs are not qualified by the one-rectangle timings below.

The new caster-only case exposed a reference crash: a camera-unknown background
point may lie inside a known closed caster. `column_field_query.trace` now has an
explicit `inside_is_hit` mode, used only by child visibility. Initial containment
counts as a hit at minimum; no surface normal is promised for that hit. Default
camera queries retain their occupied-start rejection; standard-column mode only.
This does not imply any native raster/near-clip shader change (master #1113/6353898353).

Measured `beaba3860` includes feature `0271372c9` and master `54fa7db0e`.
The later `334bfcff1` extends only ready-union detection/docs/tests; all timed masks
contain0/1 rectangles. Final151 focused tests pass in25.238s;148 passed on the
measurement freeze. They include81 independently enumerated box worlds across
finite stops/height mixtures/sun/point lights/independent replacements, nonuniform
conditional T/S controls, ready-union/hole/scale cases, contained caster/camera
separation, external direct-only shadows and original first-hit/layer/field tests.

Six clean paired image runs retain247,731 original masses/616x624: elevated day,
backlight, night, grazing day and8km motion, plus night with16 surface draws. Each
checks pending, view-only, caster-only, half-ready, full-ready and a foreground
plane covering75% of the field width. Spatial samples stay4x4 in1280x720; only the
projected ROI is evaluated. Altitudes/distances:75,894.663844m/240km elevated,
2,000m/50km grazing. Flat ENU controls do not prove curved-Earth visibility or a
complete native frame. All full-ready composites are bit-identical to their supplied
child baseline.87 actual PNGs (78 layer/control plus9 legacy) were optically checked;
the3 legacy image/array sets remain bit-identical to `de291413a`.

| Case / draws | State | Expectation s | Sampled s | Added pixel RGB RMSE |
| --- | --- | ---: | ---: | ---: |
| elevated-day/4 | pending | 1.955 | 1.459 | 0.003322 |
| elevated-day/4 | half_view | 1.276 | 1.045 | 0.002378 |
| elevated-day/4 | half_caster | 2.594 | 1.540 | 0.004019 |
| elevated-day/4 | half_ready | 1.617 | 1.080 | 0.002378 |
| elevated-day/4 | all_ready | 0.040 | 0.004 | 0.000000 |
| elevated-day/4 | foreground | 1.118 | 0.600 | 0.001629 |
| elevated-back/4 | pending | 1.449 | 1.217 | 0.011649 |
| elevated-back/4 | half_view | 0.854 | 0.891 | 0.008149 |
| elevated-back/4 | half_caster | 2.115 | 1.324 | 0.011344 |
| elevated-back/4 | half_ready | 1.215 | 0.998 | 0.008149 |
| elevated-back/4 | all_ready | 0.056 | 0.004 | 0.000000 |
| elevated-back/4 | foreground | 0.866 | 0.582 | 0.005315 |
| elevated-night/4 | pending | 4.401 | 2.689 | 0.075841 |
| elevated-night/4 | half_view | 3.432 | 2.120 | 0.041518 |
| elevated-night/4 | half_caster | 7.486 | 2.958 | 0.088138 |
| elevated-night/4 | half_ready | 4.286 | 2.210 | 0.041518 |
| elevated-night/4 | all_ready | 0.044 | 0.004 | 0.000000 |
| elevated-night/4 | foreground | 2.985 | 1.244 | 0.002023 |
| grazing-day/4 | pending | 28.150 | 13.806 | 0.006018 |
| grazing-day/4 | half_view | 18.619 | 11.548 | 0.005583 |
| grazing-day/4 | half_caster | 48.416 | 14.645 | 0.006918 |
| grazing-day/4 | half_ready | 34.889 | 16.047 | 0.005583 |
| grazing-day/4 | all_ready | 0.238 | 0.018 | 0.000000 |
| grazing-day/4 | foreground | 19.017 | 9.304 | 0.004618 |
| grazing-motion/4 | pending | 33.815 | 16.278 | 0.003053 |
| grazing-motion/4 | half_view | 29.803 | 13.057 | 0.002161 |
| grazing-motion/4 | half_caster | 51.377 | 20.603 | 0.004325 |
| grazing-motion/4 | half_ready | 27.138 | 12.273 | 0.002161 |
| grazing-motion/4 | all_ready | 0.156 | 0.016 | 0.000000 |
| grazing-motion/4 | foreground | 17.564 | 6.811 | 0.001509 |
| elevated-night16/16 | pending | 6.497 | 5.559 | 0.015646 |
| elevated-night16/16 | half_view | 5.513 | 3.969 | 0.009462 |
| elevated-night16/16 | half_caster | 9.778 | 5.802 | 0.013238 |
| elevated-night16/16 | half_ready | 6.005 | 3.152 | 0.009462 |
| elevated-night16/16 | all_ready | 0.058 | 0.006 | 0.000000 |
| elevated-night16/16 | foreground | 4.176 | 2.028 | 0.000615 |

These times compare finite MC radiance with the complete categorical expectation,
not equal-quality renderers. They are single sequential calls on shared Linux/
i5-1135G7 CPU, batch2048, with original asset preparation outside query timings.
No own profiling/tests overlap normal timing; other host Python activity was seen.
Negative cases remain: backlight view-only is slightly slower; night MC4 grain is
visible and large. Night pending error.075841->.015646 at2.689->5.559s for4->16 draws;
this is neither convergence nor equal-time superiority. The underlying source,
height/model, silhouette, spatial aliasing and transition errors remain.

In the full elevated-night transport check, maximum T difference is5.55e-17 and
contributing receiver S differences are zero in all six states. Raw unlit S may
be0 in the old shortcut and1 in the full-ready identity; direct light is zero there.
The returned transport is consumed with the same receiver normals/light. Small
numeric agreement is not a native culling or full floating unbiasedness certificate.

| State | Complete2048 peak B | Sampled2048 peak B | Sampled512 peak B |
| --- | ---: | ---: | ---: |
| pending | 898,897 | 2,548,845 | 1,523,589 |
| half_ready | 926,183 | 1,374,053 | 877,866 |
| all_ready | 644,588 | 644,708 | 644,588 |

This separate tracemalloc pass spans1,024 actual grazing rays. It includes mixed
parent/child query allocations; source/asset preparation is outside and outputs
remain retained. These are observed Python/NumPy peaks, not total allocations or
renderer/app/GPU budgets. Sampled2048 can consume more transient memory than the
complete expectation; batch512 lowers the peak with additional packet work.
Instrumented times are excluded from normal comparisons. Original C/T/S output
arrays have the same size; frame RSS also includes both controls and resolved data.

Post-union all-ready probes with2/4/16/64 rectangles keep the original child frame
bit-identical and record zero parent packets for24,960 rays. That proves neither
partial-mask cost nor automatically certified readiness. The original sources,
full original input archives and prior controls remain protected. No images are
published. Raw commands/hashes/terminal results, pixels, work/memory/union records
and duplicate redirects: `/tmp/outshine-research-sampled-layer-beaba3860/receipt.json`.

Decision: retain this optional composed consumer and the unchanged complete
expectation as its quality control. It closes the Python finite-stop/ready/external-
caster contract, not world/native admission or a seamless filtered transition.
Next need remains pixel/temporal filtering and native interface/integration/geometry
qualification with original-height/terrain/curvature evidence. Renderer #1129/#1131
stays with Engine; World/cache #1102 with master. No native parallel implementation,
material/night side technique or new cache/export framework was introduced.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
python -B -W error test/experiments/buildings/compare_urban_parents.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-control --samples 16 --jitter-seed 1113
python -B -W error test/experiments/buildings/compare_urban_horizon.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --shadow-runs --paired-exact \
  --surface-samples 4 --sparse-probe --lighting day \
  --output /tmp/outshine-urban-first-hit --distance 50000 --altitude 2
python -B -W error test/experiments/buildings/compare_urban_layers.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --surface-samples 4 \
  --paired-sampled --foreground-fraction 0.75 --distance 50000 --altitude 2000 \
  --output /tmp/outshine-urban-sampled-layers
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
