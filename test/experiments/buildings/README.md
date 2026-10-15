# Buildings experiments

Run from the repository root with `.venv`. Install the reference library as well
as the experiment dependencies:

```sh
.venv/bin/python -m pip install -e ./lib
.venv/bin/python -m pip install -r test/experiments/buildings/requirements.txt
.venv/bin/python -m unittest discover -s lib/tests -v
.venv/bin/python test/experiments/buildings/verify_modelling_scope.py
.venv/bin/python test/experiments/buildings/verify_scope_library.py
```

The connected Shape/modelling core and CGA ↔ ENU conversion come from
`outshine_ref.buildings.modelling`. Static OBJ/glTF/GLB decoding additionally needs
`.venv/bin/python -m pip install -e './lib[assets]'`.
`verify_scope_library.py` compares all emitted streams on four saved control meshes
against the committed pre-migration implementation, then measures the same five
operations and emission. Saved inputs live under `build/experiments/buildings/`;
the comparison is offline and reports each input hash. It does not install or
import another checkout. CGA scope frames use east/up/south metres internally;
shared spatial products use east/north/up. The conversions preserve winding.

Building plans/recipes and comparisons still live here and are being
transferred separately. The installed modelling core does not provide the complete
CityEngine capability set or building generator. Issue [#51](https://github.com/Weichwolf/outshine/issues/51)
tracks that transfer; [#34](https://github.com/Weichwolf/outshine/issues/34) owns
modelling capabilities.

Experimental coplanar CSG support partitioning compares GEOS triangle/plane
regions with exact rational convex clipping and original-edge stations. The
`exact-rounded` alternative contracts only existing edges whose endpoints
convert to identical Float64 positions; disconnected contact fans stay separate.
The `exact-output` alternative additionally cancels opposite facets only when
their exact rational Float64 area is below the unchanged normal-resolution
bound, their logical root cycles are opposite, and their native triangle and
source supports match. The area bound includes the actual rational-to-Float64
coordinate errors of the constructed vertices: with rounded edges u/v and their
L1 error bounds du/dv, cross-product uncertainty adds
`du*|v|1 + dv*|u|1 + du*dv`. There is no global welding distance or small-area
cutoff. Fan orientation comes from exact construction, avoiding
rounding-induced reversals. Cancelled facet altitude is conservatively bounded
and reported. Unpaired, resolvable or differently supported facets remain. Directed
closure and all other output requirements still have to pass independently.
None is a general Shape Boolean API. Full checks keep directed closure,
resolvable normals, volume, surface, whole-region source support, UV0–9,
material priority and source tags. Rational construction alone does not prove
valid Float64 output. [#88](https://github.com/Weichwolf/outshine/issues/88) owns
the remaining rounding and general modelling contracts.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_boolean_exact.py
.venv/bin/python -W error test/experiments/buildings/verify_boolean_semantic_overlay.py --real --method exact
.venv/bin/python -W error test/experiments/buildings/verify_boolean_semantic_overlay.py --real --method exact-rounded
.venv/bin/python -W error test/experiments/buildings/verify_boolean_semantic_overlay.py --real --method exact-output
.venv/bin/python -W error test/experiments/buildings/verify_boolean_semantic_frames.py
```

`exact` and `exact-rounded` currently expect exit 1 and preserve rejected reports.
`exact-output` passes the 183-product prism comparison. The additional frame
comparison covers five controls in local, arbitrary remote common and independently
yawed operand frames, not general independently tilted solids. Per-coordinate
rounding error and cancelled facet counts are reported; no source is rewritten.
UV construction uses the dominant planar projection in extended precision;
its independent reference uses exact rational barycentric coordinates. This
replaces unstable Gram inversion in the verifier, preserving every tolerance.
Rational coordinate numerators/denominators are capped at 4096 bits; candidate,
region and triangle counts are bounded. There is no hard process deadline or
general memory bound, and source coplanarity selection remains floating point.
The experiment adopts exact construction from Zhou et al., SIGGRAPH 2016,
not their complete winding-number mesh arrangement or output repair algorithm.

`boolean_shape_probe.assemble` reconstructs logical source-aware regions from
shared native root edges with the existing polygon boundary recovery. Material,
group, tag and original source-face boundaries stay separate; native triangles
and all UV0–9 interpolation domains remain present. Logical rings share vertex
identities but carry their own corner UVs. The result keeps A's scope, writes
resolved operand materials into geometry and uses neutral root overrides.
Conflicting prepared CGA map names receive deterministic names; encoded texture
bytes stay shared, unaffected paths keep their prepared objects, and material
links are rewritten in their own resource context. Unprepared maps fail explicitly.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_boolean_shape.py --real
.venv/bin/python -W error test/experiments/buildings/verify_boolean_shape_controls.py
```

This is experimental Shape assembly, not the general Shape Boolean API. General
recursive geometry, imported indexed texture bindings, authored normals, surface cut-edge
components, independently tilted/open geometry and hard resource bounds remain
pending. Public component appearance already propagates correctly through
`Shape.comp`; the inner constructor alone is not its full inheritance contract.

`boolean_sequence` declares immutable leaf/operand/operation records with explicit
source names and original face/triangle references. Reusing a declared leaf is
valid; redeclaring its name inside one graph is rejected. Each output stores
complete leaf ancestry and the original preferred material/UV source, with its
immediate parents retained. Repeated Boolean operations replace reserved
`bool.A`, `bool.B` and `bool.cut` tags, as specified by SDK Auto-Tags; other tags
survive and prior tags remain available through the parent graph. Empty generated
intermediates obey union/difference/intersection identities.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_boolean_sequence.py
.venv/bin/python -W error test/experiments/buildings/verify_boolean_sequence_controls.py
.venv/bin/python -W error test/experiments/buildings/boolean_sequence_comparison.py
```

Both room/door expressions, `outer - (room union door)` and
`(outer - room) - door`, pass independent original-prism volume/surface, original
leaf UV0–9/material origins, current tags and actual mesh slices with a 0.6 m-wide
route. Conflicting prepared map paths also preserve original byte identities
through either chain. The comparison runs six alternating complete two-step
pipelines under those same quality requirements. Timing excludes independent
verification; peak RSS belongs to the combined process, not an individual
strategy. No generic recursive geometry/runtime guarantee follows from this
one workload; #109 owns this work.
`compare_boolean_output_repair.py --baseline-revision <commit>` additionally
compares eager versus candidate-first repair on the identical pre-repair arrays
from both expressions. It checks exact output roots/supports/native IDs before
reporting isolated kernel time. Source geometry capture and verification are
excluded from that timer; the historical own-source kernel stays in build output.

`boolean_building_plan` converts complete existing multi-storey plans into room,
door, window and stair-opening CSG sources. Balanced/sequential expressions are
retained comparisons, including rejected topology/candidate-capacity cases.
`boolean_building_bands` builds the same planned structure with the installed
`PrismVolume`/`union_prisms` library core. Original footprint coordinates remain
unchanged; structural wall bands are recomputed against that footprint.
Only the primitive constructor's planned source/material semantics are claimed,
not arbitrary inherited CGA Shape UVs/tags/material equality.

```sh
.venv/bin/python -W error -m unittest discover -s lib/tests -p test_prism_union.py
.venv/bin/python -W error test/experiments/buildings/verify_structural_shell_controls.py
.venv/bin/python -W error test/experiments/buildings/verify_structural_shell.py --compare-csg
.venv/bin/python -W error test/experiments/buildings/showcase_structural_shell.py
```

Validation uses independent original-prism volume/surface integrals and every
height slab. Slices come directly from Float64 native edges and explicit root
identities; the Manifold cross-section conversion loses precision for these
prism dimensions, so it is not the section verifier. Actual compiled assembly
rays check all door passages, sampled stair torsos and foot disks. Sealed
entrance/stair openings and missing flights are negative controls.
[#124](https://github.com/Weichwolf/outshine/issues/124) owns this structure; the
generic room programme, native player controller and architecture families remain
separate.

The Buildings gallery uses four fixed native Place locations/cameras:
DarmstadtWest, Flensburg, Rosenheim and Tokyo from `src/assets/places/`.
`doc/images/places/` supplies the comparison images only. Own current Python
building renders belong in `doc/images/buildings/`, at 1920×1080 or higher with
the same aspect ratio and vertical FOV; replace them after clearly visible gains.
Do not copy native images as Python results. The local server and its UI are retired.
Private source data and intermediate outputs stay in system-temp directories.

```sh
.venv/bin/python test/experiments/buildings/verify_place_reference.py
.venv/bin/python test/experiments/buildings/place_sources.py --fetch --output <temp>/source-receipts.json
.venv/bin/python test/experiments/buildings/render_places.py --sources <temp>/source-receipts.json --output <temp>/verified-place-renders
.venv/bin/python test/experiments/buildings/showcase.py --input <temp>/verified-place-renders
```

The publisher requires all four own clean render reports and matching scenario
camera hashes before replacing the gallery. A manifest may declare one `commit`
for the whole set or explicit `commits` keyed by all four Places, preserving each
image's actual generator snapshot during gradual verified updates. Tokyo's height requires actual DEM
sampling; the other three scenario heights are absolute. Source extent, missing
world classes and terrain/material/light differences remain explicit. The initial
source acquisition is a declared 3 km window, not native 240 km Place admission.

Small residential roofs are extended beyond the old 70 m² envelope scope in
`residential_roof_choice.py` (#229): 70–350 m², ordinary residential source or
site context, ground-level height 3.5–15 m and bounded footprint complexity.
Explicit roof choices and other uses retain their previous path. Source heights,
footprints and properties stay unchanged; roof space is reserved before rooms.
Supplied levels require at least 2.64 m each below the roof. This is a procedural
roof assumption, not a reconstructed roof or a complete architecture family.
`verify_residential_roofs.py` checks four pinned real Flensburg footprints against
independent solid composition, sections and topology plus semantic counterparts.
`residential_roof_probe.py --output <temp>/roof-controls` produces actual 1920×1080
plan/section/2.5D controls and equal-shape method timings. Native section comparison
uses millimetres internally to reduce measured quantization, retaining the existing
1e-7 m² threshold; an intentional 1 mm geometry shift must still fail.

## Sonderbauwerke aus dem bevorzugten Provider

`special_sources.py --fetch` samples named study windows against the endpoint
declared in `PreferredVectorSource.h`; pinned receipts are in
`doc/references/data/osm/special-structures-20261014.json`. Original bytes remain
in the client source cache. No additional OSM service supplies missing tags.
Names choose study locations only. `verify_special_structures.py` imports the
library classifier, records every decision/omission, preserves POI markers without
inventing dimensions, and checks known structural families plus building controls.
Development inspected all study sites; checks are regressions, not an untouched
statistical accuracy sample. `showcase_special_structures.py` plots actual
footprints, height regimes and the unresolved type distinctions at 2400×1400+.

`esri_special_reference.py` exports four bounded primitive functions from the
pinned external SDK via Ghidra. Foreign decompilations remain ignored Build output;
the local reference note records their scope. The installed SDK contains no CGA
rules or RPK recipe library. [#130](https://github.com/Weichwolf/outshine/issues/130)
owns this research; meshes, functional architecture and native LOD remain separate.

## Whole-tile radial prototypes

`tile_buildings.py` decodes every delivered building polygon in four pinned tiles
around Spittelau, Berlin Fernsehturm, Grohnde and Hameln, including provider buffer.
Invalid geometry/height and hide_3d are source-specific omissions; unresolved
colours use recorded procedural neutral values while original properties remain.
`special_forms.py` codes initial shaft/hollow-flue/hyperboloid/dome profiles from
classified source dimensions. Proportions are procedural, fine type/blueprint and
functional access remain unproven. Normal buildings remain supplied footprint prisms.

`showcase_tiles.py` fits every delivered building envelope from the complete tile before emission,
retaining the complete tile scene. Front-facing radial products use a conservative
minimum-depth 0.5-pixel error bound; behind-camera profiles stay complete at 0.25 m.
Four 3840×2160 physical CPU renders use generated geometry and PBR materials.
`radial_probe.py` compares own and Trimesh construction with the same binding work,
records the small-cap rejection and current-address-space Linux VmHWM. General
primitive contracts and independent volume/area/topology tests belong to #136;
full architecture families and native integration remain open.

## Complete room controls in provider tiles

`tile_room_details.py` selects at most sixteen visible ordinary near controls from
each otherwise complete tile scene. It reuses the actual generic room, portal,
storey, stair and framed-opening plan. Sampled 0.30-m-radius/1.90-m-height actors
test the assembled collision geometry before a detail replaces its source prism.
Unplanned or obstructed details retain that prism and record their reason. This
experiment limit is not a native world-class quota or runtime LOD admission.

`building_circulation.py` folds exactly equivalent strip/BSP partitions before
routing and uses remaining attempts for a simpler open floor. Landing ports on
long core sides connect the actual front landing; previous successful room/door/
stair geometry remains unchanged. The twelve-attempt limit is retained.
`verify_room_fallback.py` uses twelve pinned provider inputs, the explicit previous
source commit, independent volumes/every height slab and actor rays. Declared
CSG polygons remove implicit GEOS precision metadata while keeping the original
Float64 coordinate streams; native shape inputs cannot carry that hidden grid.
No section repair or relaxed area/actor bound is used. `room_fallback_plan.py`
draws the recovered source house's actual three mesh sections outside the page.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_room_fallback.py
.venv/bin/python test/experiments/buildings/room_fallback_plan.py
```

`building_vertical.py` keeps a connected single-storey room plan when the richer
core/corridor alternatives fail. The explicit `direct-rooms` mode reuses that
already built baseline; it invents neither a service core nor upper-floor access.
Common-access plans remain preferred. Door placement checks the whole 0.90-m
approach segment against the boundary of the two rooms and their actual opening,
with the same 0.30-m actor radius. Full-width backing lines alone are insufficient.

`verify_single_storey.py` checks seven recovered provider sources, thirteen previous
successful plans with byte-equal mesh streams, and actual multi-storey/undersized
controls. Independent shell sections, assembled actor rays, blocked entrances,
stable room-light states and unchanged zero-setback/terrace paths are required.
Build phases are measured separately; the previous unplanned result is not a
valid cheaper alternative. `single_storey_plan.py` draws actual mesh sections and
the obstructed/valid door approaches outside the four-image page. These generic
room controls are not complete architecture or use-programme families.
The clearance region uses only polygonal area: GEOS may also return zero-area
line fragments in a union, whose collection has no defined boundary. Discarding
those lines preserves every surface and avoids a NaN clearance without relaxing
the actor radius. The previous eleven-storey Tokyo control stays byte-equal.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_single_storey.py
.venv/bin/python test/experiments/buildings/single_storey_plan.py
```

[#142](https://github.com/Weichwolf/outshine/issues/142) owns this bounded recovery
and tile demonstration. Generic programmes do not count as complete architecture
families; external terrain/transport contact, native controller/cache/device
integration and city-wide complete room coverage remain open.

## Small-body roof envelopes in complete tiles

`tile_roof_envelopes.py` imports the general heightfield-envelope kernel and the
existing Shape shed/ridge operators. A stable scalar choice precedes geometry:
unclipped ordinary 10–70 m² bodies, 3.5–12 m height and sufficient width receive
an explicitly procedural small-body roof estimate. Other bodies and the larger
complete room controls retain their flat envelope. Explicit roof fields remain
preserved; unsupported supplied sloped-roof/room composition is labelled rather
than silently replaced by an inferred claim.

The full source height/footprint remains. A subpixel roof may directly use a
simple bound with the same logical choice, before constructing roof geometry;
this is a geometric experiment criterion, not native image/frame admission.
Actual construction failures retain the valid source prism and record their
cause. `verify_roof_envelopes.py` checks actual native solid topology/volume,
same-source independent body/roof composition and all relevant height sections.
Rejected comparison paths remain rejected and do not enter cost comparisons.
`roof_envelope_plan.py` draws actual upper surfaces and sections privately.
The public page continues to show exactly four complete source scenes.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_roof_envelopes.py --output "${TMPDIR:-/tmp}/outshine-buildings-roofs-$(git rev-parse --short HEAD)"
.venv/bin/python test/experiments/buildings/roof_envelope_plan.py
```

## Rooms below small source roofs

`pitched_room_details.py` uses the same stable roof choice before planning
storeys below the eave. It cuts declared room/door/window prisms from the closed
roof envelope through `subtract_prisms`, then reuses the existing panes,
metrical frames, entry landings and stairs. The complete source height, roof
domain and logical room/opening IDs remain; attic space is currently unfitted.
Failed room plans retain the complete roof envelope with a reason.

The tile experiment adds at most sixteen visible small-body controls per tile,
using the same industrial classification as the whole source builder. This is
a prototype sampling bound, not an engine quota or native LOD admission.
The larger established room controls remain unchanged. The private verifier
checks analytical lower structure plus original upper triangles/boundary
segments, independent body/roof composition, every planned height slab,
actual window rays to their declared rooms, frames/panes, and the unchanged
0.30 m radius/1.90 m height actor. Sealed entrances/windows and missing floors
must fail. Invalid independent compositions retain their full rejection.

All source and installed-wheel tests stay independent from native cache,
controller, frame and device gates. The public page keeps exactly four current
whole source scenes with every source envelope inside the image margins. Private
near-control views retain readable opening/room proof independently of this overview. [#156](https://github.com/Weichwolf/outshine/issues/156) owns this
roof/room connection; general use programmes and architecture families remain
in #31/#32, general Shape Boolean semantics in #88.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_pitched_rooms.py
.venv/bin/python test/experiments/buildings/pitched_room_plan.py
```

## Context-scoped whole-tile facades

`tile_facades.py` attaches the existing exact provider-context joins once,
retains unknown whole-building use, and uses the existing context recipe
parameters to prepare source-aligned facade axes. It decorates complete source
envelopes; actual room/control meshes bypass this placeholder. Geometry, source
height/footprint/colour and radial special structures remain unchanged.
The facade fields are procedural estimates, not a complete architecture family.

`FacadeRepeat` and `rectangle_coverage` in `lib` replace the duplicate facade
coverage implementation. `facade_extent` is an experiment-only per-vertex
domain binding; UV0 carries cell coordinates. `mitsuba_facade.py` blends live
wall PBR with a rough dielectric/absorbing-room approximation using analytic
coverage of the current camera's tangent-plane box. This explicit projection
is checked against independent finite-difference ray/plane intersections.
It is not Mitsuba path-ray differential filtering, EWA, actual rooms or a
native cache/shader ABI. Missing/zero/nonintegral bindings reject before render.

Unlit surface parameters remain camera-independent. Current camera/shading is
resolved after loading the saved mesh and scene; cached data contains no baked
lighting or acquired textures. Existing constructed near controls retain real
openings; general replacement by a coherent near programme stays open in #33.
Finite source domains, metric UVs, analytic/explicit area, wrong attribute
names, preserved sources and physical point/box controls are checked privately.
The public page still contains exactly four current complete 3840×2160 tiles.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_tile_facades.py
.venv/bin/python test/experiments/buildings/facade_filter_plan.py
.venv/bin/python test/experiments/buildings/facade_physical_controls.py
```

## Source-derived stepped controls

`stepped_form_plan.py` compares at most thirty whole-storey alternatives:
all-side erosion and cuts along four directed source axes, with one or two
upper setbacks. The existing rooms, core, common access and stable IDs remain.
Every positive terrace component needs its own actual door; opening frames
must leave the connected 0.30 m actor clearance. Invalid alternatives retain
the previous valid body and an explicit reason. Numeric geometry failures
reject locally. There is no new source provider or use-class inference.
The choice also audits the complete far union and compiled near actor paths;
failed products try the next already planned alternative before fallback.

`stepped_source.py` reserves the roof parapet within the source height before
room planning. Its distant envelope uses the installed `union_prisms` directly
from the same contours, without building room geometry. Layer interfaces are
the actual indoor **floor** heights, including the exposed terrace plates.
Brüstungen und Körper werden zu einer Außenhaut vereinigt; ihre innenliegenden
Kontaktkappen verschwinden. Unabhängige Prismenintegration prüft Volumen,
gesamte Außenfläche und jeden Höhenabschnitt; getrennte Kontaktkappen scheitern
als Gegenfall trotz passendem Volumen.
Near assembly contains the existing room walls, slabs, stairs, glazing,
frames, terrace decks and guards. Actual compiled solid rays check access.
`stepped_openings.py` supplies two triangles per distant opening with the same
window/entrance/terrace and room IDs; its 1 mm exterior surface offset and
rough dielectric/absorbing-room estimate are explicit approximations.
Clipped rooms may contain zero-area boundary lines. Removing only these
pieces preserves their exact polygon area; no positive room fragment is lost.
The existing wing control consequently admits one room-safe inset but retains
the explicit missing terrace access. It remains ineligible for a complete
stepped prototype. The courtyard and narrow controls keep their rejections.

The stricter `layered_envelope.py` alternative remains an experiment: analytic
controls compare directed closure, volume **and** area, source/role/material,
metric UV0, holes, remote translation and replay. Source clipping can leave
floating point support differences that it rejects. Its direct triangulation
was slower and emitted more faces on the same analytic control than the
existing prism union; it is not installed or advertised as a faster kernel.
`verify_stepped_source.py` records the same-input comparison and every actual
source rejection. Missing decks/guards, sealed portals, closed stair openings,
disconnected terrace graphs and lowered far terraces must fail independently.

Private source plans and physical 2.5D/opposite/terrace/night controls keep
readable detail separate from the public page. `tile_stepped.py` admits at most
eight verified prototype plans per tile; this is a research selection bound,
not a native quota. The public page retains **exactly four complete current
3840×2160 source tiles**, the existing camera/light and all source accounting.
These remain generic room programmes, zero complete architecture families;
continuous native players, coherent general LOD/cache and device gates stay
at #31/#32/#33/#48. The bounded form contract belongs to
[#166](https://github.com/Weichwolf/outshine/issues/166).

```sh
.venv/bin/python -W error test/experiments/buildings/verify_layered_envelope.py
.venv/bin/python -W error test/experiments/buildings/verify_stepped_source.py
.venv/bin/python test/experiments/buildings/stepped_source_plan.py
.venv/bin/python test/experiments/buildings/stepped_source_probe.py --spp 32
.venv/bin/python test/experiments/buildings/showcase_tiles.py --spp 32
```

## Source-scoped room programmes

`room_programme.py` binds one bounded office furnishing programme to the existing
source room/storey/door identities. An actual office attribute can cover the
building; a uniquely hosted office POI supplies local use evidence only. Missing
levels explicitly assume a candidate ground floor, never a supplied tenant
boundary. Known levels outside the available storeys stay unresolved. Other
storeys, common access, stairs and service reservations remain unchanged.

Workstations, storage, meeting groups, tea counters, WC and basin use owned
procedural geometry with matching footprint/height bounds. Full activity areas
and actor routes are planned by `outshine_ref.buildings.interiors`; the original
circulation experiments import the same installed routing kernel. Wall and
deterministically jittered candidates, greedy and bounded beam search compare
identical actual rooms and required functions. No unbounded search or global
optimum claim. Missing furniture does not turn into a completed programme.

`verify_furniture.py` compiles actual furniture with the existing structural
shell, frames and stairs. All original access paths and every activity route
retain 0.30 m radius/1.90 m height. Corrupted actual object heights, a shelf
blocking the entrance, missing sanitary function, weakened requirements and
unsupported source floors must fail. `furniture_benchmark.py` separates source
preparation from layout and reports fresh-process peak RSS. `room_programme_plan`
and `room_programme_probe` produce private 2D, 2.5D, indoor day/night and exterior
window controls; the interior camera uses an actual checked room-side anchor.

These are furnishing controls, **zero complete architecture recipe families**.
Oversized legacy rooms, plumbing, ventilation, occupancy/accessibility rules,
other programme families, seated interactions and native detail admission remain
open at [#174](https://github.com/Weichwolf/outshine/issues/174), #31/#32/#33/#48.
The public gallery retains four fixed Place views. Invisible far furniture is
not generated for those views.

`inspect_render_exposure.py <render-report.json> --exposure 8` derives a private
display inspection from the stored linear denoised radiance. Its provenance
retains the original tracing commit and pixel buffers; the explicitly changed
exposure does not alter geometry, lights or the fixed day/night comparison.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_furniture_controls.py
.venv/bin/python -W error test/experiments/buildings/verify_furniture.py
.venv/bin/python test/experiments/buildings/furniture_benchmark.py --repeats 7
.venv/bin/python test/experiments/buildings/room_programme_plan.py
.venv/bin/python test/experiments/buildings/room_programme_probe.py --spp 32
```


## Compact office support room plans

`office_support_plan.py` refines only the actual office scope already established
by `room_programme.office_scope`. Source envelopes, existing doors/windows,
common corridors, stairs and unsupported storeys remain intact. Original room
IDs remain as the identifiable parents of appended support-room IDs. Compact
sanitary rooms and separate tea rooms receive their own full-width door from the
existing corridor; neither is a route through another work/support function.

`office_support_geometry.py` compares wall-end/centre placements with seven
sliding wall positions under the same whole-polygon, retained-approach, window
and 0.30 m actor requirements. At most 48 candidates per room, 24 spaces per
floor and four intermediate floor states. More candidates do not guarantee a
better bounded result: the current sliding control misses Hameln's tea room.
The successful wall-end controls remain research plans, not optimal or measured
real interiors. Compact support rooms use deterministic unperturbed furniture
wall anchors: replacing these by random inward shifts can remove every fitting
tea-counter/basin candidate. Each room records its actual placement method;
work/meeting rooms retain the requested method. The actual mesh gate requires
all added support functions, even when another original room stays incomplete. No training corpus or learned architectural model is claimed.

`verify_office_support_controls.py` exercises notches, full-width backing,
retained doors, clear sanitary windows, rigid transforms and capacity.
`office_support_verification.py` independently checks source/room ancestry,
unchanged openings, floor/wall accounting, compact areas and full-radius access.
`verify_office_support.py` adds actual furniture meshes, original building paths
and deliberately removed/escaped/misassigned/unsupported-floor counterparts.

`office_support_benchmark.py` times subdivision alone with equal compact-room
constraints, deterministic replay and independent pre/post admission; source
preparation and whole fresh-process Linux RSS stay separate.
`office_support_probe.py` writes private 2880×2160 before/after plans.
`room_programme_probe.py --support-plan --overview-only` uses the chosen plan,
then renders a declared ground-office cut: actual body parts capped at 1.10 m
above that floor, upper storeys/ceiling omitted, full furniture retained. The
uncut mesh is checked separately. The public gallery retains four fixed Place views.
Door leaves/privacy states, services/ventilation, complete architectural
programmes, other uses, runtime LOD/cache and native admission remain open.
