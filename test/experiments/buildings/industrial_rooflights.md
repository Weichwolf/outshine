# Industrial rooflight prototype

Owner: [#500](https://github.com/Weichwolf/outshine/issues/500), parent
[#32](https://github.com/Weichwolf/outshine/issues/32). This is an executable
exterior research prototype on three stored real provider halls. The default
hall builder and recipe-graph v1 retain their previous output. The probe injects
the alternative into the actual tile builder; it does not decorate renderer
images or substitute a manually authored building.

## Geometry and ownership

`industrial_rooflights.plan(hall, RoofSection)` creates immutable source-bound
rectangles on descending roof intervals. Both symmetric and asymmetric sections
use their existing metre frame. Facet margins protect ridges/valleys, and a
0.35 m inner-domain clearance protects outer walls and courtyards. At most eight
columns per interval and 64 fields in total are admitted before mesh expansion.
Tiny/flat/rising intervals and unsuitable domains produce no fields. Excess
capacity is rejected, rather than silently dropping selected fields. Supplied
heights/footprints and existing room/access IDs remain unchanged. The result is
an inferred architectural supplement, not an OSM rooflight observation.

One extra vertical prism per field opens the existing shell from 0.02 m below
its room ceiling through the roof. The original room/window/door/loading/ramp
cuts remain in the same Boolean operation. No full-hall second CSG operation is
needed in the probe. Only owned roof patches change; all remaining top facets
lie on the previous roof and retain their original projected domain.

A closed projected ring fills each aperture with a 0.08 m intrinsic rim. The
frame extends 0.12 m vertically below the original roof plane. A closed glass
pane extends from 0.046 to 0.04 m below that plane; these are vertical, rather
than normal, thicknesses. The XY-to-roof shear followed by the rigid ENU frame
preserves the section, and normals/UVs are calculated from the resulting actual
triangles. Cached local templates are read-only; per-field placement materializes
positions and normals. Material 3 uses the existing glass, material 10 the
existing metal frame. No new material/shader parameters or external textures.

The alternative constructs the same rim as four disjoint bars. Native solid
symmetric difference independently checks equal frame/glass volume on all three
sources. It retains internal contact faces and requires five components/60
triangles per field; the ring needs two/44. Both methods share the same cut shell,
panes, dimensions and existing functional hall components.

## Reproduction and checks

Use a clean frozen worktree with `.venv` and an explicit system-temp output path:

```sh
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python -m unittest verify_industrial_rooflights verify_industrial_halls verify_industrial_roofs verify_recipe_halls verify_recipe_graph -v
PYTHONPATH=lib/src:test/experiments/buildings .venv/bin/python test/experiments/buildings/industrial_rooflight_probe.py --output /tmp/outshine-buildings-rooflights-output --tile
PYTHONPATH=lib/src:test/experiments/buildings .venv/bin/python test/experiments/buildings/industrial_rooflight_cost.py --output /tmp/outshine-buildings-rooflights-cost.json
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/blender_city.py /tmp/outshine-buildings-rooflights-output/Rooflight2WholeGlazed-geometry.json --output /tmp/outshine-buildings-rooflights-output/renders --samples 16
```

The seven rooflight checks cover actual open shaft rays, glass plane/thickness/
normals, solid closure/volume, source-roof coverage/height, all three loading-gate
states and previous personnel/ramp/pawn paths, the opaque prior as a negative,
all three equivalent frame alternatives, rigid frame changes/metric UVs, holes,
read-only descriptors and capacity rejection before geometry. The combined
40-check run on `11b560f41` ended0. No `lib/` procedure changes in this prototype;
these are source-reference experiment checks, not a new installed-library or
native/device admission.

The probe exports three 2400×1350 plan/actual-section/2.5D diagrams and six paired
1920×1080 cameras: three whole halls, one opposite exterior, one interior and one
near roof. The interior camera must see actual mesh vertices in both products
and keep the eye in the owned inner human-clearance region. An initial centre-
ceiling view with no visible prior vertices was rejected; the camera gate was
preserved. Only the diagnostics select roof subsets; Blender uses the full
actual mesh and the original source palette/daylight/flat-no-DEM ground.

Actual local image delivery: 14 optically opened 1920×1080 Eevee images, 16 samples
each, plus the three optically opened diagrams. Geometry/render inputs are frozen
at `b2c942373` (six paired building cameras) and `e9af461fd` (paired full source
tile). All three final raster batches ended0 after successful rendering; their
small runners flush and call `os._exit(0)` after the domain function returns,
retaining the previously documented bpy teardown workaround. Backend:
llvmpipe LLVM 19.1.7, 256 bits. No Cycles/GPU/normal-teardown admission.
Six current glazed camera meshes exactly match the independently verified
assembly after the existing palette binding; all streams and dtypes are checked.
The `333c9342d` focused seven-check run and camera replay both ended0.

The entire Grohnde source tile retains 37 members with the same original source
attributes, identities, footprint WKB, height intervals, classes and palette.
Six constructed halls emit 148 fields; the eight logical plans contain 228 fields,
including deferred halls. The existing unsuitable loading entrance retains its
same rejection reason. Source omissions and construction coverage do not change.
The industrial-group close view uses 60123→72523 triangles; 7644/2073600 pixels
change (0.3686%, mean RGB error0.059852/255) under `test/scripts/pixels.py`.
The opposite roof hides most descending panes as expected. The interior witness
shows a real recessed shaft/rim/pane instead of the prior uniform opaque ceiling;
this is not a room-wide daylight-energy or nighttime-lighting validation.

Costs distinguish template-cold construction, 21 warm runs and traced peak for
the rooflight parts from the separately measured whole prepared-hall assembly.
Known transformed position/normal arrays, output payload/storage and Boolean
operand/output counts are explicit. Native allocator/solver internals and total
copy traffic remain unmeasured. Concurrent Software-Eevee/other domain work is
not an isolated hardware comparison; no runtime/frame-budget claim follows.

The `1f86c1d26` whole-assembly measurement used normal priority on a shared
four-logical-CPU host while Software-Eevee and other domain work ran. Reported
load was 15.36/13.31/11.65; the whole-process RSS high-water mark was 146236 KiB.
The measured phase starts from the same prepared hall/envelope and ends at the
joined render mesh. It excludes provider acquisition and initial plan/envelope
construction. Timings are milliseconds; traced peaks are bytes:

| Fields | Method | Warm median ms | Render triangles | Render bytes | Traced peak bytes |
|---|---|---|---|---|---|
|55|ring|453.290|11094|3084132|7379415|
|55|bars|478.065|11974|3328772|7895421|
|16|ring|272.640|4974|1382772|3498851|
|16|bars|222.035|5230|1453940|3650180|
|46|ring|452.085|10512|2922336|7060565|
|46|bars|447.008|11248|3126944|7490098|

Ring emission consistently saves field triangles/components and output bytes;
the noisy whole-phase CPU medians do **not** establish a consistent speed gain.
The three Boolean shells use 103/44/95 prism operands and 4166/1762/3880 output
triangles identically for both alternatives. CSG remains the expensive shared
work; direct compact/far products must avoid it rather than generate then strip
these bodies. The opaque prior has 6526/3634/6656 render triangles, so glazing is
additional quality/work, not a performance improvement against that prior.

The experimental optional-cut hook also replays all eight previous recipe-v1
render inputs exactly, including actual stream dtypes, cameras and source
palette. This is stronger than comparing two current paths to each other; the
unchanged original reference inputs/render receipts remain in system temp.

## References and admission boundary

[Instant Architecture, SIGGRAPH 2003](../../../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
page 4 optically inspected: split/replacement composition supports the chosen
construction, but supplies no industrial rooflight recipe. Existing Ghidra
exports of SDK3.4.12206 `Processor::extrude` (`004b5210`) and `subtract`
(`00541230`) from #489 remain hash-matched after reboot:
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
This reuses those bounded primitive/dispatch studies, not a newly exported
rooflight algorithm or ready Esri recipe/oracle render.

General default selection, direct compact/far glazing, per-field detail demand
and transition/shadow/radiance bounds remain open in #500. This prototype must
not be silently substituted for v1 under the same
operator fingerprint. A fully procedural city/architecture catalogue, final
night-light performance and native integration remain separate open goals.

Direct closed/checked-contact component construction now lives in the installable
reference module; source/CSG preparation is retained, emission avoids repeated
extrusion. Contract, counterexamples, scoped costs and image/replay evidence:
[lib/rooflight_panels.md](../../../lib/rooflight_panels.md). This adds no v1 recipe
semantics or automatic standard/LOD selection.

Explicit normal hall v2 and its loadable domain recipe now use those closed
components without prototype patches. Logical/full demand, retained products,
source/tile controls, actual image inputs and scoped costs are documented in
[recipe_rooflights.md](recipe_rooflights.md). Version1 stays the default; complete
direct far roofs and transition/night/native admission remain open.
