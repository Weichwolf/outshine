# Original building source planes

Owner/decision: #1570/#1127. Reuses the four controls in
`native_building_cell_controls.md`; no new source or fitted material model.

```sh
cmake --build "$build" --target test_experiments_render_NativeBuildingSourcePlanes --parallel
"$build/test_experiments_render_NativeBuildingSourcePlanes" \
  "$original/input.json" "$request" "$new_output"
```

`original` is the protected `original-inputs.tar.gz` extraction. `request` contains
`cells`, `east`, `north`, `up`, `origin`, `pixels`, `source_tests`, `directions`.
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

Outputs: `atlas.bin` with provenance `native-source-planes-impostor-1`,
`source-result.json`, and view-major linear RGBA material fields `base.linear`.
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
