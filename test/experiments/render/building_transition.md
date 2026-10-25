# Root LOD transition on the same source tree

Owner #1167; source compiler/node format from #1312 remains unchanged. The test
driver requests cuts immediately either side of its eight-pixel radial root
threshold. For root radius R and focal length f, the threshold distance is
R+2Rf/8. Selection eyes differ by±0.01%; both products render at exactly the same
middle eye. The two existing directions and source cells are reused, not swept.
Only required source-tree parents/children are built; no fine mesh is an input.

The comparison keeps every source in both cuts, including actual original rests.
It measures opaque and fractional complete building C/T layers with unchanged
materials,1280×720 and a16×16 regular grid. A32×32 refinement reports empirical
sampling changes, not an integral/model bound. Opaque GEOS coverage is separate.
The native capture and the labelled analytic source reference remain distinct.

A complete-layer midpoint interpolates premultiplied C and T together. Independent
opacity fading followed by layer-over is a negative control: even where both
endpoints are opaque, half-fading each lets one quarter of the background through.
This is a known same-source overlap, not independent mask coverage. The negative
control operates on filtered layer transfers, not a claimed native fragment test.

Convex interpolation commutes with composition over the same fixed pixel background.
It cannot create T outside the endpoint interval and keeps common opaque pixels
opaque. It does not correct either endpoint's geometry, normal, coverage or lighting
error. It can blur/ghost unlike shapes; a negative fade may accidentally be closer
to the original where an endpoint already overfills space. No quality threshold
is chosen to admit the current parents.

Both endpoints must already be independently camera-ready for the same source,
view/domain and opaque stop. Data existence does not prove Ready. A pending product
does not authorize fading out the old one. Camera readiness never grants caster
readiness (#1102/#1113); function/collision/shadow products stay independent.
This experiment supplies no scheduler, cache state machine or native shader binding.

The loader counts both ready node sets and diagnostic assembly copies. The union
of actual original-rest sources/triangles/decoded attributes is paid once for
residency; this complete-layer reference still rasterizes/shades the remainder in
each endpoint. GGX/raster counters expose that duplicated work. CPU ROI transfer
arrays do not define a native render-target format or prove an app-memory budget.
Both endpoints would need current-view evaluation during motion/relighting; these
fixed-eye probes do not certify a moving transition or a16.67ms/p99 frame.

Use `.venv` Python, one OpenBLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/build_building_transition.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 44 --output /tmp/outshine-transition-products
python -B -W error test/experiments/render/compare_building_transition.py \
  --input /tmp/outshine-transition-products \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --cells 33 44 --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --native-cell 33 --output /tmp/outshine-transition-views
python -B -W error -m unittest test_building_transition
```

SIGGRAPH-first review: [Continuous Height-Field LOD,1996](../../../doc/references/transport/siggraph/1996-continuous-height-field-lod.pdf),
p.2 optically inspected: small view changes should not cause large workload
changes; its regular-grid repolygonalization is not this topology-changing
building aggregation. [Porter/Duff,1984](https://keithp.com/~keithp/porterduff/p253-porter.pdf),
p.3 previously optically inspected, distinguishes unknown mask overlap. Complete
source-family interpolation here relies on an explicit shared domain; it does not
derive physical correctness for the existing approximate parent layers.
