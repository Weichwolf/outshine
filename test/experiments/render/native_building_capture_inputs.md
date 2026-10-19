# Actual native capture inputs for movement research

Refs #844. Extends `native_building_selection.md`; original index, form and view
packages are copied by its existing read-only exporter. Build
`export_building_selection.cpp` together with `building_capture_inputs.cpp`
using the complete current generator compilation flags and native libraries.

```sh
"$native_selection_exporter" "$original_input_manifest" \
  "$fresh_system_temp_result" --captures "$explicit_triangle_limit"
```

This mode restores the archived `RequestedCell` and selected eye/projection.
It does not substitute the whole-parent comparison used by the ordinary mode.
It requires at least one accepted native projection; no capture, failed replay
or exhausted export budget returns success. Default and `--surfaces` modes remain.
The limit counts reference plus captured triangles stored in this diagnostic,
not an engine geometry allowance or solver/frame budget.

Each projection records the exact eye offset from the parent ECEF anchor,
focal pixels, allowed pixel error and actual cube-face resolution. Source slots
refer to `members`; original source indices refer to the unchanged root `sources`.
Null aggregate-source identities remain null. No proximity matching invents IDs.

`reference` contains the complete native input surface for each captured source.
`captured` contains its actual selected triangle geometry. Stream vertices retain
native Float32 positions/UV and packed normal words; triangle winding is explicit.
Parts and FaceAt indices are retained for face/patch emission. Whole-mesh fallback
retains source/stream/triangle identity, with unknown Part/Face explicitly null.
These rows describe the selected native input surface. Additionally,
`native_fine_reference` independently meshes every source at Fine with recessed
openings enabled; it is not replaced by a cheaper selected-level reference.
An unavailable or over-budget complete reference fails the export. Comparing
those levels remains the research consumer's work.

The optional native capture record supplies actual visibility masks, patch
samples/corners, fallback flags and the chosen emission mode. The observer does
not reimplement visibility or the cost decision. Individual native faces/patches
are replayed and their expanded vertex streams must match the native emission.
Whole-source replay must also match every actual captured vertex, index, colour
and material-metre byte. A mismatch fails the export; there is no tolerance or
nearest-face ownership fallback. Indexed sharing can differ between chunks;
triangle order and vertex attributes cannot.
Reference/replay meshing and JSON allocation are observer costs, not runtime
capture timings or memory budgets.

`native_reference` and `native_capture` retain separate control streams for an
independent reader. `check_building_capture_inputs.py --input <json-or-json.gz>`
checks their expanded attributes, source bindings and patch/face ownership without
trusting native success flags or recomputing the visibility selection.

Opacity and runtime culling remain explicitly unqualified. Geometry-only opaque
controls must name that assumption and cannot admit these sources globally as
occluders. The data do not prove nonzero translation reuse, narrower LOD bounds,
shadow/interaction completeness, runtime images or device budgets. Follow-up
poses and their proof belong to the research consumer. No cache writes or network
acquisition are performed by this diagnostic.
