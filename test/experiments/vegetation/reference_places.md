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

`place_inputs.py` selects explicit native provider URLs through that shared
kernel. Raw responses and receipts stay under an explicitly supplied temporary
directory. Fine DEM requests at every camera verify eye clearance even for
absolute camera heights. Initial vector coverage is 3 km at z14; coarse z8 water
and DEM extend to a declared 120 km terrain request window. Finest actual zoom
may be lower than requested. These windows do not prove complete 240 km world
coverage. An explicitly supplied Buildings receipt set can be reused without
copying or modifying its sources.

`render_place.py` composes actual curved DEM, source water and initial planting
sites from landcover. The rendered terrain mesh supplies the plant contacts
through the shared `SurfaceSampler`; fine camera terrain is retained. Source
buildings/water and nominal road buffers exclude sites. Final predecessor
reservations and reusable Buildings scene context still need binding. The
current catalog supplies one generic broadleaf architecture and experimental
parent-grown *Picea abies*, not a botanical identification of all source areas.
Complete cached angular products supply separate camera/sun views without
regenerating organs. Astral binds the sun to the actual fixed clock. This is an
initial Python image composition, without botanical/LOD/native admission.

Three independent scene controls check upward curved terrain and camera
clearance, source-site exclusion/stability under camera rotation, and a water
hole that must remain uncovered. Run `place_scene_checks` alongside the five
camera controls. Heavy actual renders run one at a time under the shared host
lock from a clean frozen worktree. `publish_places.py` checks all four actual
camera/lens reports, source hashes and historical implementation snapshots
before copying the manually opened/reviewed PNGs. A successful publisher is
not an optical-quality verdict; visible improvement remains the update rule.
