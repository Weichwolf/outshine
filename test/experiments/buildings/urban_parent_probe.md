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

## Current step: coordinate surface strata within a pixel

`urban_pixel_samples.py` changes only the CDF uniforms consumed by the optional
sampled layer. Existing spatial rays, geometric hypotheses, lighting, opaque stops,
C/T/S composition and independently declared ready domains stay the same. No
neighbouring-pixel smoothing or averaged geometry/material/normal is introduced.
The default still uses the previous independent per-ray uniforms.

For P spatial rays and M surface draws, draw j already has its own interval.
Independently for each pixel and j, a random permutation assigns its P rays to P
subintervals. With rank r and independent jitter v, `u=(j+(r+v)/P)/M`.
For any fixed ray, r is uniform over 0..P-1, so its marginal remains uniform in
its original stratum. Thus its real-arithmetic expectation is unchanged. Covariance
between rays changes: this is not a universal variance or floating-unbiasedness
bound. A finite independent counterexample has variance1/12 versus1/16 before
coupling; dissimilar ray integrands can be worse.

`pixel_surface_packets` generates one pixel-row band and reuses a ray packet.
Its storage follows `O(width*P*M + batch*M)`, independently of image height; it
needs no per-world table or fine-child construction. Each row has its own seeded
stream, so packet boundaries do not alter pixels. Yielded storage is valid only
until next iteration. `render_sampled_layer(..., surface_packets=...)` checks each
used packet's ray-by-draw shape and finite [0,1) domain; short/extra streams fail.
Completely ready camera+caster domains return before requesting any packet.
This does not define world random state or reproject history; crop/seed changes and
continuous camera motion still require temporal qualification.

Measured freeze `e00d0033e` passes158 focused tests in28.253s. Seven new tests cover
per-ray/per-pixel strata, raster order, non-square layouts, seed repeatability,
packet widths1/7/19/71/2048, lazy enormous image height, invalid/short/extra streams,
unchanged default arrays, full-ready bypass, the variance counterexample and an
independent81-box-world pixel mean with one surface draw per spatial ray.
All previous conditional transport, finite-stop, opaque/ready and source controls
remain. The earlier composition/inside-occlusion contract and its87 checked PNGs
are pinned in [004983530](https://github.com/Weichwolf/outshine/blob/004983530/test/experiments/buildings/urban_parent_probe.md).

There are18 terminal-zero original-field runs: four night seeds1113/1729/2111/4093,
two day/backlight seeds1113/1729, and one moved grazing-view seed1113, each with
both methods. Each retains247,731 original masses in616×624,4×4 spatial rays and
M=4. All six states below are recomposed, including independent camera/caster
readiness and an opaque foreground at75% field width. Complete expectations are
freshly checked once per case and reused across that case's other seeds. Every
old seed1113 array/control is bit-identical to the previous frozen delivery.
All-ready output preserves the supplied child baseline in every run.

RMSE below is `sqrt(mean(seed,pixel,RGB squared error))` against the complete
categorical expectation at the same spatial rays. Timings are median query/
composition seconds over those seeds; pattern generation is inside the timed call.
These are not equal-error renderers, convergence bounds or statistical speed claims.
Methods alternate order by seed on shared Linux/i5-1135G7 CPU; small array/report
readbacks and other host activity overlapped some timings. Own tests and memory
instrumentation ran outside normal comparisons. Only projected ROIs inside1280×720
are traced; flat elevated240km/75,894.663844m and grazing50km/2,000m controls do not
prove curved-Earth visibility or native whole-frame/p99 performance.

| Case / seeds | State | Independent RMSE | Pixel RMSE | Independent s | Pixel s |
| --- | --- | ---: | ---: | ---: | ---: |
| night/4 | pending | 0.088139 | 0.072160 | 3.002 | 3.144 |
| night/4 | half_view | 0.062716 | 0.057913 | 2.071 | 2.223 |
| night/4 | half_caster | 0.090338 | 0.074975 | 3.165 | 3.655 |
| night/4 | half_ready | 0.062716 | 0.057913 | 2.161 | 2.610 |
| night/4 | all_ready | 0.000000 | 0.000000 | 0.005 | 0.005 |
| night/4 | foreground | 0.001958 | 0.001836 | 1.347 | 1.572 |
| day/2 | pending | 0.003360 | 0.002879 | 1.315 | 1.259 |
| day/2 | half_view | 0.002363 | 0.002094 | 1.000 | 0.927 |
| day/2 | half_caster | 0.003976 | 0.003775 | 1.469 | 1.427 |
| day/2 | half_ready | 0.002363 | 0.002094 | 1.057 | 1.009 |
| day/2 | all_ready | 0.000000 | 0.000000 | 0.004 | 0.004 |
| day/2 | foreground | 0.001622 | 0.001558 | 0.590 | 0.580 |
| back/2 | pending | 0.011820 | 0.009899 | 1.030 | 1.079 |
| back/2 | half_view | 0.008094 | 0.006889 | 0.800 | 0.797 |
| back/2 | half_caster | 0.011399 | 0.009843 | 1.254 | 1.208 |
| back/2 | half_ready | 0.008094 | 0.006889 | 0.876 | 0.872 |
| back/2 | all_ready | 0.000000 | 0.000000 | 0.004 | 0.004 |
| back/2 | foreground | 0.005317 | 0.004971 | 0.515 | 0.508 |
| motion/1 | pending | 0.003053 | 0.002695 | 16.364 | 17.953 |
| motion/1 | half_view | 0.002161 | 0.001920 | 12.005 | 12.546 |
| motion/1 | half_caster | 0.004325 | 0.004051 | 19.863 | 17.587 |
| motion/1 | half_ready | 0.002161 | 0.001920 | 12.223 | 12.906 |
| motion/1 | all_ready | 0.000000 | 0.000000 | 0.015 | 0.016 |
| motion/1 | foreground | 0.001509 | 0.001404 | 6.950 | 6.772 |

Aggregated nonzero RMSE reductions span3.9-18.1% in these cases. Individual seeds
remain negative: night pending seed1113 grows0.075841->0.088481. Night half-ready
median costs2.161->2.610s while error falls0.062716->0.057913. Visible grain and
underlying source/height/model/silhouette/aliasing errors remain. This does not
establish an equal-time win; moved-pose images are not a temporal sequence.
All150 generated PNGs were checked through nine full-frame/ROI comparison sheets;
known children and foreground survive. Diagnostic images stay local.

All54 paired seed/state queries have identical surface-draw counts and identical
prefix/receiver-shadow work counters. Pending night seed1113 draws57,524 surfaces;
view-cell visits86,986->86,957, unique shaders44,400->44,300, surface-shadow visits
373,193->371,811. Day unique shaders44,279->44,396 and shadow visits131,453->131,953
instead increase. Permutation/layout work and an extra band-to-packet copy of
`8*N*M` bytes are added for N emitted sampling rays. Sharing inputs is not a claim that
every operation shrinks; work and timing details remain in the raw reports.

Separate warm sampler-only tracemalloc at width1280/spatial4/batch2048, consuming
packets without retaining a full table: M4 peaks1,445,496/1,445,800B for heights1/720;
M16 peaks5,574,344/5,574,664B. Existing independent generation at height720 peaks
265,960/855,880B respectively. Whole-image tables would require471,859,200/
1,887,436,800B; none was allocated. This proves bounded extra sampler storage for
these inputs, not a memory improvement over the existing independent sampler.

| Mixed-query state | Independent2048 peak B | Pixel2048 peak B | Pixel512 peak B |
| --- | ---: | ---: | ---: |
| pending | 1,934,769 | 2,096,626 | 1,795,944 |
| half_ready | 2,155,272 | 2,293,297 | 2,155,592 |
| all_ready | 2,155,272 | 2,155,592 | 2,155,592 |

The mixed probe spans one complete pixel-row band of5,312 actual moved/grazing
rays. It includes parent/child query allocations, excludes original source/asset
preparation and retains outputs. Smaller batches change packet work; instrumented
times are not normal comparisons. These observed Python/NumPy peaks exclude
renderer/app/GPU/device memory; they do not establish the A18's available budget.

Decision: keep pixel stratification opt-in (`--pixel-strata`) as a small verified
sampling alternative. Default independent sampling and the complete expectation
remain controls; neither universal nor equal-time superiority is proven. The
largest gaps remain original-height/source/model evidence, spatial aliasing,
continuous temporal/ready transitions and World/device integration (#1102/#1127).
No native implementation, learned model, material/night side technique, dependency
or cache/export framework is added. Full commands/hashes, terminal states, per-seed
errors/work, memory and byte-identical redirects:
`/tmp/outshine-research-pixel-strata-e00d0033e/receipt.json`.

## Earlier controlled alternatives

Full derivations, controls and negative cases are preserved in the pinned method
versions; original inputs and referenced raw artifacts have not been removed.

| Alternative | Result/limit | Detailed method |
| --- | --- | --- |
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
python -B -W error test/experiments/buildings/compare_urban_parents.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-control --samples 16 --jitter-seed 1113
python -B -W error test/experiments/buildings/compare_urban_horizon.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --shadow-runs --paired-exact \
  --surface-samples 4 --sparse-probe --lighting day \
  --output /tmp/outshine-urban-first-hit --distance 50000 --altitude 2
python -B -W error test/experiments/buildings/compare_urban_layers.py \
  --parent "$PARENT_NPZ" --resolved "$RESOLVED_NPZ" --surface-samples 4 \
  --paired-sampled --pixel-strata --foreground-fraction 0.75 --distance 50000 --altitude 2000 \
  --output /tmp/outshine-urban-sampled-layers
```

Set PARENT_NPZ/RESOLVED_NPZ to the saved original inputs pinned in the reports;
do not refresh the changed owner URI. Omit distance/altitude for50km/2m;
`--motion 8000` changes the camera;35000 gives the diagonal case.
Other retained drivers: `compare_urban_horizon.py`, `compare_urban_response.py`,
`compare_urban_poisson.py` and `compare_urban_layer_filter.py`; their pinned method
versions document sampling, independent controls and qualification limits.

## Primary sources read and visually inspected

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
