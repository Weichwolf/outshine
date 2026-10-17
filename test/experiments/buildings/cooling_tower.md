# Cooling-tower intake construction

Owner: [#526](https://github.com/Weichwolf/outshine/issues/526), parent #130.
The existing inferred cooling towers were uninterrupted radial shells down to
ground. This bounded recipe adds a visible open intake, crossed supports,
foundation, lower collector and upper rim. It uses the same provider outlines,
height intervals, classification and source identities. Construction dimensions
are procedural interpretation, not a structural survey or engineering analysis.

## Stable programme and geometry

`cooling_tower_plan` derives immutable named components from the existing complete
`RadialProfile`. The source remains intact. The shell begins at
`bottom + min(8, max(3, 0.055*height))`; lower-profile radii are interpolated from
the original profile. Shell thickness and the remaining original knots survive.
The two actual Grohnde sources are138m tall with base radii50.667/50.686m and
7.59m intake height. Both receive54 bays and108 crossed rectangular supports.

Foundation height is0.18m. Support feet begin0.09m above the source bottom,
inside that foundation; horizontal end faces avoid pushing tilted square ends
below ground. Support heads end0.20m below the shell's start. The collector
extends from0.50m below to0.10m above that start and encloses their ends.
Its radial section extends0.60m outside and1.00m inside the shell's lower
outer radius. The upper rim occupies the final0.80m of the source interval.
Every component remains inside the original inscribed footprint disk.

Each support is a sheared standard Trimesh box with0.65m horizontal section;
volume is exactly width² times its vertical span. General Shape extrusion is
the independent comparison. Both paths emit closed geometry, outward normals,
metric support UVs and the same wall material. Radial parts retain the existing
normalized radial UV convention. The source material/palette API is unchanged.

The four radial parts and108 supports give112 independent closed physical
components. Their actual native Boolean union is one connected solid. The draw
product shares all shell arrays except normals: interpolated original-profile
slopes replace visible horizontal shading bands, preserving positions, UVs and
materials. Cap normals remain sharp; authored wall normals stay in the actual
triangle hemisphere. The renderer appends components with hidden overlaps;
the independent collision-reference union removes those internal surfaces.

Direct detail generation uses the existing radial error policy from the same
logical programme. Collector error is capped at0.20m to retain its physical
support-head coverage even in the1m coarse product. All support heads are
independently clipped and checked inside the actual collector at0.05/0.20/1m.
Near and coarse unions remain connected. No general appearance/shadow bound
follows from radial geometric bounds, and no warm whole-asset cache is claimed.

The optional `tile_buildings.construct(radial_builder=refine)` experiment hook
changes only admitted cooling recipes. Other delivered families replay exactly.
Unfitting explicit cooling sources keep the original coarse radial body and a
recorded rejection reason. No source is discarded merely for not fitting this
recipe. Provider, classification, adjusted footprints, reservations, library,
native and material/shader contracts remain unchanged.

## Functional and visual checks

Eleven current controls cover the two actual sources, deterministic plans,
positive connected closed solids, beam volume, footprint/height preservation,
elevated source intervals, invalid/small/solid/rim-fit negatives, material/UV/
normal binding and independent general-Shape comparison for all216 supports.
Each bay admits a continuously swept0.70m-wide,2m-high capsule above the0.18m
foundation, from outside to inside. The previous shell blocks those same sweeps.
This proves the tested intake opening, not ground approach, a full controller,
stairs, maintenance equipment, complete interior routes or real structural loads.

Actual export controls retain industrial context in isolated views and use the
immutable prepared row snapshot. Whole exports preserve all37 sources without
omissions; the other35 source programmes keep their existing interpretation.
The established industrial export helper is shared rather than duplicated.

At frozen `f68d7c32e`,11 source controls passed32.534s and the same11 controls
passed against the installed reference package in26.865s, both exit0. The prior
`61b6bd7c2` integration passed25 affected source controls in74.477s; its retained
hall/material/whole-tile paths remain unchanged by the collector correction.
All198 installed library modules are byte-exact to the checked package, wheel
SHA-256 `9aaf486c0f5dc793fe0c8ce28c316156b5ea6997cab5db3d737a3445e49e0179`.

Two actual1920×1080 source-section/plan/support-projection diagrams were opened
before perspective admission. Four new actual Blender rasters at `f68d7c32e`
ended0 and were optically opened: whole tile, near, opposite and elevated view.
Four earlier actual baselines are reused only after exact mesh streams/dtypes,
camera, bindings, palette and image-hash checks. They retain their original
render commits; no new baseline raster is implied. Paired scene bounds are exact.

| Actual view | Previous triangles | Current triangles | Sources |
|---|---:|---:|---:|
|Complete original tile|88401|92369|37|
|Near / opposite / elevated tower|11312|14512|1|

The intake/support silhouette is visible in the complete tile and both close
directions; the elevated view shows the smooth shell and upper rim.
The actual whole-tile comparison changes95627/2073600 pixels; this quantifies
the intentional construction/shading change, not a quality score.
An earlier actual near counterexample exposed support ends protruding through the shell.
The collector and actual head-containment test correct that defect. This is
greater construction detail, not a radiance-error allowance or CityEngine-level
admission. Materials remain neutral diagnostics and the existing industrial
dome remains visibly banded. Further special families/equipment/access stay #130.

Backend: Eevee16 samples, software llvmpipe LLVM19.1.7, inherited post-success
bpy exit workaround. No normal-teardown, GPU, frame/A18, native integration or
full cache admission. No images are published and no showcase server is added.

## Equal-output construction costs

Frozen `f68d7c32e`, shared four-logical-CPU host, normal-priority measurements
with nice10 model/verification work and other branch jobs active. Load averages
10.61/10.47/10.66. First call,21 subsequent calls and a separate traced-peak call
compare the same112-component construction via standard boxes versus general
Shape extrusion. Support solid differences and metric UV requirements are
checked independently; UV chart orientation need not be byte-identical.

| Actual source | Triangles | Box median ms / peak bytes | Shape median ms / peak bytes | Unique final bytes |
|---|---:|---:|---:|---:|
|tower0|5552|69.558 /1809191|642.020 /1809319|1241280|
|tower1|5560|58.296 /1809191|513.531 /1809319|1242936|

Each result has672 final arrays. These are complete new component products,
not pointer selection from an existing asset. Planning/source decode, physical
Boolean union, smooth draw normals and final export copies are excluded. Scoped
tracemalloc peaks exclude parts of the native heap; final array counts/bytes are
not all allocations or memory traffic. No full-generator, cache, GPU or frame
speedup follows. The cheaper equal-contract box composition is retained.

## Reproduction and provenance

```sh
export PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings
.venv/bin/python -m unittest verify_cooling_tower -q
.venv/bin/python test/experiments/buildings/cooling_tower_sections.py --output <system-temp>/sections
.venv/bin/python test/experiments/buildings/cooling_tower_probe.py --output <system-temp>/models
.venv/bin/python test/experiments/buildings/cooling_tower_costs.py --output <system-temp>/costs
.venv/bin/python test/experiments/buildings/blender_city.py <system-temp>/models/CoolingNearAfter-geometry.json --output <system-temp>/renders --samples 16
```

Private final models/rasters/hash receipts are under
`/tmp/outshine-buildings-cooling-intake-collector-output/`; costs under
`/tmp/outshine-buildings-cooling-intake-collector-cost-output/`. Compare actual
PNGs using `test/scripts/pixels.py`. The versioned
[provider fixture](data/provider/grohnde/README.md) requires no internet.

[Instant Architecture, SIGGRAPH2003](../../../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
p.3/Figure4 optically inspected: attributed shape/layout-to-component hierarchy.
[Existing CGA/SDK reference](../../../doc/references/buildings/esri/special-structures-sdk-3.4.12206.md)
describes primitive composition, not a supplied cooling-tower recipe.
Fresh bounded Ghidra exports on SDK3.4.12206 ended0 in32.484s: cone `004bfa70`
and cylinder `004bff30`,132 instructions each, taper `0051f160`,67 instructions.
Minimum-side checks/error reporting and taper face handling are inspectable;
indirect calls, optimized control flow and exact implementation equivalence
remain unresolved. Core SHA-256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
Exports remain external under `/tmp/outshine-buildings-cooling-intake-esri-69f9b6bf2/`;
no foreign source/binary, ready recipe oracle or SDK runtime is bundled.
