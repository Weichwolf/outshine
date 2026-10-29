# Registered native original fields

Native connection for #1721/#1570, using the existing MaterialFieldPass and GPU
integral. `NativeBuildingImpostors --material-fields` accepts complete original
geometry under `geometry/0/` and explicit capture cameras in `input.json` queries.
It uploads the source once, captures each query, then reads coupled raw colour,
raw depth and the paired Float32 integral. No private cache, reconstructed Raw
planes, second renderer, scheduler or SourceView admission is substituted.

```sh
PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings:test/experiments/transport \
python test/experiments/render/compare_registered_fields.py \
  --binary "$native_build/test_experiments_render_NativeBuildingImpostors" \
  --output "$fresh_system_temp_output"
```

The driver reconstructs the existing versioned originals through
`field_background_inputs.original_inputs()`. Camera/raster come from the frozen
native fixture, not the differing viewer camera. Every native result must retain
that exact binding. The three original material roles match the existing native
control; this is not a general scenario/material loader.

Controls use all supported original triangles. The ordered neighbour moves one
original AABB diagonal along capture-forward; the interleave moves one tenth
along capture-right. Translation rounds positions to Float32 before upload.
The driver checks all resulting vertices against the same capture volume and
records the measured margins. These finite checks do not implement #1720's
conservative pre-IO metadata/rounding admission. Unsupported source ordinals and
the archived opacity assumption remain explicit in source provenance.

Each query contains `name`, `sun_elevation_deg` and `capture`. The capture object
has `eye_m`, `right`, `up`, `forward`, `orthographic`, `x_mag_m`, `y_mag_m`,
`yfov_rad`, `near_m`, `far_m`, `width_px`, `height_px`; these are the existing
native camera fields. It must keep the raster of the held input. The mode writes:

| File | Meaning |
|---|---|
| `<name>-colour.f32` | Raw premultiplied linear HDR RGBA, coverage in channel 3 |
| `<name>-depth.f32` | Coupled reverse-Z Float32 point depth; zero is clear |
| `<name>-integral.f32` | RGBA32F adjacent hi/lo pairs; physical width `2*(W+1)`, height `H+1`, raw raster unchanged |
| `material-fields-result.json` | Actual camera, dimensions, native geometry and scoped GPU/wall times |

The Python comparison requires opaque point masks, verifies nearest original
colour/depth against a fresh joint native capture, and distinguishes actual
interleaving from mere overlapping hull intervals. Whole-group order is used only
for separated intervals. Both components have a zero border and their Float64 reconstruction is compared against
an independent Float64 source sum. The report names `rgba32f-hi-lo-pairs-v1`; NPZ exports retain the raw
`<name>_integral_pairs` arrays. The existing positive Float32 compatibility bound
is **not** a local HDR rectangle-query guarantee; #1718 remains a separate defect.

There are no ordinary oracle writes or published pictures. All inputs, readbacks,
logs, results and optional review images stay under the explicit system-temp
output. The existing frozen original capture is an independent regression input;
normal tests must not regenerate it. Equal registered original points prove this
capture connection, not approximate Raw replacement, continuous subpixel
coverage, pose reuse, external-background composition, World selection, cache
Miss/Hit, main/shadow depth, GPU retirement or the full 720p60/8GB budget.
