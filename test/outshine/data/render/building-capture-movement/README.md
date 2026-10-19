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

## Original Fine observer-ball control

Refs #862/#852. `original-fine-cell.zip` contains the unchanged original cell 58
Fine geometry used by `measure_native_capture_region.py`: both independently
exported native streams, their SHA/count/ownership receipt, input metadata,
native camera/report, original index/forms and archived view package. No private
logs or rendered images are included. Every archived file byte matches the
preserved original; ZIP entry timestamps are 1980-01-01 with sorted relative names.
Archive SHA256:
`7b7d196ac58b57f22e347d722de5a7e266b8f6398826d7ca7f1d4549c3421203`.

```sh
.venv/bin/python -m zipfile -e \
  test/outshine/data/render/building-capture-movement/original-fine-cell.zip \
  /tmp/outshine-original-fine-input
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error \
  test/experiments/render/measure_native_capture_region.py \
  --fine /tmp/outshine-original-fine-input --radius-m 6 \
  --output /tmp/outshine-original-fine-region-result
```

Use fresh system-temp paths. The input contains 51970 vertices and 26378 triangles
from 32 original sources of parent
`035e2150a3c1d52a53b4f299007ec4557489a8b51c5780f58928a84a7c8b4500`.
Source ordinals remain stable; original ID[0,0] means unknown and stays unknown.
Historical absolute path strings in metadata are provenance only: the Python
consumer reads only these unpacked files and never follows those paths.

This earlier geometry control is distinct from the actual capture inputs above.
It assumes opaque triangles, invents no Part/Face identity and grants no runtime
capture, continuous native reuse, material/culling or device admission. Original
source acquisition and rebuilding the native cooker require additional original
producer resources; this bundle suffices for the independent Python control.
