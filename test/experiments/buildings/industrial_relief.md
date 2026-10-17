# Functional industrial facade relief

Owner: [#519](https://github.com/Weichwolf/outshine/issues/519), parent #32.
The geometric-omission selection and its pixel/count results below describe the
historical #519 control. [#523](industrial_relief_signal.md) replaces omission in
the ordinary Python path with shared front surfaces that preserve more distant
metal signal. Re-running this probe now uses that corrected default.
The prepared #500 halls had through-openings and shaped glazed roofs but flat
exterior walls. This experiment adds source-stable industrial bay hierarchies
using the existing facade/window axes, with real closed physical relief and a
shared exterior draw product. Source bodies, roof sections, openings, access,
room IDs, gate states and height intervals retain their existing programmes.

## Programme and geometry

Three procedural interpretations share the same planning mechanism:

| Interpretation | Existing-cell cadence | Pier width | Outward depth | Extra band |
|---|---:|---:|---:|---|
|portal-bays|1|0.30 m|0.20 m|plinth / upper closure|
|paired-bays|2|0.38 m|0.28 m|plinth / upper closure|
|ribbon-bays|3|0.24 m|0.14 m|plinth / upper closure / sill ribbon|

A stable source-ID digest chooses the interpretation; none is claimed as an OSM
observation. Piers follow existing cell boundaries. The local domain starts at
ground0, ends0.10m above the existing ceiling and stays below the existing eave.
Outer edges leave0.04m corner clearance. Existing window/casing/sill regions,
personnel doors and loading openings are subtracted with0.18/0.20/0.38m margins,
respectively. This includes the existing gate housing. No window or door grid
is regenerated or moved. The selected existing metal frame family is semantic
slot10; supplied wall colors and the shared material/shader API are unchanged.

Generated local detail regions use a10nm GEOS precision grid before union and
difference. This removes arithmetic-width slivers that would otherwise collapse
during triangulation; provider geometry and existing openings are never snapped.
The region then supplies both caps and boundary walls. Ring orientation is
explicit in both direct and general-Shape paths. Finite valid polygons with
at most32 existing cells per facade,32 facades and4096 full triangles per source
are required before mesh generation. The Euler count for each polygon is
`4*n + 4*h - 4`, with unique ring vertices `n` and hole count `h`.

`industrial_relief_plan` tests the outward support against the original source
wings and known height-overlapping neighbors using the existing entrance
clearance owner. Rejection is local to a facade and recorded. This is not a
new shared reservation contract: transport corridors, unknown neighbors and
vegetation remain unproved. The complete Grohnde tile yields39 accepted facade
regions and one known-neighbor rejection across eight source halls.

`industrial_relief_mesh` composes the existing library cap triangulation and
boundary-side extrusion, then supplies metric UVs and authored normals. The
complete closed physical mesh is immutable. Front/sides precede back triangles,
so the covered-back exterior uses read-only prefix views into those same
buffers. No second mesh construction or duplicate vertex storage is needed.
Actual source-wall rays verify that the backs meet opaque host surfaces.

`industrial_relief_products` prepares these parts once on the existing complete
hall assets. The historical #519 camera policy selected stored outer parts. A wall-projection
correspondence displaces each point by at most its outward depth; the existing
conservative projected-error policy permits omission only within the declared
geometry budget. Full physical parts remain available independently. Bounds
and geometry do not establish bounds on metal response, normals, shadows,
temporal switching or radiance. Final diagnostic whole-tile export still copies
source bodies; no native frame/cache/device result is implied.

## Checks and images

Seven feature controls cover all39 actual domains: exact aperture separation,
early capacity/invalid-type negatives, source-wing and independent neighbor
height negatives, shared immutable buffers, analytic volume and surface area,
metric UV edge lengths, normals and bidirectional native solid differences
against general Shape extrusion. All eight combined hall solids preserve
loading, personnel and internal supported routes at gate0/0.5/1 and personnel
0/90 degrees. Actual covered-back wall rays, camera-only prepared reuse and
complete37-source accounting are included.

At frozen `d86b3800f`, all18 affected source checks pass in50.705s: these seven
plus the existing prepared-hall/tile controls. The same seven feature controls
pass against the actual installed library in16.649s; all197 library modules
remain byte-exact to the pinned installed wheel. Normal v1/v2 paths and native/
shared interfaces are unchanged. Terminal commits/paths belong to the PR.
The ordinary old baseline is reused only after exact streams, dtypes, camera
and palette checks.

After the additive Transport #520 merge, `a47fe2d75` passes13 installed feature/
welding transition controls in13.567s; all198 modules are byte-exact to that
pinned current package. The actual entire tile also replays arrays/dtypes and
palette exactly against the frozen raster input. No new raster or old-library
equivalence is inferred merely from a clean merge.

The complete unchanged37-source tile uses84801 triangles before and88235 after
the selected relief (+3434,4.05%). Near examples have7672/11810/8574 triangles
including the existing complete hall products; the corresponding baselines
have7280/11094/8034. Three actual facade plan/physical-section/2.5D diagnostics
use1920×1080 pixels. Separate true Blender near/opposite/tile comparisons use
the exact generated source geometry, unchanged camera/light and existing
material families. No images are published and no server is added.

Seven new raster processes at `d86b3800f` ended0 and their images were optically
opened, together with four exactly replayed earlier baselines. Eevee16 samples,
llvmpipe LLVM19.1.7. The new hierarchy is visible in all three near cases, the
whole/opposite source view and the complete tile. The complete-tile before/after
changes18617/2073600 pixels. This is facade articulation, not an entire industrial
architecture family or CityEngine-level admission.

An additional controlled full-relief tile draws all39 facades (88649 triangles)
against the selected88235 product. Ground center/span, camera, light and material
definitions match. Omission changes215 distant pixels, worst channel44/255,
mean channel0.001162/255: the geometry budget alone does not preserve thin metal
signal. General radiance/shadow/temporal LOD admission remains open in #33.
Both full physical and complete outer draw data remain available to consumers.
Near before/after views use the inherited renderer's fitted flat plane; its span
grows0.366–0.837m with outward relief. Those are actual scene comparisons, not
strictly isolated pixel-error experiments. Complete-tile plane bounds are exact.

## Reproduction

Use `.venv` on a frozen clean implementation, with explicit system-temp outputs:

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/assets:test/experiments/buildings
.venv/bin/python -m unittest verify_industrial_relief verify_prepared_hall verify_prepared_hall_tile -q
.venv/bin/python test/experiments/buildings/industrial_relief_probe.py --output <system-temp>/models
.venv/bin/python test/experiments/buildings/industrial_relief_sections.py --output <system-temp>/sections
.venv/bin/python test/experiments/buildings/industrial_relief_costs.py --output <system-temp>/costs
.venv/bin/python test/experiments/buildings/blender_city.py <system-temp>/models/HallRelief1After-geometry.json --output <system-temp>/renders --samples 16
```

The existing local bpy teardown workaround flushes output and exits0 only after
`blender_city.render` returns successfully. This is not normal-teardown or GPU
admission. Use `test/scripts/pixels.py` for actual paired PNG comparisons.
The fixed original provider input and attribution are in
[`data/provider/grohnde/`](data/provider/grohnde/README.md); no private source
cache or internet dependency is needed for this experiment.

The cost probe compares direct construction and general Shape extrusion on the
same canonical source regions, closed-solid/UV contract and material family.
It separates original hall preparation and added relief preparation, first/21
subsequent calls, known final array storage, shared full/exterior buffers and
scoped traced peaks. Traced peaks exclude parts of the native heap; final byte
counts are not total traffic or all intermediate allocations/copies. Shared-host
CPU numbers cannot prove a native frame/device budget or a global optimum.

Frozen `d86b3800f`, normal priority on a shared four-logical-CPU host, with
nice10 raster/verification jobs concurrently active. Load averages were
12.35/12.00/11.38 at both samples. Median milliseconds / separate traced peak
bytes over21 subsequent calls:

| Actual region | Full / outer triangles | Direct ms / peak bytes | Shape ms / peak bytes |
|---|---:|---:|---:|
|paired-bays|52 / 38|1.528 / 39144|9.892 / 103426|
|portal-bays|252 / 178|4.707 / 157776|36.152 / 487738|
|ribbon-bays|92 / 66|3.944 / 68320|16.127 / 175442|

All39 prepared reliefs add256.120ms after the existing hall preparation and
retain1490080 unique geometry-array bytes for closed/exterior products together.
The three representative shared buffers occupy14456/70056/25576 bytes.
These results support the direct composition for these bounded domains; the
general Shape operator retains its broader attribute/provenance functionality.
All individual samples, first-call times, known final bytes and scoped peaks
are retained in the reproducible cost JSON.

## Reference and limits

SIGGRAPH2003 *Instant Architecture*, p4 Figures5–6, was optically inspected in
this work block: derive components within existing split axes. The local
SIGGRAPH2006 *Procedural Modeling of Buildings* and previous facade hierarchy
study supply alignment/occlusion context. Hash-bound SDK3.4.12206 Ghidra
`extrude`/`splitXYZ` observations from
[`facade-hierarchies-sdk-3.4.12206.md`](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md)
were reread, including `HoleRelation`, side-face UV generation and side tagging.
This is reuse of inspected primitive evidence, not a new decompilation, ready
Esri industrial recipe, oracle image or source-code equivalence.

These exterior programmes improve facade hierarchy while retaining simple
interiors. Complete architecture families, richer cladding/roof hardware and
mixed-height industrial complexes remain open. Existing gray surroundings and
coarse special structures still fall short of the overall quality target.
