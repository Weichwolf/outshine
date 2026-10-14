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

```sh
.venv/bin/python test/experiments/buildings/showcase.py --publish --serve
```

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

The showcase reads the checked current snapshot and serves it on localhost:8766.
The latest user instruction replaces the earlier multi-page gallery: exactly four
current complete tile images, no diagnostic panels or subpages. New stands appear
on opening or via the manual refresh button; no polling timer runs. Detailed
controls/plots remain private experiment artifacts. Outputs remain under
`build/experiments/buildings/`; native runtime/device budgets remain unproven.

```sh
.venv/bin/python test/experiments/buildings/showcase_tiles.py
.venv/bin/python test/experiments/buildings/showcase.py --publish
.venv/bin/python test/experiments/buildings/showcase.py --serve
```

The publisher reads `tile-showcase/reports.json` directly and prunes obsolete public
images. It requires four clean render reports from the same source commit.

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

`showcase_tiles.py` chooses a near view from source envelopes before emission,
retaining the complete tile scene. Front-facing radial products use a conservative
minimum-depth 0.5-pixel error bound; behind-camera profiles stay complete at 0.25 m.
Four 2560×1440 physical CPU renders use generated geometry and PBR materials.
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
