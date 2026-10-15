# Python Place reference images

[#208](https://github.com/Weichwolf/outshine/issues/208) owns the four current
whole-place Python renderings under `doc/images/vegetation/`. The selected native
shots under `doc/images/places/` are location/camera references, never copied
outputs or background plates. The former web server and its UI are removed.

| Place | Scene focus | Camera height declaration | Vertical FOV |
|---|---|---|---|
| CentralPark | Dense park woodland and open lawns | DEM + 60 m | 55° |
| Koerbersee | Alpine meadow and conifers | 1772 m, no terrain offset | 33.97° |
| DarmstadtWest | Urban trees and courtyards | 205 m, no terrain offset | 38.04° |
| Malcesine | Mediterranean slopes and shore | 140 m, no terrain offset | 38.04° |

The source `.scenario` files retain latitude/longitude, bearing, pitch and fixed
UTC. New Python images use 2560×1440 with the same aspect and vertical FOV.
`place_camera.py` selects them through the shared `outshine_ref.places` camera
reader ([#216](https://github.com/Weichwolf/outshine/issues/216),
[contract](../../../lib/places.md)). One explicit WGS84 origin rotates ECEF
into right-handed east/north/up metres. Curvature
is retained rather than flattening distant mountains into a local XY plane.
Source height numbers remain unchanged; no geoid/MSL correction is claimed.

The camera basis uses right/up/back columns for Blender. A positive native pitch
raises the forward direction. Pixel projection has a top-left origin and uses
the vertical FOV; points behind the camera have no valid projected pixels.
Five independent controls check an equatorial circular section, curved-Earth
coordinates against PROJ's topocentric pipeline, cardinal directions/handedness,
exact image edges and all four source height/lens declarations. PROJ is a test
oracle, rather than a second production camera implementation.

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 \
  PYTHONPATH=lib/src:test/experiments/vegetation \
  .venv/bin/python -W error -m unittest place_camera_checks
```

The shared source/provider contract is discussed with Buildings in
[#205](https://github.com/Weichwolf/outshine/issues/205); native caches on another
host are not assumed to be available. Current state and missing images belong
in #208. Private data, reference comparisons and intermediate images stay in
system temp. Only the four current final PNGs are published; visibly improved
results replace them after opening and comparing, without a history gallery.
