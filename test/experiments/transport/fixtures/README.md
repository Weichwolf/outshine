# Transport reference fixtures

`surface-sampling-vienna-4ea502363.npz` is the explicitly generated derived mesh
and 22,000 contact queries/results from [#69](https://github.com/Weichwolf/outshine/pull/69).
Native integration belongs to [#48](https://github.com/Weichwolf/outshine/issues/48).
No raw provider tile, decoder, source refresh or native result is included.

The adjacent JSON pins the fixture bytes, query/result buffers, reference commit,
source hashes/receipts/usage, local ENU frame and numeric comparison tolerances.
Arrays use local metres: `vertices`/`triangles`, `queries_xy_m`, `points_enu_m`,
unit upward `normals_enu`, `face_indices`, `reasons`, and the original road mapping.
Unresolved height/normal are NaN and Face -1; the reason remains explicit.
Face indices/reasons compare exactly, heights at 1e-7 m and normals at 1e-12.
Facet normals do not imply smooth C1 terrain or a complete functional Place.

From the repository root, verification only reads existing pins:

```sh
.venv/bin/python -W error test/experiments/transport/surface_contact_fixture.py
```

Reference creation is a separate deliberate command requiring the retained,
hash-checked `4ea502363` Ground/benchmark products under `build/experiments/transport/ground/`:

```sh
.venv/bin/python -W error test/experiments/transport/surface_contact_fixture.py --generate
```

Generation verifies the original query and full-result digests before writing.
Normal tests and the default verifier never create reference data or update pins.
