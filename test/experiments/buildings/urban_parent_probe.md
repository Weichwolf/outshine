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
