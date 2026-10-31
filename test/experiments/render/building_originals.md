# Complete prepared building originals

`NativeBuildingOriginals` calls the production `BuildingSourceView::Originals`
through the existing archive decoder. It exports indexed geometry without a GPU,
private renderer, source cache or replacement mesher. JSON limits select source,
face and triangle-attempt admission; errors still write a diagnostic report.

```sh
cmake --build "$native_build" --target test_experiments_render_NativeBuildingOriginals --parallel
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings:test/experiments/transport \
python test/experiments/render/compare_building_originals.py \
  --binary "$native_build/test_experiments_render_NativeBuildingOriginals" \
  --output "$fresh_system_temp_output"
```

The versioned `building-full-view` archive and separately delivered native-cell
meshes supply the inputs. The manifest determines complete and unsupported cells.
The archived opacity assumption remains in provenance; this is not a transparency
qualification.
Each complete cell and their union must retain every original oriented triangle,
position, normal, UV set and colour. Cyclic corner rotation preserves winding;
signed zero is canonicalized. No geometric or image tolerance is increased.

Archived octahedral words are decoded with the CPU's Float32 operations and then
the supplied Float64 frame dot products. The existing Float64 analytical normal
reference remains unchanged. Facade/Metres material labels are equivalent only
where the production facade shader does not read metre coordinates: negative
surface codes, other wall variants and the glazing branch. Roof material identity
and all vertex attributes remain part of the comparison.

The ten native cases cover three complete cells plus their union, unsupported
whole-group refusal, cancellation, three pre-IO quota refusals and exhaustion
during meshing. Refusal must export no partial geometry; metadata-only refusal
must load no forms. Successful calls load each required form package once.

`generation_ms` includes lazy form IO/decode and geometry construction, excluding
geometry export and descriptor setup. `logical_mesh_bytes` counts active arrays,
not capacity, roof scratch, allocator or process/GPU peaks. Reports and generated
geometry stay in system temp; normal tests do not generate or replace oracles.
This does not qualify cache restart, GPU capture, pose reuse, World admission,
screen-space error or the shared frame/memory budget.

## Existing material-reference cohort

`test/outshine/data/render/building-depth-samples/source-binding.json` binds the
1558-source/38476-triangle material reference to its existing prepared archive,
native cells, recorded producer frame and original three material parts (#1894).
This frame has zero relative origin and the recorded producer's tangent axes;
`building-full-view/source-plane-request.json` uses a different frame.

Read the selected cells through `native_building_cell_reference.load_cells`
with the supplied east/north/up basis and zero anchor. Replace `vertex_normal`
with `compare_building_originals.native_normals(source["normal"], basis)` before
`native_building_cell_reference.native_parts`. All 18 resulting attribute/index
files must match the existing original manifest, without tolerance. The existing
`input.json` fixes part/material order; per-part triangle-binding hashes fix the
actual prepared-source ordering. Unknown source-part entries remain unknown.
The current `BuildingSourceView::Originals` groups metre materials differently;
equal total counts do not identify these historical material-reference parts.
This diagnostic binding supplies no live World revision or normal World admission.
