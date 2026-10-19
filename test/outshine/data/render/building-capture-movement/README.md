# Native building capture movement inputs

Refs #844. Self-contained native geometry for Python movement research, not
rendered-image oracles or movement admissions. Writer/schema:
[native_building_capture_inputs.md](../../../../experiments/render/native_building_capture_inputs.md).

```sh
python3 test/experiments/render/check_building_capture_inputs.py \
  --input test/outshine/data/render/building-capture-movement/near-cell.json.gz
python3 -m unittest discover -s test/experiments/render \
  -p test_building_capture_inputs.py
```

| Input | Original parent/cell | Sources | Fine / selected / captured triangles | Fine vertex range |
| --- | --- | ---: | ---: | ---: |
| `native-cell.json.gz` | `604260a705f1` / 64 | 131 | 18081 / 2951 / 691 | about 5.1–5.3 km |
| `near-cell.json.gz` | `e2e2b448f9a8` / 21 | 26 | 20824 / 14754 / 14165 | about 264–555 m |

Ranges are distances of Fine vertices from the recorded eye, not exact
point-to-triangle distances or continuous validity bounds. The native archived
pose/cell are unchanged. The first input has 121 visible-face, 9 patch and 1
whole-mesh emissions; the nearer input has 22 visible-face and 4 whole-mesh.
Including duplicate controls, 25365 and 78662 triangles respectively are stored.
Counts describe these inputs only; neither proves near-field or disocclusion
correctness, a nonzero reuse radius, narrower LOD bounds or device budgets.
Opacity/runtime culling remain unqualified; unknown whole-mesh Part/Face IDs null.

`provenance` retains original package keys/hashes/CRC, native commit
`4e5daa4cf6702eb34c3855fb41df05aae6d292d1`, capture-record commit `f1e1a3d6e`,
writer-source/binary/library/report hashes and the original view key. The nearer
input also names exporter commit `14fa81118`; its writer and binary hashes match
the first input. Original cooker packages are not included. Expanded triangles
and attributes suffice for independent Python checks; replaying the native
cooker requires the original packages. Geometry is generated from OSM-based
building data, not source OSM or imagery textures. Only elapsed observer timing
was removed. Gzip uses timestamp 0 and no filename.

Compressed SHA256:

- `native-cell.json.gz`: `e29429e2fa670c2a5ce4b2357ee274c31fc6be7764866737b57933fb863a6c99`
- `near-cell.json.gz`: `ccc956c2a9672975dd9693740cb3f814f02d7863537e0e82dd59a3eff5414409`
