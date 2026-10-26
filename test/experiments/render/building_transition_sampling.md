# Complementary ownership before building material evaluation

Owner #1167, following #1315's paid complete C/T blend and #1337's physical
camera probe. Compare complete stored finer/coarser source-tree cuts without
regenerating products or changing their source/material/geometry requirements.

The camera crosses the existing root threshold physically, moving outward
at200m/s for one second, with nine poses and unchanged orientation. A declared
linear transition covers this200m band; it is a diagnostic choice, not a new
runtime fade/validity contract. Actual hard-cut selection must match one of the
two loaded Ready products at every pose. Both cuts cover exactly the same whole
sources, including paid original remainders. One fixed screen ROI contains the
complete geometry over the whole path. Defaults retain1280×720 and SS16.

Controls are the complete original, both full fractional endpoints, their
complete C/T mix, the independently faded negative control and actual hard cut.
Two candidates choose a whole cut before material interpolation/GGX:

- Pixel ownership assigns all256 quadrature samples of a pixel to the same cut.
  It is the relevant candidate for potentially skipping an entire pixel's
  material work. It still needs native fragment/branch/cost qualification.
- Sample ownership assigns individual quadrature samples. Its lower CPU GGX
  count proves savings in this sample-rate reference only. With pixel-rate
  native shading, both fragments can still require shading in one pixel;
  sample coverage alone is no proof of saved native pixel-shader work.

Ranks use explicit PCG64 seed0, once per fixed ROI. Pixel ranks are a uniform
permutation across that ROI; sample ranks are a permutation inside each pixel.
The two endpoint masks are exact complements, shared by every group and the
entire original remainder. They stay fixed over movement/lights; only their
threshold changes. No seed/rate sweep, material/normal fit or temporal filter.
This fixed-ROI Python pattern is no native seed/global-viewport contract.

Rounded rank counts define represented weights, reported separately from the
requested transition weight. Under a uniform random permutation, each owned
measure has the corresponding inclusion probability. Its ensemble mean is the
complete C/T mix at that represented weight; a finite fixed pattern need not
equal the mean, be temporally smooth or improve the original image.

Masked `transfers` returns partial measures: both C and T are zero at unowned
samples before box integration. Adding complementary pieces yields the selected
complete measure. These pieces are not independently transparent layers.
Two fully opaque endpoints remain opaque. Pixel ownership chooses one complete
pixel endpoint, so its T cannot exceed the endpoint pixel maximum; sample
ownership can exceed that pixel-average maximum through finite sampling.

Masking happens after CPU first-hit rasterization and before point/material/GGX
work. Interior transition poses still pay both endpoint raster passes. An empty
owned endpoint is skipped. Both Ready sets, retained originals, constructed
control meshes, rank/mask arrays and work remain counted separately. No native
geometry reduction, frame budget, allocation/bandwidth total or residency pass.
Three previous direct/grazing/ambient-only light controls use the same path;
ambient-only is not night/local-light/sky/shadow simulation. Fixed-screen error
changes use the existing independently tested diagnostic, not motion reprojection.

Use `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/compare_building_transition_sampling.py \
  --input /tmp/outshine-transition-products --cells 33 44 \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --native-cell 33 \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --frames 9 --duration 1 --speed 200 \
  --output /tmp/outshine-transition-sampling
```

Input construction remains `building_transition.md`; outputs stay explicit Temp.
Retain first/midpoint/last and candidates worse than the exact mix, with all
per-pose metrics. Actual image/work findings belong in the PR. Camera-/Caster-
Ready, missing-product publication, larger populations, physical/world transitions,
Metal/Vulkan and1280×720/60-p99 remain separate qualifications.

SIGGRAPH first: [GPU-Driven Rendering Pipelines,2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf)
checked for LOD/transition guidance; PDFp.54's separate culling/LOD/draw costs
optically viewed. It supplies no complementary-cut proof used here.
[Enderton et al.,2011](../../../doc/references/vegetation/tvcg/2011-stochastic-transparency.pdf),
pp.1–4, PDFp.3's stratification equation and p.4/Figs.2–3 optically checked.
That paper's independent-fragment OIT estimator differs from the deliberately
correlated choice between two alternative whole cuts here. No alpha-correction,
accumulation pass, shadow algorithm or paper-performance guarantee is imported.
