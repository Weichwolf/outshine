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
PYTHONPATH=lib/src:test/experiments/buildings .venv/bin/python test/experiments/buildings/industrial_rooflight_probe.py --output /tmp/outshine-buildings-rooflights-output
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

Costs distinguish template-cold construction, 21 warm runs and traced peak for
the rooflight parts from the separately measured whole prepared-hall assembly.
Known transformed position/normal arrays, output payload/storage and Boolean
operand/output counts are explicit. Native allocator/solver internals and total
copy traffic remain unmeasured. Concurrent Software-Eevee/other domain work is
not an isolated hardware comparison; no runtime/frame-budget claim follows.

## References and admission boundary

[Instant Architecture, SIGGRAPH 2003](../../../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
page 4 optically inspected: split/replacement composition supports the chosen
construction, but supplies no industrial rooflight recipe. Existing Ghidra
exports of SDK3.4.12206 `Processor::extrude` (`004b5210`) and `subtract`
(`00541230`) from #489 remain hash-matched after reboot:
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
This reuses those bounded primitive/dispatch studies, not a newly exported
rooflight algorithm or ready Esri recipe/oracle render.

General default selection, direct compact/far glazing, per-field detail demand,
transition/shadow/radiance bounds and a versioned recipe-graph v2 remain open in
#500. This prototype must not be silently substituted for v1 under the same
operator fingerprint. A fully procedural city/architecture catalogue, final
night-light performance and native integration remain separate open goals.
