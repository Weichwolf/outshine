# Original building source planes

Owner/decision: #1570/#1127. Reuses the four controls in
`native_building_cell_controls.md`; no new source or fitted material model.

```sh
cmake --build "$build" --target test_experiments_render_NativeBuildingSourcePlanes --parallel
"$build/test_experiments_render_NativeBuildingSourcePlanes" \
  "$original/input.json" \
  test/outshine/data/render/building-full-view/source-plane-request.json "$new_output"
```

`original` is the protected `original-inputs.tar.gz` extraction. The fixed request is
versioned beside it; other requests contain
`parent`, `cells`, `east`, `north`, `up`, `origin`, `pixels`, `source_tests`, `directions`.
Axes are orthonormal ECEF directions; origin is relative to the original tile's
ECEF anchor. Directions use the resulting East/Up/South frame. Existing Python
tree metadata supplies `east=basis[0]`, `north=basis[1]`, `up=basis[2]`,
`origin=basis.T @ anchor`; the control's fixed nine directions remain unchanged.

Compact source headers determine selection, bounds and the shared projection
before form IO. Only selected cells bind lazy, compatibility-checked form blocks.
The existing source hierarchy traces pixel centres; native plane patches retain
normals, paint, UVs and material metres. There is no fine/shell mesher call or
control-geometry input. Each view builds only its own indexed patches and uses
one persistent native capture context. Unsupported original ordinals remain in
`remainder`; omission is not occlusion or a zero-cost native fallback.

Optional request `view` selects one catalogue index before form IO and capture.
Shared bounds/padding are still planned from the full direction catalogue, so
the selected capture has exactly the same projection as its full-atlas view.
The output contains only that view at local index zero; `selected_view` records
its catalogue identity. Invalid selection fails without reading source forms.
This provides individual source-view production; automatic asset-miss scheduling
and the general World producer remain separate integration work.
The source loader and persistent capture context are shared by both native
diagnostics through `BuildingSourceCapture`; there is no second source decoder.
Its owner retains lazy inputs until capture ends and reports descriptor/index/
form bytes plus file-read and decode time. Cached-world mode can call it directly
on a selected product miss; see `native_building_impostors.md`.

Outputs: `atlas.bin` with provenance `native-source-planes-impostor-1`,
`source-result.json`, and view-major linear RGBA material fields `base.linear`.
Optional request `export_geometry: true` writes the same generated indexed patches
under `geometry/<view>/`, including their material roles and complete attributes.
Conversion, diagnostic export and capture have separate timings; export does not
change the atlas. This is a representation control, not additional asset production.
These fields are material captures, not final lit Places. Timings distinguish
source work from native conversion/capture; the first capture includes startup.
Package/index IO, generated vertices/triangles and unresolved sources are explicit.

For the existing native cached-world consumer, provide the encoded atlas digest
and `atlas_provenance` in its `input.json`, retain the fixed control queries and
use the source report's bounds. No original part arrays are needed in that mode.
A fresh hit process must work with `atlas.bin` absent and report zero source
reads/decodes/writes with unchanged raw images. Preserve original references.

This qualifies source production and selective cached consumption. Image energy,
coverage, view reuse, parent/child transitions, the general World producer and
device frame budgets require their own evidence. No smaller SSE bound follows.

To compare the patches directly, place the unchanged control queries, bounds and
catalogue in the output directory's `input.json`, then run
`test_experiments_render_NativeBuildingImpostors "$output" --source-mesh`.
It reads only the selected view's generated arrays and writes `mesh-result.json`
plus native raw images. Compare against the existing original SS16 controls;
retain failed quality/cost cases. Raster-size experiments keep cameras, light,
source set and the presentation profile fixed. Fewer triangles alone admit no LOD.

## Rejected whole-contour sampling

```sh
python test/experiments/render/compare_source_contours.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --request test/outshine/data/render/building-full-view/source-plane-request.json \
  --source-report "$output/source-result.json" --output "$new_comparison"
```

The experiment selects the nearest contour at each ray before triangulation.
On the first two fixed directions, 16² needs 71–75 triangles but retains only
0.6–1.7% of the complete projected union area. At 64², 1,136–1,163 triangles retain
14–15%. It is rejected: small faces between rays disappear, not just hidden faces.
These are geometric area ratios, not native material/image or SSE results.
The existing Python roof domains also have three additional unsupported sources
compared with the native producer; all five source remainders remain explicit.
Sub-ULP projected slivers are reported as collapsed contours, never as occlusion.
Do not port this sampler as a complete far representation or discard the parent.
