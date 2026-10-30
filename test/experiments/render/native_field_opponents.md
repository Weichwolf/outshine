# Registered native field opponents

The existing material-field renderer exports whole originals, the unchanged stored
512² source field, analytic terrain, terrain plus an opening, and both direct
original/outer unions. Capture, raster, clips and sample locations are identical.
`lighting_camera` independently selects the PBR viewer when `field_lighting` is
`viewer`; it does not move the captured geometry. Effective cameras are reported.

```sh
cmake --build build --target test_experiments_render_NativeBuildingImpostors --parallel
python test/experiments/render/export_native_field_opponents.py \
  --binary build/test_experiments_render_NativeBuildingImpostors \
  --output "$system_temp/new-native-field-opponents"
```

Python 3.10+ and NumPy are required. Output must be new and outside the checkout.
All 19 original operands retain their hashes. Opponent coordinates are declared
Float32 inputs from Research's finite controls, not new generator recipes. Their
vertex colours multiply existing native role-0 materials; no terrain appearance
or World geometry is being approved.

Six fresh native processes each render viewer, moved viewer, repeat and changed
light. Two further processes must reject a malformed PBR basis and a shifted
stored-field capture. Clear is premultiplied transparent black; opaque alpha
matches positive Reverse-Z Depth32F. Observer/light changes keep depth unchanged.

`manifest.json` binds controls, original provenance, native source/binary hashes,
effective cameras and every array. `samples.npz` holds lossless Float32 C/T and
depth arrays. Identical buffers share one physical array; each logical name has an
`array` reference, shape, dtype and raw-byte SHA. Read with `allow_pickle=False`.
Commands, terminal exits and native reports remain beside the arrays.
New manifests include executed request JSONs, their byte hashes/exit codes and
the exporter source hash. The shared historical fixture keeps these saved inputs
in `execution-inputs.json`, bound to its unchanged manifest. Its versioned exporter
source is identified; the historical runtime script hash was not recorded.
Repeat failures are recorded in `reference_failures`; complete diagnostics are
saved before the exporter exits unsuccessfully. A later pass does not erase them.

Comparisons report native stored/original mask, depth and PBR differences, plus
point composition against directly rendered unions. They report observations;
they do not manufacture a continuous error interval or general pose/World
admission. Ties, unknown coverage, changed bindings and unsupported views still
require the existing parent. Readback/prefix generation is diagnostic work, not
a frame-time or device-memory benchmark. References and ownership: #1797 / #1805.

Prepared original or stored sources can share one native depth/colour pass through
`additional_sources`, with each original `directory` pointing to an existing
export root. All opaque sources share the declared capture raster and PBR view;
filtering follows their joint raster. Blend/transmission inputs retain the
separate existing path. Native equal-depth draw order is not uncertain-interval
admission. Reproduce against an explicit preserved source export:

```sh
python test/experiments/render/compare_native_resident_unions.py \
  --binary build/test_experiments_render_NativeBuildingImpostors \
  --reference "$system_temp/previous-native-field-opponents" \
  --output "$system_temp/new-native-resident-unions"
```
