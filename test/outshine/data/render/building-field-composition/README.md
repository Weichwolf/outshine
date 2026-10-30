# Bound building field/background inputs

Native A18/Metal inputs for #1716/#1715. Controlled translations of the existing
whole original cells 25/33/34/41, not separately acquired geographic neighbours.
All 46,889 supported triangles of 1,891 sources remain. Original source ordinals
3825/3827 have no supported geometry and remain explicitly unresolved. Cell 33
retains its archived opaque-control assumption. OSM: © OpenStreetMap contributors,
via OpenFreeMap; source provenance stays in `../building-full-view/`.

`manifest.json` binds native commit/binary, source/array hashes, cameras, placement,
lighting query and terminal runs. `captures.npz` contains Float32 readbacks and
Float64 whole-source bounds; load with `allow_pickle=False`. Original geometry is
already versioned: `field_background_inputs.original_inputs()` reconstructs its
three native material parts, per-triangle lineage and unresolved-source metadata.
Its arrays must match `geometry_arrays_sha256`; no private paths or cache required.

| NPZ key | Shape / meaning |
|---|---|
| `source_ids`, `source_least_m`, `source_most_m` | 1891 source ordinals and complete native-triangle bounds; parent namespace, not OSM IDs |
| `field_capture_colour`, `field_capture_depth` | 128×128 raw shaded reconstructed field, coupled to `capture_camera` |
| `original_capture_colour`, `original_capture_depth` | 128×128 freshly shaded whole original geometry in exactly that capture camera |
| `<name>_linear`, `<name>_depth` | 720×1280 pixel-centre original colour/depth in `viewer` |
| `<name>_ss16` | 720×1280 colour averaged over 16 physical microcamera rotations |

Names: `foreground`, `separated-background`, `separated-original`,
`interleaved-background`, `interleaved-original`. `*-original` draws the original
foreground and translated neighbour together. For source identity, distinguish
foreground instance 0 from neighbour instance 1 before using parent source ordinals.
The manifest supplies Double translations and actual forward intervals; native
positions are rounded to Float32 after translation.

Colour is premultiplied linear HDR RGB, channel 3 is coverage `1-T`. Arrays are
row-major, top-left origin, x right/y down; pixel centres at integer+0.5. Camera
basis rows are right/up/forward; subtract Double eye before projection. Viewer
reverse-Z has near 0.1 m/infinite far, so positive depth gives forward distance
`0.1/depth`. Capture is orthographic: `far-depth*(far-near)`. Zero is clear, not a
whole-original visibility certificate. Geometric hulls include hidden surfaces;
first-hit depth does not. Preserve separate capture/viewer cameras.

The separated control shifts the neighbour by twice the source AABB diagonal
along viewer forward. The other shifts by one tenth along viewer right. Whole
forward intervals separate only in the first case. Native shared pixel centres
verify 14 foreground-first hits versus 4 foreground-first/5 neighbour-first hits.
Both native unions reproduce nearest original colour and depth exactly. Filtering
foreground/background separately before over-composition differs from the native
joint 16-sample reference by up to 0.25 coverage in the separated case. This is a
finite counterexample, not a continuous error bound or a suggested tolerance.

The 16 samples do not establish converged area integration, native TAA, World
admission, arbitrary movement, shadow/depth authority, geographic coverage or the
720p60 budget. No terrain/sky/vegetation or full-world performance is represented.
Whole unresolved sources retain their fallback; do not silently qualify this set.

## Reproduce

Python 3.11+ with the existing experiment dependencies; native target
`test_experiments_render_NativeBuildingImpostors`. `--field` points to an existing
bound `--radiance-world` export with its complete private SQLite cache and raw
capture/report. Source cache is opened read-only and backed up privately. All
native work and logs go to a fresh system-temp directory; do not use shared cache.

```sh
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings:test/experiments/transport \
python test/experiments/render/export_field_backgrounds.py \
  --binary "$native_build/test_experiments_render_NativeBuildingImpostors" \
  --native-commit "$native_commit" --field "$bound_field_export" \
  --output "$fresh_system_temp_output"
```

Only `manifest.json`/`captures.npz` are shared frozen inputs. Generated native
meshes, duplicate readbacks, logs and review PNGs stay private. Existing original
input/manifests are validated before geometry use. Never regenerate an oracle as
part of an ordinary test run.
