# Physical rail hardware on selected tilted ground beds (#340)

Owner: #340 under #21; existing horizontal reference kernel #250/PR #260.
The current Vienna body had traffic/rail beds but no physical ground rails.
This experiment adds actual closed rail sections and shared sleeper instances on
explicit affine parts of the unchanged #303 ground. Original bridges, tunnel
portals, piers, road/soil geometry, materials and source usage remain retained.

## Method and physical limits

First reference: Chen et al., [SIGGRAPH 2008 page 2/Figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
then Galin et al., [Eurographics 2010 page 8/Figures 17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf).
Both pages were actually opened for this work. They motivate separating the
source graph and parameterized physical geometry; neither specifies our rail
kernel, rigid transform, dimensions or source reconstruction.

The old installable horizontal rail section runs in an orthonormal frame on each
explicit selected field triangle. ENU vertices and sleeper transforms undergo a
proper rigid rotation/translation. Actual metric gauge, normal section heights,
box dimensions and volumes therefore retain their meanings. Vertical shearing
would change real transverse dimensions; the cross-slope control demonstrates
that failure. The default 1.435 m gauge and concrete sleeper interpretation are
procedural when tags are absent. Existing own profile dimensions/provenance stay
in [lib/rail_hardware.md](../../../lib/rail_hardware.md).

Noded axes retain original IDs/attributes. Existing switch/crossing/land gaps are
reused. Each coarse affine triangle is inset 1.35 m before cutting an eligible axis;
this conservative margin admits the full default hardware, including its tilt.
It leaves explicit gaps at C 0 creases. Full finite contact and actual source
traffic membership are checked afterward. Original traffic surfaces remain
usable through omitted hardware; no smooth train path, turnout or bank/crease
transition is claimed. Upper bridge and tunnel tracks are not silently flattened.

## Real inputs and actual products

Clean geometry/presentation source: `f8bca9a03ee8f6ac2d6f3a6d29d2493667b3070c`.
Same #303 original three OpenFreeMap tiles and actual DEM-derived ground;
no acquisition/refresh. Full source originals and original 3259-record usage
ledger remain upstream. The newly recorded graph/ID/usage/domain file pins are
explicit in `tilted_rail_inputs.py`; default camera-receipt reading remains
compatible and its separate path is tested. The affine field pin is
`20699c72258c3aae6e4a872d151241bc9d3cf8ce340a112e09e4847476206cac`;
actual ground pin is `16cd7e8b774136dc50e70e9549d376630b4f7bb88e221f4b46527c30367627e5`.

40 eligible rail chains contain 9,075.399 m XY length. Conservative existing
switch/land exclusions retain 7,803.611 m in 35 pieces. Affine-cell/short-span margins
retain 6,903.805 m XY in 77 pieces. Every one of those 77 physical products is admitted:
**11,516 sleepers,6,903.872 m metric rail centreline**,4,800 rail vertices and
147,176 total added hardware faces. Carrier provenance spans 25 original source
IDs,22 of which are carried by accepted pieces. IDs are retained at chain
provenance granularity, not presented as exact per-source used lengths; the
original noded-edge owner table stays pinned. Chain/cut/optional-piece exclusions
all retain their relevant source lists and reasons. No original source record
is reclassified. General-purpose methods, not place-specific rail geometry.

The final hardware is independently decoded from saved arrays. Its actual rail
triangle projections and independently reconstructed box corners form the
verification footprints. Every intersecting original positive-area ground face
must be an actual traffic face and agree with the selected affine bed; this
checks whole finite contacts, including interior peaks missed by four corners.
Gauge is measured along each actual segment's transverse in-bed unit direction,
not Euclidean distance across a polyline miter. Profile heights, instance sizes,
closed directed volumes, source centreline, mutual projection conflicts and
actual rendered prefix/material/normal ownership are checked.

Maximum replay errors: gauge 7.306e−13 m; source-bed plane 2.843e−14 m; sleeper
contact 3.159e−14 m; section height 2.027e−14 m. Maximum finite-bed uncovered area
2.842e−11 m². All 147,176 new faces survive actual Float32 presentation without
degeneration/inversion. Maximum added height over actual ground is 0.352110 m;
the already whole-face-verified old bridge clearance yields a conservative
**4.589256 m** remaining lower bound, above the retained 4.5 m requirement.

32 affected source/library controls and 13 actual Blender controls pass, without
skips, on final gate source `eafb2ae20df940af0b29787f3ed72131e4792e82`.
Both forms of the saved graph reader and corruption rejection are controlled.
The final input guard fixes the already captured domain SHA without changing
physical arrays. Final independent replay rechecks the emitted products.

## Fair transformation costs

Scalar point dispatch and batched NumPy matrix transforms use the same 4,800
original local rail vertices,77 actual frames and identical arithmetic/acceptance.
Both reproduce saved world coordinates exactly in these measured runs. Only this
operator is timed: profile construction, frames, sleepers, contact/projection
work, Mesh copies, export and render are excluded. Seven normal repetitions per
worker; one separate Memray 1.20.0 repetition with Python/native allocations.

| Same saved inputs/outputs | Batched | Scalar |
|---|---:|---:|
| Normal median, ms |0.462|21.593|
| Python point/batch dispatches |77|4,800|
| Logical scalar arithmetic operations |86,400|86,400|
| Allocation events |941|43,392|
| Cumulative requested bytes |504,488|4,315,427|
| Tracked peak heap, bytes |129,440|136,320|
| Owned output array bytes |115,200|115,200|
| Instrumented wall, ms |12.105|74.244|

Select batching for this Python operator. These are shared-host observations
while local software renders existed, not an isolated device or whole-generator
speedup. Tracking changes timing. Event counts include Python/native requests;
cumulative allocation requests are not live memory. Owned output is one known
amount, not every internal copy. The whole hardware build still takes seconds.

Full generation with verification and body exports:17.053 s /444.371 MiB worker
peak; optional hardware generation/admission 9.831 s. Work:162 chain-pair and 34
land-pair candidates,382 actual contact-face candidates and 57,734 mutual
projection candidates; no optional piece rejected after field cutting.
Limits:3 GiB AS/120 CPU s;40 s cooperative hardware-loop setting,128 accepted
pieces,16,384 sleepers,262,144 hardware faces,2,048 affine pieces and 1,048,576
mutual pair candidates. One profile call retains the existing library budgets.
Calls may overrun a cooperative step; this is offline research, not frame work.

Final body:311,616 vertices /575,592 faces;39,711,936 exported array bytes.
Known explicit export copies:41,442,624 indexed-XYZ bytes and 20,721,312 repeated
normal bytes. Shared instances remain saved; Blender inspection expands them.

## Actual regional picture and remaining gaps

Fresh matched 1280×720 before/after and 1920×1080 after pictures were opened.
Camera centres the generated hardware's median XY with a 1400 m span; identical
pose, resolution, AgX/daylight and original eight materials within the pair.
Two appended material slots identify dark rails and procedural concrete sleepers;
old source materials are unchanged. Those are inspection colours, not a new
physically complete metal/material shader. No closeups, insets, display cuts,
height exaggeration or extra ground. The camera was repositioned relative to
earlier #326 views, so only this matched pair is a direct image comparison.

`pixels.py`: 5,065 changed 720p pixels (0.5496%), maximum 24/255 channel change.
Rail detail improves locally but remains fine at the regional scale; overall
materials and the finite source border remain schematic. Initial pale hardware
and bounding-box camera were less readable and are superseded. The second
presentation uses the same physical geometry, with explicit material/pose change.
No claim of a complete visually finished world.

Blender 5.2.2/Eevee/llvmpipe LLVM 19.1.7, 16 samples/two threads. Offline software
wall/RSS: before 25.139 s /1,961.789 MiB; after 720 23.792 s /2,074.898 MiB;
after 1080 36.408 s /2,235.133 MiB. Load differences preclude a renderer speed claim.
Native/device/frame/LOD gates remain open. Kernel extension to library, smoother
whole-network profiles, actual switches, upper/deeper tracks and new consumer
reservations remain separate work.

Current local products/images/replay: `/tmp/outshine-transport-tilted-340-final/`.
Cost inputs/results/traces: `/tmp/outshine-transport-tilted-340-cost/`.
Commands in [README](README.md); existing NumPy/Shapely/Python rail library and
Blender dependencies only, optional Memray for probes. No SDK, foreign code or
new runtime dependency. All images remain local; no replacement gallery.
