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

## Original Fine observer-ball control

Refs #862/#852. `original-fine-cell.zip` contains the unchanged original cell58
Fine geometry used by `measure_native_capture_region.py`: both independently
exported native streams, their SHA/count/ownership receipt, input metadata,
native camera/report, original index/forms and archived view package. No private
logs or rendered images are included. Every archived file byte matches the
preserved original; ZIP entry timestamps are1980-01-01 with sorted relative names.
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

Use fresh system-temp paths. The input contains51970 vertices and26378 triangles
from32 original sources of parent
`035e2150a3c1d52a53b4f299007ec4557489a8b51c5780f58928a84a7c8b4500`.
Source ordinals remain stable; original ID[0,0] means unknown and stays unknown.
Historical absolute path strings in metadata are provenance only: the Python
consumer reads only these unpacked files and never follows those paths.

This earlier geometry control is distinct from the actual capture cell64 above.
It assumes opaque triangles, invents no Part/Face identity and grants no runtime
capture, continuous native reuse, material/culling or device admission. Original
source acquisition and rebuilding the native cooker require additional original
producer resources; this bundle suffices for the independent Python control.
