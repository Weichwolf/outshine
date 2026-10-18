# Historical Python Place reference images

The user replaced these GIS/Place presentation requirements with four flat
vegetation research tiles. Current images and workflow: [biome_tiles.md](biome_tiles.md)
and PR #290. This document retains the earlier source/camera experiments.

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

Frozen `77277ff26` exports the actual Koerbersee view successfully: 14,136,746
bytes, three packed images, 20.03 s wall/20.04 CPU-s and 576.93 MiB peak RSS.
All six site-array columns remain bit-exact against `30aef3c0c`. A separate
isolated fresh Blender process opened the file without GIS/generator imports,
verified camera/profile/64 samples and packed-image hashes, replaced every
original image path with a nonexistent path and successfully reloaded all
three images. Both processes ended0. No rendering occurred in this check.
Current packet, cost and portability receipts:
`/var/tmp/outshine-vegetation-packed-77277ff26/`. Blend SHA256:
`942c844d36d37340d05e1ab853be5fdcdc4e6821410dcecdae0be04ed020bbc4`.

## First complete private optical diagnosis and fine-water correction

Frozen `48b46571d` now renders the complete packed Koerbersee scene at its
unchanged2560×1440 camera with an explicitly private **one-sample** profile.
Stored64samples and the original cost gate remain untouched; this diagnostic
is not a cheaper passing version of that comparison. Its first PNG completed:
27.077s wall/53.905CPU-s/3152.69MiB peak RSS, llvmpipe OpenGL. The whole image
and the native camera reference were opened. It exposed coarse green terrain,
uniform narrow conifer proxies and a missing near lake. No public image update.

The cause of the missing lake was the water filter: fine z14 `lake` features
were discarded while only `pond` survived. `f1af72d2e` retains fine water and
clips coarser polygons to regions outside supplied finer tile footprints.
Buffered geometry is clipped to its own tile too; boundary-only intersections
do not create surface records. Superseded/outside sources keep explicit reasons.
Three independent controls cover fine lakes, dry fine tiles and exact disjoint,
order-independent tile ownership; original scene/contact/hole controls pass.
This changes neither DEM nor the complete planting plan, and does not claim
connected water levels or hydrological/shoreline accuracy.

The corrected full composition/export completes with exit0:
19.817s wall/19.765CPU-s/580.18MiB peak RSS. All six original columns of all
53,200 sites and the actual camera remain bit-exact. Water sources207→224,
including17 fine lake fragments. New packed scene SHA256:
`b93e86e4a01be72e34f9a94a42c16bf05b00c78d69ef8a8f3c5c511823aa59e8`.
The same private one-sample profile then completes with exit0:
26.694s wall/53.657CPU-s/3196.54MiB terminal child peak RSS. Before/after PNGs
were opened: the near lake is now present. It still has a simplified shoreline
and overly pale material; terrain form/material and inappropriate generic
conifers over grass remain the next concrete image deficits. Species atlas/
spatial-marking experiments have not yet changed these prototype instances.

Inputs and first diagnosis: `/tmp/outshine-vegetation-packed-optical-48b46571d/`.
Current export, exact plan comparison, PNG and terminal reports:
`/tmp/outshine-vegetation-place-water-f1af72d2e/`.
`render_packed_preview.py` takes an explicit packed scene, source receipt,
diagnostic profile and temporary output. It requires packed procedural images,
retains the source resolution and records both old/new sample counts. The profile
in`packed_optical_diagnostic.json` explicitly forbids presentation/cost admission.
Four final public Place images, the64-sample cost gate and SpeedTree/botanical/
LOD/native acceptance remain open. No HTTP showcase is restored.

The subsequent source-grass correction is complete for the Koerbersee prototype:
`86e5b9b94` changes the9,853 nonwood grass form indices to cached graminoid tufts,
while retaining all IDs/positions/frames/source indices/scales. The whole private
before/after is opened; scattered inappropriate conifers on grass are gone.
Eight actual cached forms are also opened in the declared near control.
[place_grass.md](place_grass.md) owns the exact verification, costs and remaining
coverage/material/LOD limitations. The shared geometry constructor is delivered
independently through PR#276; this does not add accepted botanical species or
replace the public four Place images. Current complete diagnostic and product:
`/tmp/outshine-vegetation-place-grass-86e5b9b94/`.
