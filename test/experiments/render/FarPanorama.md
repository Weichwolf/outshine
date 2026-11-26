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
rejects unstable depth reprojection at disocclusions. The default product domain
allows rotation at the anchor.

Cube faces follow +X, -X, +Y, -Y, +Z, -Z in east/up/south coordinates. The packer converts
reversed perspective depth to radial metres and mirrors the right-handed capture camera
rows into cube storage. It retains scene-linear RGB, opaque coverage, source declarations
and a payload-derived revision. The capture records native GPU, readback and writing costs.

## Native rotation control

Create a 1280×720 scenario with the same world origin and an absolute camera at the
capture anchor. Use the original lens for the display view. Pass the comparable world
exposure with `pack_far_panorama.py --exposure` for paired colour checks.

```sh
outshine benchmark --scenario "$view" --panorama-product "$product" \
  --frames 30 --angular-velocity 0,-360,0 --image "$png" --output "$report"
```

This mode uploads the two cube textures once and draws one fullscreen triangle per
frame. It samples colour across cube faces, samples depth at the nearest texel and writes
reversed scene depth. The benchmark records the asset range, native submission times,
bytes read, upload cost and memory. Set `--translation-domain-m` when packing a validated
local replay radius. The runtime checks the complete linear camera path against that radius
before submission.
During translation it iterates radial-depth reprojection up to three times. The fallback
for an unstable ray is its rotation-only colour with zero geometry depth.

Add `--tile-product "$tile"` to draw a prepared near tile in the same pass. Both products
use the same reversed-depth buffer. The tile covers nearer samples and reprojection holes,
while valid panorama depth covers tile samples behind it. Benchmark reports distinguish
this two-product composition from a complete world render.
