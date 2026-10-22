# Complete dense building view

Original input for #844. Tokyo camera, one complete cell at 5.60–5.92 km:
576 Shell sources, 0 Fine/Massed, 14,169 stored triangles, 0 unsupported sources.
Every direction and hidden surface of the original selected 3D cell is retained.
The native replay reproduces geometry bytes and footprint/detail metadata exactly.

| File | Content |
|---|---|
| `cell.json.gz` | Actual whole-cell geometry, source index ranges, original footprints/heights and analytic roof/part plans |
| `inventory.json.gz` | All 3,066 original view records: ECEF bounds/anchor/eye, geographic cell bounds, LOD counts, geometry and encoded/wire bytes |
| `original-inputs.tar.gz` | Unchanged native index, all original parent shape blocks, selected view and original per-package hashes |
| `context.json` | Actual scenario, 1280×720/60 profile, 6 m/s and 360°/s sequence, projection/error, input/build hashes and frame conventions |
| `manifest.json` | SHA256 and byte size of each delivered data file |

Inventory totals independently match the native control: 613,121 footprints and
21,172,410 building triangles. The archive supplies the complete parent required
by the native decoder; `cell.json.gz` contains only the original selected cell.
No selection is inferred by proximity. Null aggregate/part identities remain null.
Analytic parts/faces and exact per-source triangle index ranges are retained.

Geometry is Float32 ECEF relative to `anchor_ecef`; adding that anchor gives ECEF.
Scene coordinates use East/Up/minus-North about the scenario's WGS84 surface anchor.
The supplied Double frame maps the actual starting camera to the recorded ECEF eye.
The original benchmark samples `(frame + 1) / 60`, for frame 0 through 59.
`context.json` retains its actual start/finish matrices and world-axis motion rule.
The projection allows 1 pixel; the original normal-3D selection radius is 64 m.

Actual UV, packed normals, wall colours and material-metre attributes are present.
Packed GPU placement/material bindings and opacity are unqualified. Research must
name an opaque-control assumption explicitly. Current lighting, shadows and later
forest/cloud budgets remain part of the eventual native integration contract.
This frozen reference does not establish a new LOD error or runtime budget.

## Read and reproduce

Python 3.10+ reads the JSON with standard `gzip`/`json`. All original native inputs
are included; no network, private cache, new acquisition or asset regeneration.
Build the established exporter using the complete generator compilation flags and
native libraries described in `test/experiments/render/native_building_selection.md`.
Its additional `--full-view` mode restores the archived cell/projection and selects
the normal 3D hierarchy. It fails if replay differs from the original geometry or
footprint/detail metadata. Other exporter modes retain their previous behavior.

```sh
view_inputs=$(mktemp -d "${TMPDIR:-/tmp}/outshine-full-view.XXXXXX")
tar -xzf test/outshine/data/render/building-full-view/original-inputs.tar.gz -C "$view_inputs"
view_parent=6d3f9945a1035effd6769a8d93a00499aad7662296e276c8ec15dc2a10b52cae
view_key=1f1d5c8f5e910a6199092d3caeb7c92c29f7cb5013d5a7392d56fa64a139860f
"$native_selection_exporter" "$view_inputs/$view_parent/input.json" \
  "$view_inputs/replayed.json" --full-view
python3 test/experiments/render/check_building_full_view.py \
  --input "$view_inputs/replayed.json" \
  --artifact "$view_inputs/$view_parent/$view_key.bin"
```

The independent reader compares original vertex/index/attribute bytes, validates
source coverage/ranges and rejects altered geometry, source bindings or artifact SHA.
Observer/JSON costs are diagnostic costs. The archived input originates from the
private ordinary-3D control with projection selection disabled; product source and
shared caches were unchanged. Original provenance is retained in native packages.
OSM-derived inputs: © OpenStreetMap contributors, supplied through OpenFreeMap.
