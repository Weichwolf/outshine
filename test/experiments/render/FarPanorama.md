# Far panorama source capture

The world benchmark captures all six faces from one prepared source-derived world. Use a
square render extent with antialiasing set to `none`. Set the active perspective view to
90° and declare its axial near and far planes. The first two captures use 512×512. Their
planes are 1,200 m and 240 km. At the design trial of a 6 m eye shift and a 690 px screen
focal length, a 1,200 m foreground boundary limits the angular shift to about 3.5 px.

```sh
outshine benchmark --scenario "$source" --frames 1 --offline \
  --preload-seconds 240 --panorama-output "$capture"
python3 -B test/experiments/render/pack_far_panorama.py "$capture" "$product"
python3 -B test/experiments/render/preview_far_panorama.py "$product" "$comparison" "$png"
```

The comparison is a standard world benchmark JSON with the same geographic anchor and
camera exposure. The preview checks rotation and optional eye offsets against actual
captured colour and depth. It preserves zero depth for absent geometry. The reference
rejects unstable depth reprojection at disocclusions. Its rotation-only product domain
remains in effect until translated native views are measured.

Cube faces follow +X, -X, +Y, -Y, +Z, -Z in east/up/south coordinates. The packer converts
reversed perspective depth to radial metres and mirrors the right-handed capture camera
rows into cube storage. It retains scene-linear RGB, opaque coverage, source declarations
and a payload-derived revision. The capture records native GPU, readback and writing costs.
