# Parent shading work and aggregated response controls

Owner #1167, native reference #1350/#1355. Keep the original parent geometry,
coverage, source identities, palette, albedo and complete available original rest.
Compare the existing full mixture with its established fixed one/two-component
response fits. No contour, tolerance, camera, light or fitting-grid search.

`fit_building_parent_shading` shares material reads and the existing training/
held-out response basis across both controls and all nodes. Raw complete source
containers are still hashed for strict sidecar identity, with bytes charged.
Only four material arrays are decoded. Solvers run in this offline producer.
The fitter imports no image/mesh stack through incidental array-loading helpers.
Rendering/loading checks receipts and reads weights without fitting. The source
files remain available. The dense weight shape remains unchanged, so neither
sidecars nor active-zero entries imply lower stored/uploaded material bytes.

`compare_building_parent_shading` uses the same four regional views/light and
native opponent as #1355. Original, opaque full parent, fractional full/one/two
share one ROI and16×16 quadrature where the bounded32×32 comparison permits.
Require word-equal T at both rates. Preserve raw/refined fields and actual opened
model/counterexample images. Separate source error from material approximation
and empirical quadrature change. Originals3825/3827 remain required/unrenderable
with unknown geometry, image effect and price. Available native geometry uses
the current-policy reference shader, not a captured native-PBR replay.

The algebraic factorization compares original first hits, proxy first hits,
GGX per first hit and additional layer GGX. It is not additive causal attribution:
geometry/normal/coverage distributions remain coupled. The opaque proxy already
uses the full mixtures, providing a direct control for extra composited layers.

The native cost model records submitted/index-referenced geometry and actual
reference array dtypes/bytes, plus front-facing projected triangle bounding boxes
in full-resolution pixel and2×2 quad coordinates before depth/sample coverage.
Those boxes bound this Double reference projection, not actual GPU invocations.
Logical lobe slots use per-face maxima. Device constants for setup, coverage,
material/shading, composition/sorting and memory remain unmeasured. Never divide
SS16 work by256 or turn CPU GGX counts/box slots into GPU milliseconds. Dense
material bytes, alpha-ordering costs and missing geometry stay separate.

The unchanged sparse fitting method is `building_brdf_fit.md` (SIGGRAPH2016 LTC
review and finite relative-response objective). One-component global validity
cannot follow from a few favourable day views; retain its worst fixed held-out
material point. [Han et al., SIGGRAPH2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf),
p.1/Figs.1–2 and p.2/scope/equation1 optically inspected, motivates the separate
synthetic normal-map counter: average responses of two fixed normals, then compare
with shading their normalized mean. No normal averaging is introduced by our fits.
No SH/vMF implementation or convolution/shadow/geometry validity is imported.

Use project `.venv`, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_parent_shading.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --seed-cell 33 --cell-count 4 --output <fresh-system-temp-producer>
python -B -W error test/experiments/render/compare_building_parent_shading.py \
  --input <producer>/parents --fits <producer>/fits \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --controls test/outshine/data/render/building-full-view/native-cells/manifest.json \
  --output <fresh-system-temp-view>
```

For strict opaque-blocked work run `diagnose_building_layer_work.py` with the
same comparator arguments. It preserves the actual shade/composition path,
checks per-record lobe counts against the shader, and verifies exact per-sample
C/T after dropping only later records behind alpha exactly1 in the existing
depth/face-tie order. Its extra sorting/copies/second composition are diagnostic
costs; zero-effect counts do not establish native work or a profitable early pass.

The composed producer measures archive/index, unchanged scalar parent creation
and subsequent fit states in one sequential run; stage RSS peaks are cumulative.
For existing cached parents use `fit_building_parent_shading.py --input <parents>
--output <fresh-temp-fits>`. No source acquisition/new native asset format,
binding/default switch or global cache architecture. Procedure, findings, exact
counterexamples, bytes/work and terminal checks belong in the PR. The eventual
limited native comparison needs agreed bindings/ownership and actual device
times; Python does not admit whole scenes, general highlights/normals or p99.
