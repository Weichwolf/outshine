# Direct glazed rooflight panels

Owner: [#500](https://github.com/Weichwolf/outshine/issues/500). The installable
`outshine_ref.buildings.rooflight_panels` module supplies directly constructed
rectangle rims/panes and checked contact render skins. Building recipes and the
existing default recipe-graph v1 retain their semantics. This is a Python
reference procedure, not a native/cache/whole-building LOD admission.

## Product and prepared input

`closed(fields)` emits one closed rim/pane component per logical field. It uses
44 triangles, matching the previous CGA prism/ring construction's physical
volume without executing polygon differences, extrusion or Boolean modelling
when emitting the component. Templates are read-only, bounded to 32 entries and
typed: a warm float/bool-equivalent key cannot bypass input validation.

Fields come from the existing camera-independent industrial rooflight plan:
`id`, `bounds` (metre section-run/transverse rectangle), `slope`, `intercept_m`,
`axes` (right-handed XY frame), `origin_xy`, `ceiling_m` and source ownership.
Frame width is 0.08 m intrinsically along the slope and across it. Frame depth is
0.12 m vertically; the closed pane lies 0.04..0.046 m below the roof plane.
Output positions/normals/UVs are metric, with semantic reference slots 10 for the
rim and 3 for glass; consumers bind their actual materials. `triangle_surface`
packs finite nondegenerate input triangles with geometric normals/intrinsic
per-triangle metric UVs. The existing hall adapter imports that identical
packing procedure instead of keeping a duplicate implementation.

`prepare_host(shell, fields, material=1)` checks the actual immutable `PrismCut`
host: every owned lightwell source must exist, and correctly facing triangles of
the expected wall material must cover each outer frame contact completely.
Coverage is tested on all four actual wall planes, with an area tolerance of
1e-7 m². Missing/shifted/incomplete contacts and changed material IDs reject.
The caller must bind an **opaque** parent wall: the material ID check verifies
identity, not arbitrary downstream shader transparency. Current source controls
use the existing opaque wall material and retain their exact source palette.

`contact(host)` then emits 36 triangles per field, omitting only the eight outer
rim triangles buried against the checked host. Top, underside, inner rim and
all 12 glass triangles remain. Its geometry is an open render component and
requires the retained mating body; it is not an independent collision solid.
The full logical/physical source body remains unchanged. Emission neither
rebuilds the CSG body nor repeats contact certification. Body/source/CSG setup
is still required on asset construction; no work is made to disappear merely
by labelling the input as prepared. No new persistent index/cache is introduced.

## Independent controls and rendering

The actual stored provider halls have 55/16/46 fields. Native solid symmetric
difference proves the closed construction's volume matches the former CGA
components. Trimesh's triangle-ray implementation independently compares 1440
fixed seeded rays from above and below the roof: first-hit distance, material
and normal agree with the original complete body/parts. A test-only all-triangle
broad phase avoids an additional spatial-index dependency. Missing/shifted/
wrong-material/corrupt hosts, standalone open-skin collision use, capacity,
mutation and warm-cache type aliases have explicit negative controls.

The 38 source checks after library promotion cover these controls plus existing
rooflights, halls and shared graph/hall consumers. The same suite is run against
the freshly installed wheel. Original v1 render meshes and current direct-skin
inputs are replayed separately to preserve actual array/dtype/palette evidence.
Proof commits, terminal results and artifact bindings belong to the delivery PR.

Actual 1920×1080 Software-Eevee controls use the earlier verified cameras/palettes:
whole/near/opposite/interior, plus a new four-times-distant camera and the entire
37-source Grohnde tile. Earlier CGA images are reused only after exact input
array/dtype/palette checks; they are not labelled as newly rendered. New images
stay in system temp and are optically opened. Small nonzero image differences
remain; no bitwise raster, general transition/shadow/radiance or nighttime bound
is claimed. All glass thickness and source/room/access identities are retained.

## Measured scope

Frozen `6db23d04f`: normal-priority, shared four-logical-CPU host, concurrent
installed-wheel checks/replay. 21 warm runs; timings in ms, traced peaks in bytes.
These are panel-emission measurements, not whole-hall/frame/device speedups:

| Fields | Construction | Cold ms | Warm median ms | Triangles | Unique array bytes | Traced peak bytes |
|---|---|---|---|---|---|---|
|55|CGA closed|122.068|3.495|2420|377960|410248|
|55|direct closed|4.448|1.477|2420|377960|382992|
|55|checked contact|6.209|2.955|1980|309240|319152|
|16|CGA closed|28.747|0.508|704|113168|121192|
|16|direct closed|1.430|0.418|704|113168|114912|
|16|checked contact|2.409|0.891|576|92592|96000|
|46|CGA closed|46.374|2.287|2024|309144|343432|
|46|direct closed|2.890|1.835|2024|309144|321120|
|46|checked contact|2.555|1.362|1656|252936|267648|

Body/source preparation separately costs 237.265/103.772/188.395 ms; contact
certification adds 96.064/24.992/68.085 ms once to those inputs. Contact emission
saves 18.18% of field triangles/storage and halves the number of transformed
components versus CGA. Direct closed construction strongly reduces template-
cold work; warm medians do not establish a uniform speed gain. The contact
preparation cost needs justification by reuse or measured downstream savings.
No savings in unchanged body work, total allocator/copy traffic, native scratch
or device frame time are inferred. Known final position/normal arrays and emitted
triangles/bytes are reported; unmeasured backend work stays unknown.

The chosen general construction is the direct closed primitive. The contact
variant remains optional: its lower geometry/storage does not demonstrate a
uniform warm CPU or complete-pipeline gain and needs the certified opaque host.
The prepared body plus contact parts uses 10144 instead of 10512 triangles in the
large individual hall; the same 37-source tile uses 71339 instead of 72523. No
source, existing hall or access programme is removed to obtain those numbers.

Nine new 1920×1080 Eevee images (16 samples, llvmpipe) were rendered and optically
opened; both final raster batches ended0 using the previously documented flush/
post-render teardown workaround. Pixel differences are small but nonzero:

| View | Variant | Changed pixels / 2073600 | Worst channel difference / 255 |
|---|---|---|---|
|near|direct closed|62|5|
|near|contact|127|13|
|inside|contact|107|1|
|opposite|contact|34|5|
|whole hall|contact|536|9|
|far|direct closed|407|7|
|far|contact|663|15|
|whole source tile|contact|994|19|

The source 38-check run and the same 38 installed-wheel checks pass on library
promotion. The final immutable-template refinement has its focused six checks
and a fresh 38-check wheel run. Five actual new component inputs and eight
previous v1 inputs replay exactly after promotion (arrays/dtypes/source palette).
The three far inputs and the entire contact source tile also replay exactly
(arrays/dtypes/palette and facade bindings). All source-member metadata agrees
except 24 actual plan/shell/assembly timing measurements in eight unchanged
generic pitched-room controls; these diagnostics are measured anew. The whole
source reference is separately checked before emitting its contact variant.
These local controls do not establish general image/shadow/LOD bounds.

## Reproduction

Use `.venv`, a frozen checkout and explicit system-temp outputs:

```sh
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python -m unittest verify_rooflight_surfaces verify_industrial_rooflights verify_industrial_halls verify_recipe_halls verify_recipe_graph -q
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python test/experiments/buildings/rooflight_surface_probe.py --references /tmp/outshine-buildings-rooflights-cameras-output --output /tmp/outshine-buildings-rooflight-surfaces-output
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python test/experiments/buildings/rooflight_surface_scenes.py --far-reference /tmp/outshine-buildings-rooflights-cameras-output/Rooflight2WholeGlazed-geometry.json --tile-reference /tmp/outshine-buildings-rooflights-tile-output/GrohndeRooflightsGlazed-geometry.json --output /tmp/outshine-buildings-rooflight-surfaces-scenes
```

References are the original actual inputs from [PR501](https://github.com/Weichwolf/outshine/pull/501),
reproducible with its industrial-rooflight probe and fixed stored source cases.
[Instant Architecture, SIGGRAPH 2003](../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
page 4 optically inspected, motivates component replacement. The reused hash-
matched SDK/Ghidra extrusion/Boolean study belongs to #489/#501; no ready Esri
rooflight recipe or new oracle-render equivalence is claimed.
