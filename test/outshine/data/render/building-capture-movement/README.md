# Native building capture movement input

Refs #844. `native-cell.json.gz` is a self-contained native geometry input for
Python movement research, not a rendered-image oracle or a movement admission.
Writer and schema: [native_building_capture_inputs.md](../../../../experiments/render/native_building_capture_inputs.md).

```sh
python3 test/experiments/render/check_building_capture_inputs.py \
  --input test/outshine/data/render/building-capture-movement/native-cell.json.gz
python3 -m unittest discover -s test/experiments/render \
  -p test_building_capture_inputs.py
```

The archived cell 64 of parent
`604260a705f162b6c92ca2038a3d8ac56242ecd646d53ebe5a0531c28c9f25d4`
projects 131 of its 2001 original sources: 121 visible-face, 9 patch and 1 whole-mesh
emissions. It retains 2951 complete selected-surface triangles, 18081 independently
meshed Fine triangles and 691 actual captured triangles. Including duplicate
control streams, 25365 triangles are serialized. Counts describe this input only.

Sources lie approximately 5.1–5.3 km from the recorded eye. This is a distant cohort,
not a near-field, disocclusion or thin-feature stress proof. Opacity and runtime
culling remain unqualified. Source/part/face identity, projection and pixel error
are retained; unknown whole-mesh face identities remain null.

`provenance` records native commit `4e5daa4cf6702eb34c3855fb41df05aae6d292d1`,
capture-record commit `f1e1a3d6e`, source-file/binary/library/report hashes and original
asset-package identities/hashes/CRC. The original packages are not included;
replaying the native cooker requires those packages. The expanded triangles and
attributes suffice for independent Python checks. Geometry is generated from
OSM-based building data; this file is neither source OSM nor an imagery texture.

Only elapsed observer timing was removed. Gzip timestamp is 0 with no filename.
Compressed SHA256:
`e29429e2fa670c2a5ce4b2357ee274c31fc6be7764866737b57933fb863a6c99`.
