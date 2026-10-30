# Native capture and perspective opponents

Owner: #1831 / #1838 / #1797. Whole unchanged 1558-source/38476-triangle native input;
19 original operand hashes. The existing MaterialFieldPass renders references.
No new World renderer, stored field, view bank or runtime admission is introduced.

```sh
cmake --build build --parallel --target test_experiments_render_NativeBuildingImpostors
python test/experiments/render/export_native_world_field.py \
  --binary build/test_experiments_render_NativeBuildingImpostors \
  --output "$system_temp/new-native-world-field"
```

Python 3.10+ and NumPy. Fresh system-temp output only. Six fresh native processes
each render two poses and an identical repeat. Fixed 128² orthographic capture;
1280×720 perspective originals, terrain, terrain/opening and both original unions.
The PBR viewer follows the corresponding World pose in every capture. Raster poses
are 32000 m from the unchanged support centre, rotated 0.25°/35° about vertical Y.
The first is a small geometric candidate; the second reveals a different view.
Neither name grants coverage, lighting, depth or normal World validity.

Use `--sample-axis 2` for a fresh 2560×1440 World reference. Capture geometry
remains 128²; FoV, pose and PBR viewer remain unchanged. Raw centre samples then
fall at output-pixel quarter positions `(0.25,0.25)`, `(0.75,0.25)`,
`(0.25,0.75)`, `(0.75,0.75)`. The existing comparison resolves common 2×2 samples
to 1280×720 after depth ordering:

```sh
python test/experiments/render/compare_native_world_fields.py \
  --input "$system_temp/new-native-world-field" --sample-axis 2 \
  --output "$system_temp/new-native-world-comparison"
```

`manifest.json` binds executed requests, native reports, binary/driver/operand and
buffer hashes. `samples.npz` holds unfiltered linear premultiplied Float32RGBA
readbacks of native RGBA16F and raw reverse-Z D32F. Load with `allow_pickle=False`;
logical names use `arrays[name]`
for physical key, dtype, shape and SHA256 of array bytes. Perspective arrays share
one declared `crops_xyxy[pose]`; zero-fill the declared `raster` and place the crop to reconstruct
the full original byte hash. Every omitted pixel was verified exactly zero.
Capture arrays retain all 128² samples. Native RGBA/coverage/depth union and repeat
errors remain in `reference_failures` and make the exporter exit unsuccessfully.
Each readback is cropped immediately; the shared pose crop reconstructs and
checks every full-buffer hash without retaining all full rasters in Python.

Lens matrices are native Float32 column-major Lens results recomputed from the
effective cameras, with double pre-view translation and zero jitter. They are
not GPU-uniform readback. ViewProjection acts on original position minus eye;
camera-forward becomes negative camera-Z. Arrays start at top-left; pixel centres
are `(x+.5,y+.5)`. No filtering or temporal accumulation precedes the raw exports.
Runtime `WorldFromField` and live World revision remain null: normal World field
consumption is still absent. The directed Python AABB displacement bound uses
the actual native projection, but does not bound GPU rounding or material/coverage.
Readback, integral generation and process startup are diagnostic work, not frame
timing or an 8 GB device claim. Device name and native submission timings remain in
the reports; rendering and Python-derived checks are distinct.
