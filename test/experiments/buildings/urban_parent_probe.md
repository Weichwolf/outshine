# Direct urban parent probe (#1113)

Work in progress under #1083. This is a tested experiment, not an admitted parent
renderer or complete-world/native/device result.

The source compiler clips coarse regions directly against the existing urban/open-
space evidence. It generates zero child cells. Continuous circle/diagonal setback
variants estimate allowable centre area; their midpoint is a candidate rather than
a fitted place factor or certified discrete bound. Parent and resolved generators
share the height CDF, vacancy, pitch and footprint recipe in `urban_recipe.py`.
Parents retain unlit area measures for six surface orientations and the inferred
height distribution. Unknown original heights are not certified maxima/occluders.

The first consumer integrates roof sheets and statistical wall extinction through
four height intervals. An optional swept-phase mixture preserves some clear grid
corridors. It is relightable, but loses conditional height/visibility correlations.
Both paths use the same direct GGX/diffuse lighting and unoccluded ambient term;
this controlled software reference does not claim native material/GI equivalence.
The existing DDA supplies an independent resolved-lot control with explicit gaps,
height clipping and a separate shadow-ray minimum. It is never a parent prerequisite.

At the pinned owner, raw area overestimates resolved roof area by 14.7%; continuous
setback estimates give +2.9% / -1.7%. The first 1280×720 day preflight used 240 km
horizontal offset / 80 km vertical offset (252.98 km target distance) and 16×16
samples in the common 62×23-pixel region:

| Model | Coverage sum, px | Coverage L1, px | Linear RGB region RMSE |
| --- | ---: | ---: | ---: |
| Resolved control | 569.44 | — | — |
| Raw statistical volume | 651.45 | 92.49 | 0.03244 |
| Setback midpoint volume | 599.45 | 58.11 | 0.03734 |
| Swept-phase mixture | 559.23 | 51.66 | 0.04310 |

These are preliminary, uncommitted preflight images; current camera inputs declare
actual target distance so the required 240-km comparisons stay at that distance.
Improved coverage does not
improve lighting here; the consumer remains red. Reference sampling convergence,
movement/back/night/ascent/transition cases, fair work/peak controls and conditional
height/shadow treatment remain required. Local images stay in system temp.

Frozen 2e2073540 subsequently completed six runs / 24 actual images at a true
240-km target distance: day 16/32 per-axis sampling, backlight, point-lit night,
8-km lateral movement and a 10-km camera altitude. Both full comparison sheets
were inspected. Regular 16→32 reference sampling still differs by 0.01971 linear
RGB RMSE. Stratified sampling at frozen 67a423bae adds three runs / 12 inspected
images: 16→32 reference difference 0.00757, two independent 32-sample seeds 0.00405.
Sampling uncertainty remains explicit; the model error is larger (day seed1113:
raw/midpoint/phase RGB RMSE 0.03231/0.03717/0.04277). The current volume therefore
remains unqualified; improved coverage has not recovered its lost light response.
Conditional height/visibility/shadow correlations motivate the next control below;
no source-specific brightness multiplier is fitted.

Frozen fdfc50d1c adds a categorical-height expectation control. A cell has exactly
one height or is vacant. Contributions from its height hypotheses are mutually
exclusive. Shadow queries skip the known hit cell and condition other cells on
earlier primary-ray misses: a missed footprint excludes heights reaching that ray.
The shared packet DDA supplies the traversal; the parent still stores only area
statistics, not a generated child field. This control **traverses recipe cells at
query time** and is not the intended coarse fast renderer.

Exhaustive enumeration of 81 independent four-cell scenes matches RGB and coverage
for vertical/day/back sun and point light against independent box intersections.
47 focused tests pass; the four earlier day32 crops remain bit-identical after the
DDA extraction. Six additional frozen runs / 30 actual 1280×720 images completed
with exit 0; the complete comparison sheet was inspected. All use true 240-km
target distance and seed1113, with no stored-product changes between views/lights.

| Case / samples per axis | Midpoint RGB RMSE | Conditional RGB RMSE | Python time ratio |
| --- | ---: | ---: | ---: |
| Day / 32 | 0.03717 | 0.01272 | 9.89× |
| Back / 16 | 0.02251 | 0.00980 | 5.26× |
| Night / 16 | 0.21414 | 0.07198 | 22.27× |
| Lateral movement / 16 | 0.04193 | 0.01387 | 9.85× |
| Low altitude / 16 | 0.01023 | 0.00403 | 71.01× |

Lighting improves, but boundaries remain too regular and the inferred density loses
local structure. Day32 coverage is 657.90 versus 633.79 resolved pixels; coverage
L1 is 56.83 pixels. Conditional 16→32 RGB difference is 0.00561, so sampling error
is material and these numbers are not a certified error bound. Day32 needs 7.15M
primary cell visits, 34.24M shadow visits and 160.69M history comparisons; low view
needs 222.23M history comparisons. Retained history peaks at 393,216 / 2,408,448 B
for day / low view, **not** total temporary/process peak or copied bytes. Times are
Python shared-load controls, not native device measurements. No cost/quality winner
or parent admission: use this as a visibility reference while seeking a cheaper
filtered transfer that preserves height/view/light correlations and pixel coverage.

A separate generation-growth check compiles the existing z5/28/12 parent directly:
one source record, 256 regions, zero child cells and 6,264 array bytes. Enumerating
the full 32-m rectangular candidate grid would involve 1,531,626,496 centres.
The one cold Python source compilation took 0.101 s / 39,878,656 B raw process RSS
under shared load. This coarse source omits finer owner exclusions: it proves
bounded direct generation, not equal-quality/global coverage or device performance.
All seven actual resolved owner arrays remain byte-identical after sharing the recipe.

Reproduce without private input dependencies, with the existing `.venv`, fresh temp
output and `PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/buildings/compare_urban_parents.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-parent-study --samples 16
```

The parent is saved before the offline control is built. `--resolved` reuses an
existing resolved field solely for that control; `--parent` consumes a saved parent
without recompiling sources. `--motion`, `--altitude` and `--lighting` vary state
without changing the stored product. No camera-specific assets or baked lighting.

Primary sources read and visually inspected:

- [Feibush/Levoy/Cook 1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), section 2.4 / Figure 2: filter polygon edges and texture footprint together.
- [Han et al. 2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf), Figure 1–2 / section 3: preserve normal distributions for BRDF filtering; its scope explicitly excludes occlusion, geometry simplification and cast shadows.
- [Horizon cloudscapes 2015](../../../doc/references/clouds/siggraph/2015-horizon-volumetric-cloudscapes.pdf), PDF page 37 / slide 35: separate coverage and height-density modeling. This probe copies no cloud implementation and assumes no cloud-to-building equivalence.
