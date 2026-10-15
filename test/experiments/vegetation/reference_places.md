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
camera controls. Choose parallelism from actual host resources; use a clean
frozen worktree and record load and terminal status. There is no global host
lock. `publish_places.py` checks all four actual
camera/lens reports, source hashes and historical implementation snapshots
before copying the manually opened/reviewed PNGs. A successful publisher is
not an optical-quality verdict; visible improvement remains the update rule.

The first frozen `17fa3b60f` Koerbersee attempt stored all 53,200 stable sites
but hit its explicit 180 CPU-s limit before saving a PNG (SIGXCPU, 77.82 s wall,
3158.22 MiB peak RSS). Its original inputs and failure remain in
`/var/tmp/outshine-vegetation-place-shots-17fa3b60f/`. A local EGL query reports
software llvmpipe for the default surfaceless backend; no GPU rendering claim
follows from using EEVEE.

`place_demand.py` now bounds complete cached angular geometry with transformed
spheres before any Blender instance is created. The camera tests six expanded
perspective halfspaces. Sunlight additionally sweeps each sphere toward every
possible receiver down to a declared scene floor, intersecting the allowed
travel intervals of all six planes. The floor includes terrain, water and all
plant-product bounds. This retains off-screen and behind-camera casters.
The sphere grows by `2*sin(a/2)` per metre for solar angular radius `a`, covering
the finite sun disk's penumbra; minimum downward light direction bounds travel.
Light disks touching the horizon conservatively keep every caster. A 5 cm
renderer tolerance plus a Float64 magnitude guard expand the tests. There is no
occlusion or exact sphere/polyhedron claim. All 64-view cached vertices must
fit their declared bounds; every original site/ID remains in the complete plan.

Five independent controls include near/far intersections, off-screen and
behind-camera shadow casters, a penumbra missed by a point light, transformed
box corners/projected shadow samples, invalid frames and the low-sun fallback.
Run `place_demand_checks` with the previous scene/camera controls. The correction
retains source data, camera, resolution, light, samples and existing plant
products; only irrelevant renderer instances are rejected. A successful
unchanged-profile image remains required before any public image update.

The frozen `30aef3c0c` real Koerbersee composition keeps every column of all
53,200 original sites bit-exact. It selects 4,569 camera and 4,776 sun instances
instead of 106,400 role instances. Despite this reduction, the same 180 CPU-s
render guard fired (66.16 s wall, 177.86 CPU-s, 2891.35 MiB peak RSS). A separate
optical diagnostic also failed at its explicit 600 CPU-s guard (182.74 s wall,
591.57 CPU-s, 2925.65 MiB); the original cost gate remains red. Neither produced
a PNG. Constant and physical-sky full-resolution empty-scene probes both hit
their separate guards; they do not establish that the sky model is the cause.

`export_place.py` calls the same composition without rendering and saves a
compressed `.blend`, packed procedural images and `<Place>-scene.json` receipt
under an explicit system-temp output directory. The receipt records actual
camera/clock, source and implementation hashes, complete site plan, selected
roles, Blender version, samples and packed-image/file hashes. A fresh Blender
process can load the scene without GIS dependencies, generator modules or
original image files; it must retain 2560×1440, EEVEE/64 samples and the saved
camera/light settings. This is a static selected reference view, not a complete
runtime world/cache asset. Export success is not a rendered/optically reviewed
Place image; hardware/backend, render costs and final PNG remain separate.
