# Four current Python transport views

Owner: [#283](https://github.com/Weichwolf/outshine/issues/283), under #11.
The gallery is deliberately just four **1280×720** images: Zürich rail approaches,
Basel Badischer Bahnhof, Vienna's bridge field and New York Midtown.
They are actual Blender 4.3.2/Workbench renders of generated 3D vertices/faces,
with DEM, closed surface bodies and rail sections; no native pixels or textures.
No server, closeups, inset panels, height exaggeration or displayed debug guides.
The single Vienna camera moves closer to the existing elevated field.

## What the images actually establish

Zürich/Basel/Midtown run the existing component planner, owned weighted-height
library, conforming soil/traffic partition and rail-hardware library on complete
pinned OpenFreeMap z14 tiles. Whole-face gradients and complete physical rail
supports are independently replayed from saved meshes. Rail-support faces remain
at their explicit horizontal datum; other selected ground can follow coarse DEM.
Material colours are procedural inspection choices, not observed OSM surfaces.

| Place | Used/input axes | Traffic faces | Hardware length m | Sleepers | Complete rail footprints | Weighted DEM vertex RMS m |
|---|---:|---:|---:|---:|---:|---:|
| Zürich |2930/3222|104990|17511.908|29209|29603|0.985046|
| Basel |1260/1407|39869|20388.171|34024|34732|1.067274|
| Midtown |2069/2621|76415|3549.818|5920|5998|2.015742|

All three saved replays pass the unchanged normalized whole-face grade limit
1+1e-7 and contact limit 1e-7 m. Actual pin errors are zero. All **70,333** hardware
footprints remain supported; gauge/closed-section/instance/conflict replays also
pass. Remaining axes: Zürich 292 /Basel 147 have deferred levels; Midtown 538 levels,
10 indoor recipes and 4 route/missing recipes. Original polygons retain their
separate deferred records. Physical source/frame binding is not inferred from
proximity or from the picture.

Vienna freshly rebuilds the actual [#21 bridge/approach/tunnel research](https://github.com/Weichwolf/outshine/issues/21)
from its pinned previous bridge field and real three-tile fixture. It does **not**
substitute a flat network for the advanced recipes. The new saved body has 2,946
vertices /5,996 faces. Previous chosen contacts are preserved; independent whole
bridge/ground clearance is **4.52 m**, tunnel air 4.5 m and selected roof cover
2.879014 m. Analytic scalar profiles are C1; finite mesh faces are C0.
Its complete 3,259-record usage ledger has 71 used sources and 3,188 explained
omissions. This is an advanced **partial reference field**, not a finished
three-tile generator. The camera emphasizes the bridge field; enclosed tunnel
geometry stays enclosed. Piers, structural load, complete terrain/world contacts,
other traffic and train routing remain open. Native integration is separately #48.

## Source and verification

Snapshot prefix: `https://tiles.openfreemap.org/planet/20260927_080001_pt`.
Source tiles are Zürich14/8579/5737, Basel14/8538/5724, Midtown14/4824/6157 and
Vienna14/8938/5680,14/8939/5680,14/8939/5681. Downloaded bytes are hashed and retained
in the client source cache; full original features/attributes/IDs remain in inputs.
The current Python library is imported, with no SDK or foreign implementation.

72 affected controls pass on frozen `a36181c3e`, including eight actual tunnel
counterexample/whole-component checks against the exact external bridge reference.
The ordinary height/rail/ground/component controls and three complete saved-mesh
replays pass. Fresh Vienna reconstruction/replay and single-camera render pass on
`901d3b2cc`; it changes presentation, not bridge/portal kernels. Ruff, formatting,
functions ≤120 lines, diff check and documentation lint pass. No native/device gate.

The four selected PNGs were all actually opened, format/dimension/hash checked and
copied identically to both checkouts' `build/shots/reference/transport/gallery-283/`.
A changed selection/camera is user-directed: this is not an equal-input/equal-camera
algorithm speed or pixel-quality comparison with the former gallery.
[Current declarations](reference/transport-gallery.json) and [commands](README.md#vier-repräsentative-transportaufnahmen).
Capture a fresh input directory when replaying the current camera declaration.

## Measured offline work and remaining limits

Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Clarabel 0.11.1; BLAS/OMP 1. Same measured
Intel i5-1135G7 host with four logical CPUs; foreign CPU renders were active.
Per-stage wait4 receipts record terminal exit, wall/user/system time, actual child
RSS and before-load process snapshots. Builds/renders use nice 10; these are loaded
host observations, not native frame, GPU, cache-loading or device measurements.

| Offline stage | Wall s | Peak child MiB |
|---|---:|---:|
| Zürich flat plan |56.398|338.6|
| Basel flat plan |16.187|181.8|
| Midtown flat plan |30.082|272.0|
| Three rail products / independent replay |14.255 /19.451|112.3 /253.2|
| Zürich full soil/height/body |42.219|716.7|
| Basel full soil/height/body |21.951|331.2|
| Midtown full soil/height/body |22.241|540.7|

The normal weighted solver requests 8 s/100 iterations, explicit caps 262144
vertices/faces; geometry/height workers have 2 GiB address-space /60 CPU-second caps.
Preparation and one-step overruns are measured separately from solver time.
Selected full PNG renders took Zürich 99.229 s/Basel 76.222 s/Midtown 10.835 s under
changing foreign load; Vienna's selected rebuild/replay/render took 3.701 s.
No cross-scene throughput ranking is inferred. Current solver allocation/copy
work and its fair method comparison remain in the
[owned-height acceptance](weighted_height_library_comparison.md); this presentation
change introduces no second numerical kernel or new runtime dependency.

A trial joining all Vienna lower references to a generic shared terrain patch
produced no independently valid height mesh. Solver status alone is no
impossibility proof; the trial is not included as a successful world product.
The verified partial field remains visible, and full terrain/net composition stays
open in #20/#21. Other sites still defer elevated/underground recipes; this
selection does not claim that every Python reference already forms one complete
world generator or has been fully integrated into native Outshine.

Detailed receipts, original/selected meshes, source ledgers, logs/load snapshots and
PNG hashes remain at `/tmp/outshine-transport-gallery-283/` (`final-source/`,
`final-products/`, `selected-wien/`, `publication.json`, `selected-metrics.json`).
The previous pinned bridge reference remains under the explicit external
`TRANSPORT_RESEARCH_REFERENCE` path; no normal test refreshes pins or source bytes.
