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

## Current step: pixel-area support filtering

`pixel_polygon_quadrature.py` clips a convex screen support polygon against each
pixel in pixel-local coordinates. Vertical strips split at polygon x vertices;
both vertical endpoints are affine there. Positive tensor Gauss weights integrate
constant, linear and quadratic test fields exactly to floating tolerance. Weights
sum to the actual intersection area, never to one. Even a1e-8-pixel-high strip keeps
its measure. This is a box pixel filter, not a general reconstruction kernel.

`urban_pixel_filter.py` projects12 coarse model-box/flat-ground-shadow vertices,
without child/source/GEOS preparation. The convex hull is only a work domain;
inside it, original categorical transport still retains streets, fixed canonical
centres, source-density variation, height labels and view-conditioned shadows.
The hull provides no real-World bound from missing original-height evidence.
Finite support currently requires model/light in front of the near plane and the
light above the flat model. Terrain, curvature and foreign surfaces remain open.

At every generated ray the complete scene and its unparented background are
requeried, then the **signed difference** is weighted. An independently filtered
opaque baseline belongs to the caller. This diagnostic uses a common pixel-centre
baseline in candidate and controls: its reported RGB error measures the correction,
not the accuracy of that baseline's own pixel filter. No separately averaged C/T/S
or reused opaque boundary sample is introduced.

Initial all-Gauss sampling made an elevated view worse: RGB RMSE.026770->.052472,
with visible lattice spokes. Frozen `a63aa20f2` retains that negative case. Final
`57e2b2a9d` keeps the original4x4 jittered nodes in every fully supported pixel;
only genuinely clipped pixels use area quadrature. Exact four-corner membership,
not a near-one area threshold, selects this fallback. Thus existing interior
sampling quality is preserved. Neither method supplies a global RGB error bound.

**117 focused tests OK (17.943s)** on the shared Linux host. Added700 independent
GEOS polygon/pixel intersections with triangle-moment controls, subpixel slivers,
translation/degeneracy, signed partial correction, exact interior-node retention
and300 box/shadow containment probes for each of sun/point lights. Existing source,
transport, opaque composition and actual3D-clipped BoxField/DDA tests remain.
Ruff/format/function-size and docs checks pass; no native gate is implied.

Six initial clean runs (`a63aa20f2`) and three final clean runs (`57e2b2a9d`) are
terminal0;27 actual RGB PNGs were inspected through full/crop/coverage sheets.
The5 thin initial cases contain zero full pixels: generated nodes/IDs/weights are
bit-identical under the final hybrid. The final far-day arrays/PNGs also exactly
match the earlier case. The unmodified default driver matches all arrays/PNGs of
`f25db1d43`'s day control bit for bit. Do not rerun these proven-equivalent controls
merely to attach a new commit number.

The complete original247,731-mass616x624 field is retained. Only the projected ROI
is traced inside1280x720 diagnostics, not a full-frame/world/device benchmark.
All use target distance240km in flat ENU; ground views have altitude2m, the elevated
view75,894.663844m. Motion is8km. These are mathematical planar counterexamples,
not proof of actual240km visibility from2m over a curved Earth.

| Case / quadrature / resolved side | Uniform RGB RMSE | Filtered RGB RMSE | Uniform coverage L1 px | Filtered coverage L1 px |
| --- | ---: | ---: | ---: | ---: |
| Far day /2 /64 | 0.007108 | 0.000963 | 5.1635 | 0.2496 |
| Far motion /2 /32 | 0.007184 | 0.000652 | 5.1875 | 0.3145 |
| Far night /2 /32 | 0.006351 | 0.007851 | 6.0518 | 0.3659 |
| Far night /4 /64 | 0.005399 | 0.004855 | 6.0269 | 0.2004 |
| Elevated day, hybrid /2 /16 | 0.026770 | 0.027643 | 99.3131 | 101.9898 |

Far day improves RGB error7.4x and coverage error20.7x against64x64 resolved samples;
500 scene rays replace6,240 uniform rays, but measured time grows5.838->6.763s.
Geometry costs about.06s; more rays now actually encounter the long field. Source
transport is still cell-based. Motion also improves the thin silhouette. Elevated
hybrid removes the large spoke regression but remains3.3% worse in RGB than uniform;
coverage L1 is101.99 versus99.31. It is not an equal-quality winner there.

Night remains unqualified. The resolved32->64 RGB difference is.002346 RMS;
parent quadrature2->4 changes.012090 RMS. Order2 versus the same64 reference has
error.008473, order4 .004855 (uniform .005399). Those uncertainties do not prove
convergence or a night-quality advantage. Day reference32->64 differs.000457 RMS,
well below the uniform error.007108. Reference convergence is empirical, not a
formal bound. Snapshot motion does not establish temporal transition stability.

Far-day work:12 projected vertices,120 clipped pixels/125 strips,16,000 named node
output bytes and32,000 named ray/ID/weight bytes. Elevated hybrid retains966 full
pixels/15,456 original rays, plus780 boundary rays;16,236 versus24,960 total rays.
Its ray arrays occupy1,039,104B. These are named arrays, not peak live allocation
or copied-byte bounds. Whole-process RSS367-806MB includes dense resolved controls
and full diagnostic images. The shared parent remains6,264B; no global fine table,
source parsing or GEOS is needed by support generation/quadrature. Interactive
host load remains; timings establish no native speed or16.67ms/p99 admission.

Decision: keep optional `--footprint-order` for image research, default unchanged.
Analytic edge-area weighting is useful, but remaining light/integrand variation and
primary-transport cost need improvement before a general parent substitute. No
Ready/Empty certificate, native port request or replacement admission is produced.
Methods/negative controls and old shadow gains are linked below. Raw commands,
source SHA256 pins, terminal reports, arrays, image reviews and equivalence checks:
`/tmp/outshine-research-pixel-filter-57e2b2a9d/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" \
  --output /tmp/outshine-urban-pixel-filter --shadow-runs --footprint-order 2 \
  --reference-samples 64 --distance 240000 --altitude 2 --lighting day
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
- [Feibush/Levoy/Cook1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section2.4/Figure2, PDF pages3-4: clipping and weighting in the actual pixel footprint. Re-read optically for this step; the coarse support/strip quadrature here is separate.
- [Han et al.2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figures1-2/section3: normal filtering excludes occlusion/geometry simplification/cast shadows.
- [Horizon cloudscapes2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF pages37/81 (slides35/79): coverage/height and alpha1 exit; our finite-error analysis is not attributed to these slides.
- [Sloan/Kautz/Snyder2002](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf), introduction/Figure2/PDF page2: transport/lighting separation, without a general SH/GI guarantee here.
- [Enderton et al.2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf), PDF page2/section3.1/equations1-2: premultiplied depth contributions, no GPU/OIT implementation adopted.
- [Mueller et al.2020](https://tom94.net/data/publications/mueller20neural/mueller20neural.pdf), PDF page1/Figure1/equation1: baseline-plus-difference identity only; no learned model or unbiasedness claim.
