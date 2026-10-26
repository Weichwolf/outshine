# Native unprojected cell controls

Owner #1350; consumer #1167. Original archive, camera, projection, enriched source
plans and native mesher. No acquisition, shared-cache writes or product changes.
`--full-cell N` disables surface projection and exports the normal 3D selection
for that original cell. It does not force Fine, change the error budget or claim
identity with archived geometry that does not exist for that cell.

Fixed inputs: `test/outshine/data/render/building-full-view/native-cells/manifest.json`.
The manifest identifies the source archive, unchanged native product tree and each
compressed control. Cell33 references the existing `../cell.json.gz`; no duplicate.

| Cell | Original sources | Native triangles | Unsupported sources |
|---|---:|---:|---|
|25|508|12327|0|
|33|576|14169|0|
|34|474|11980|0|
|41|335|8413|3825,3827|

Totals:508+576+474+335=1893sources;12327+14169+11980+8413=46889triangles.
These are complete unprojected native controls, not visible GPU work or a device
budget. Two source ordinals fail with `footprint cannot form a supported building
mass`. Their IDs, original forms and explicit failed mesh calls remain present.
They must not become zero-cost valid geometry or silently disappear from a
candidate's original remainder. Normal/material/UV streams and source index ranges
are retained. No opacity, shadow, contact or near-detail qualification follows.

The existing `--full-view` continues its strict geometry/metadata comparison with
the original archived cell33. The new control33 is equal to that replay; the other
cells are explicitly producer controls. New fixed files omit elapsed times;
the archived cell33 stays unchanged. Additional compressed inputs total2969439bytes.

Build the standard CMake target and run its focused boundary checks:

```sh
cmake --build "$build" --target test_experiments_render_BuildingCellControl --parallel
ctest --test-dir "$build" --output-on-failure \
  -R '^experiments/render/(BuildingCellControl|NativeBuildingCellExport)$'
```

Extract the existing original archive to a fresh system-Temp directory. The input
is `<parent>/input.json`; use the parent hash from the manifest. Never overwrite a
previous output. Export each needed original cell:

```sh
"$build/test_harness_building_selection_export" "$input" "$output" --full-cell 25
```

With the research environment and its existing NumPy dependency, independently
check a plain or gzip control against the original index and source IDs:

```sh
python -B test/experiments/render/check_building_cell_control.py \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --input test/outshine/data/render/building-full-view/native-cells/25.json.gz
```

The checker requires complete original source order/IDs, individually identified
Fine/Shell calls, exact geometry range coverage, finite complete attributes and
explicit zero-output failures. Projected/anonymous aggregated controls are outside
this reference contract. Source decoding reuses the existing independent Python
archive/index reader. Native boundary tests also reject malformed/duplicate/missing
inputs, damaged artifacts, invalid cell arguments and existing output paths.
