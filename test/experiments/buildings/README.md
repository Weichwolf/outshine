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
`outshine_ref.buildings.modelling`. Static OBJ decoding additionally needs
`.venv/bin/python -m pip install -e './lib[assets]'`.
Owned OBJ/material controls and native geometry channels are independent of
optional SDK or file-format oracles. Generate their private outputs explicitly:

```sh
.venv/bin/python test/experiments/buildings/verify_modelling_asset_load.py --files /tmp/outshine-buildings-obj-controls --output /tmp/outshine-buildings-obj-check.json
.venv/bin/python test/experiments/buildings/verify_modelling_material.py --output /tmp/outshine-buildings-material-check.json
.venv/bin/python test/experiments/buildings/modelling_asset_probe.py --output /tmp/outshine-buildings-obj-reuse.json
```

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

Current visual priority is exterior architecture: varied building forms,
facades, roofs and special structures at CityEngine quality, owned by
[#8](https://github.com/Weichwolf/outshine/issues/8) and
[#32](https://github.com/Weichwolf/outshine/issues/32). Simple interior depth
and believable window lighting suffice for this stage. Detailed room programmes
and furnishing remain available components; their further expansion follows
the exterior quality goal.

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

Current compact city studies use Paris, Rome, Berlin and London. The actual
preferred-provider census compares nine 800×800-metre windows in each of five
cities before selection. Density and area/height/shape diversity are explicit;
nearby POIs describe context rather than proving whole-building functions.
Full selected source polygons and source heights remain, with clipped census
pieces and generation failures recorded. This is a local study, not a global
optimum or native world-coverage gate.

Urban roof studies ([#308](https://github.com/Weichwolf/outshine/issues/308)) connect
the existing `Shape.roof_hip`/heightfield-envelope kernels to ordinary complete
150–2500 m², 8–36 m bodies beyond the existing small-house scope. Courtyards and
source top heights remain; supplied roof fields, clipped/elevated/round/slender
bodies and incompatible uses keep their previous alternatives. Roof form is
explicitly inferred, without city/name lookup. Delivered storeys reserve at least
2.64 m each before roof space. A bounded sub-0.1-micrometre height normalization
absorbs skeleton/overlay rounding, with normals transformed consistently.
Rectangle volume and concave/courtyard offset sections are independent controls;
three provider fixtures also exercise actual facade frames ending at the eave.
`urban_roof_probe.py --output <temp>/urban-roofs` records actual plan/section/2.5D
geometry and seven warm construction runs against the previous flat envelope.
The flat comparison measures incremental work for different visible quality.
Output buffer bytes are not total allocations; native/device/LOD budgets remain
unproved. The broader city-envelope scope test now uses a 3600 m² exclusion;
400 m² bodies are intentionally inside the new urban scope.

Buildings stand on one flat plane without DEM. `city_scene.py` uses the existing
Python roof, facade, stepped-envelope and simple room-control procedures.
`blender_city.py` consumes the exact frozen mesh and supplied normals/UVs with
shared Blender camera/import helpers, procedural facade masks and Principled
material parameter composition. Its Eevee sampling is not an analytic CPU
footprint oracle. The shared research workflow and fixed daylight values belong
to [#291](https://github.com/Weichwolf/outshine/issues/291).
Buildings now follows Transport's verified local normal-material path: supplied
point normals become an exact float32 CORNER attribute, transformed as NORMAL
from object to world and normalized before Principled. Blenders packed custom
normals are recorded for comparison and are not the intended material input.
`verify_city_normals.py` exercises an actual Eevee pixel with identity and
nonuniform object scaling. No new shared Blender or native API is required.

Ordinary ground-floor sources can now use the same existing entrance selector
as the first-storey room experiments (#358), restricted to existing facade cells.
`facade_entrances.py` records a procedural connected arrival-space assumption,
known neighbor-free landing, stable ID and cell. `tile_facade_recess.py` excludes
that one cell from the same window repeat, preserving other UV axes; the near
cell uses `OpeningPanel`, existing casing/leaf and simple floor/ceiling/landing.
The entrance-facade plinth is omitted so it cannot block the door. Complete room
controls are untouched; their later transition, upper access, outside routes,
night/radiance/LOD and native player/cache integration remain unproved.
`entrance_probe.py --output <temp>` writes actual same-camera before/closed/open,
side/overview/compact products, cuts/2.5D and direct-panel/CSG comparison. Render
reports with `blender_city.py`; `verify_facade_entrances.py` checks actual sources
and continuous compiled arrival-tube clearance. Core contract: lib/entrances.md.

Near ordinary facades also have procedural plinths, selected storey belts and
stepped cornices through `tile_facade_bands.py` / the installable
`BandPlan`/`band_surface` (#342). Existing facade-repeat gaps determine heights;
the original wall/roof/opening/source arrays remain intact. Height-aware source
neighbor checks exclude colliding edge supports; remaining runs have closed caps.
Special/clipped/unsupported sources keep their prior representation. These
outward trims are inferred, not supplied footprints or complete architecture.
The positional projection bound leaves distant products compact, but is no
radiance/shadow/night/transition admission. Physical entrances and transport
reservation attachment remain separate work. `band_probe.py --output <temp>`
writes actual source plan/cross-section/2.5D, same-camera before/near/opposite/far
products and equal-array scalar/batch timings. Render its geometry reports with
`blender_city.py`; research images remain local. `verify_facade_bands.py` checks
the actual source path; independent solid controls are in
`lib/tests/test_profiled_bands.py`. The recess-only probe deliberately suppresses
the new bands to retain its earlier single-feature comparison.

Near ordinary facades additionally reuse the old real-recess prototype through
the installable `RecessPlan`/`recess_surface` library (#318). The same existing
facade axes control openings; 0.18 m inward recesses and 0.06 m frames are generated
only after visibility/detail demand and capacity checks. Thin/irregular boundaries
keep the original painted wall; constructive room/door controls keep their own
shared plans. Plain source wall materials cover return faces, and the existing
physical frame component10 covers frames; its response differs from the older
flat repeat's frame response. No shader or native API is changed.
`recess_probe.py --output <temp>/recesses` writes plan/section/2.5D evidence, equal
scalar/batch construction measurements and Flat/Near/Opposite/Far geometry reports
from an actual provider building. Render those reports with `blender_city.py`.
Far demand keeps the compact input without expanding cells or allocating a new
material. This is no new room programme or complete architecture-family claim.

Research images stay local in system-temp directories, normally 1920×1080 for
detail inspection. Only master publishes integrated Places in `doc/images/places/`;
no Buildings gallery or substitute publication is created. Source geometries,
materials and cameras remain distinguishable from image-renderer changes. The
old local server is retired; the legacy Place exporter requires an explicit temp
destination.

```sh
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/city_survey.py --fetch --output <temp>/census
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/city_scene.py --census <temp>/census/city-survey.json --output <temp>/city-meshes
PYTHONPATH=lib/src LP_NUM_THREADS=2 .venv/bin/python test/experiments/buildings/blender_city.py <temp>/city-meshes/Rom-geometry.json --output <temp>/city-renders
PYTHONPATH=lib/src .venv/bin/python -m unittest discover -s lib/tests -p 'test_blender_*.py'
```

Filtered facade finishes (#245) reuse the existing opening repeat. The reference
adds metric inner frames and opaque floor bands; near-detail geometry and direct
opening patches keep their existing surface path. Wall/frame/glass/band areas
come from the installable `outshine_ref.buildings.facade_finish` library. Explicit
rectangle integration and physical mesh rays independently check coverage and
the actual UV-to-metre binding. This adds no opening, collision or room programme.

```sh
PYTHONPATH=lib/src .venv/bin/python -m unittest discover -s lib/tests -p 'test_facade_finish.py'
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/verify_facade_finish_shader.py
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/verify_mitsuba_sampling.py
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/facade_footprint_probe.py --output /tmp/outshine-buildings-facade-footprints
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/facade_finish_probe.py --output /tmp/outshine-buildings-facade-controls
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/rerender_places.py <temp>/Flensburg-report.json --output /tmp/outshine-buildings-facade-places --samples-per-pass 4
```

The cached render path checks the original geometry report/mesh hash and actual
projection/triangle count. It performs no provider decode or mesh construction.
Geometry and renderer commits are recorded separately; the four-image publisher
preserves both. This is a whole-scene experiment cache, not native per-asset cache
or runtime admission. Source geometry must remain available at the checked path.
The optional `--samples-per-pass` bounds concurrent CPU sample work, retaining
total spp, scene and resolution. Tokyo's initial renderer exceeded this 16 GiB
host's memory (kernel OOM, 11.36 GiB process anonymous RSS); a four-sample limit
alone did not resolve it. Mitsuba still supports its deprecated explicit pass
limit; the report records it separately from the dispatch fix below.
Pass grouping can change random sample ordering, so pixel identity is not claimed.
Energy, full image extent, total spp and named denoising AOVs have a separate control.

The facade filter is exact for its axis-aligned UV box. That box bounds the
projected pixel parallelogram; it is an approximation for rotated/sheared or
rank-deficient footprints, not EWA or an exact projected-pixel integral. The
native material composition/filter owner is #189; transmission and lighting
must not acquire a second illumination path during integration.
The footprint probe checks 216 rotated/thin/sheared nonzero-area pixels at finite
domain edges against independent exact polygon intersections. Sign reversal and
derivative exchange preserve results, axis-aligned pixels recover the box
integral, and singular pixels are explicitly excluded from the area oracle.
It reports the approximation error rather than asserting projected-pixel accuracy.

CPU facade fields read constant per-source-triangle domain extents directly.
This avoids nesting virtual shape-attribute lookup inside each material branch;
source vertex attributes, triangles and camera stay unchanged. A varying extent
within one triangle is rejected rather than silently changing interpolation.
Controls exercise real bound BSDFs, reversed primitive order and a null shape
pointer; direct opening patches retain the prior path. `facade_palette_probe.py`
isolates this dispatch choice with a supplied real palette and small repeated
control mesh (64×36/1spp radiance arrays, never a published Place image):

```sh
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/facade_palette_probe.py --scene <temp>/Tokyo-scene.json --mesh <temp>/facade-finish-mesh.npz --count 32 --lookup shape --output /tmp/outshine-buildings-palette-shape
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/facade_palette_probe.py --scene <temp>/Tokyo-scene.json --mesh <temp>/facade-finish-mesh.npz --count 32 --lookup triangle --output /tmp/outshine-buildings-palette-triangle
```

The `shape` option exists only in the diagnostic. Captured JIT operation counts
cover setup and rendering together, not retired CPU instructions or GPU work.
The native owner composes linear material parameters before one illumination;
the explicit CPU BRDF mixture is an independent surface reference.

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

## Support doors and furniture

Joint support doors and furnishing (#197) use immutable metric leaf plans from
`outshine_ref.buildings.interiors.doors`. The library's `placement_reserved`
keeps furniture and activity zones out of the whole movement envelope; routing
uses the actual parked leaf separately. `office_support_plan.refine(...,
door_aware=True)` rejects otherwise compact partitions that cannot accommodate
the unchanged full-size functions and either inward hinge. It tries at most24
door layouts per partition step; no state/camera/LOD transition replans rooms.
Original source boundaries, old openings and unsupported floors remain intact.
The old fixed partitions and post-fit selection remain diagnostic counterparts;
an unfit fixed room remains explicitly partial rather than being resized in place.

```sh
PYTHONPATH=lib/src .venv/bin/python -m unittest discover -s lib/tests -p 'test_door_geometry.py'
PYTHONPATH=lib/src .venv/bin/python -m unittest discover -s lib/tests -p 'test_furnishing_placement.py'
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/verify_door_joint.py --output /tmp/outshine-buildings-door-joint/verification.json
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/door_joint_plan.py --output /tmp/outshine-buildings-door-joint/plan.png
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/door_joint_benchmark.py --output /tmp/outshine-buildings-door-joint/cost.json
PYTHONPATH=lib/src .venv/bin/python test/experiments/buildings/door_probe.py --output /tmp/outshine-buildings-door-joint/views --render
```

The paired full-building Berlin tea-door views use one fixed corridor camera,
actual geometry/furniture/area lights and 1920×1080/32spp CPU path tracing.
Ceiling fixtures near circulation portals supplement the single room-centre
fixture, with physical square containment and stable portal-bound IDs. They
are independent of the camera/leaf angle, share the room's light state and
are prepared once for the paired views. The default room-light API is unchanged;
the optional `circulation_portals=True` adds at most32 fixtures per corridor.
`door_assembly.prepare` similarly retains static frames and local leaf templates;
state expansion validates their fixed leaf identity and rebuilds neither.
They are private controls. Public presentation remains the four latest complete
Place images; a hidden interior change alone does not justify replacing them.
Whole-building usage, native controller/cache/LOD, handles and services remain open.

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
.venv/bin/python test/experiments/buildings/roof_envelope_plan.py --output "${TMPDIR:-/tmp}/outshine-buildings-roofs-$(git rev-parse --short HEAD)"
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

## Source-bound ground-floor display fronts (#377)

`facade_shopfronts` uses only actual unique-host retail/food POIs, retaining evidence IDs and source attributes. Known non-ground/ambiguous levels and invalid hosts do not become ground fronts. A missing level is a marked procedural ground assumption. The selected existing entrance facade is a procedural frontage interpretation, not measured shop frontage or whole-building use. At most64 use candidates,256 existing columns and32 ground cells; each POI affects at most three existing adjacent cells, with the entrance cell excluded. Narrow/low/neighbor-blocked cells keep their prior surface. Existing constructed room controls stay separate.

`tile_shopfronts` consumes the same `FacadeRepeat`/`RecessPlan`, with wider/taller ground openings (retail .88×.80, food .82×.74 of the same cells). Upper repeats/UV scale and entrance identity are retained. Rectangular wall remainders use disjoint rectangles; rising walls use the bounded existing source-triangle difference. Compact cells bind the same proportions on two triangles, demanded near cells use the existing inward .18m recess. These are render display fronts, not complete physical shops or a night-/LOD/native admission.

Run with `.venv`: `PYTHONPATH=lib/src:test/experiments/buildings .venv/bin/python -m unittest verify_shopfronts -q`; `shopfront_probe.py --output /tmp/outshine-buildings-shopfronts-<commit>` generates actual provider before/after/overview/compact Blender inputs, a plan/2.5D diagnostic and a 21-run equal-surface rectangular-versus-GEOS remainder comparison. Render the geometry JSONs with the shared `blender_city.py`; images remain local. `data/shopfronts.json` preserves three actual Paris host/context examples and provider receipts, including supplied/missing ground levels. Whole-city neighbor/context coverage must be checked separately with `city_scene.py`.

References: local SIGGRAPH2006 p3/Fig4 (optically reviewed) separates floor programmes while retaining repeat axes; Candler `Base/GroundSubTile/GroundfloorOpening` treats street ground floors separately. Existing hash-checked Ghidra extrusion dispatcher exports support that primitive, not a complete Esri shop-front oracle. No foreign assets or texture images are used.

## Usable upper residential balcony cells (#388)

`facade_balconies` interprets ordinary residential sources as at most two vertical stacks on the existing entrance facade, at most three upper levels each. Source outlines/heights/roof, ground entrance and shopfront cells stay present. Stable existing cell/portal IDs and explicit floor datums are independent of camera and angle state. New outside footprints and height intervals are procedural asset additions, never rewritten source footprints. Whole wall cells/pockets must fit, self-wings must stay free and known neighboring height intervals are excluded. An additional1.2m opposing-deck margin conservatively protects against another procedural deck; it is not a transport/vegetation reservation admission. Existing complete room/stepped controls stay unchanged.

`tile_upper_cells` composes these cells with the existing shopfront/entrance and window kernels; only the selected upper cells change to shared through-door panels. `facade_cells.cell_complement` retains every other existing grid cell exactly once; rising walls retain the bounded source-triangle difference. `facade_entry_geometry.doorway` is the same panel/leaf/frame at an explicit level base, and earlier entry products keep base0. A deck/U-guard template is built only on demand and shared across levels of equal dimensions. Compact mode uses an opaque door and either the closed deck/guard or the separately bounded render plates described below; near mode has the actual wall passage plus a simple supported arrival pocket/ceiling. Below0.5px projected width it defers geometry;8px selects near. Inside the pocket/deck the bundle stays present when looking away. These are research choices, not admitted shadow/radiance/LOD/native budgets.

`balcony_surface` compares40-triangle explicit exterior polygons, existing four-prism union and separate closed parts. The direct boundary contains no invisible internal contacts; scalar dimensions and cardinality are checked before geometry. No global optimum or full backend memory limit is claimed. The cost probe checks the same solid and external role surfaces on three sizes,21 warm runs each, traced peak/output and expanded-render position bytes; native backend allocations/copies stay unknown.

Run with `.venv`: `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings .venv/bin/python -m unittest test_balconies test_facade_cells verify_balcony_cells -q`. `balcony_probe.py --output <temp>` generates actual provider before/closed/open/whole/opposite/compact Blender inputs and actual plan/section/2.5D diagnostics. Only the diagnostic hides horizontal plates for inspection; actor/guard checks consume complete meshes. Closed door, missing deck, wrong floor datum and missing guard fail; known neighbor/opposing margin/self-wing and nonresidential inputs have counterexamples. Local continuous arrival is not a complete upper-storey/stair/room/native-controller proof. Older entry/shopfront probes suppress this newer feature to retain their isolated comparisons.

SIGGRAPH2006 pp4–5/Figs8–11 (optically reviewed): compose volumetric parts, retain facade scopes and reject occluded placements. Existing local SDK/Ghidra extrusion exports provide primitive evidence; no dedicated Esri balcony oracle or copied code/assets is claimed. Reused earlier balcony/terrace controls provide context; detailed room programmes remain deferred behind exterior quality. Research pictures stay in system temp; native integration remains with master.

The first whole-city balcony pass retained40-triangle solids even in compact products and increased the Paris study from256667 to318371triangles. The actual consumer therefore additionally uses `balcony_plates`:16 render-only triangles for two deck surfaces and six guard faces. Missing slab skirts map along Z by at most slab/2; missing guard caps/ends and added deck-contact faces map along U/outward by at most guard thickness. Both displacement signs on all three axes are bounded with the existing directional projection function over the complete conservative bounding box and camera movement; plates are selected only below the unchanged pixel-error threshold. This is a geometry-position bound, not a radiance/shadow/topology admission. Near/inside uses the unchanged closed40-triangle solid and actual passage; logical dimensions/IDs are shared. These open render plates are never used by actor/guard collision checks.


## Source-aligned glazed window-bay programmes (#408)

`WindowBay` in the installed `buildings.window_bays` module describes an open-backed rectangular or chamfered projection: floor/ceiling, a U-shaped sill/head/frame and three thin closed glazing prisms. Local ENU uses negative Y outside and Z up, with the back at positive Y. Roles0/1/2/3 denote wall, floor, ceiling and glass. `bay_surface(method="parts")` returns individually closed components with explicit per-triangle `parts`; retain those components for collision. Merging coincident contacts by position alone is not a valid replacement for their identity. The existing prism union is a control only: some chamfered outputs lose volume when their mesh is re-imported, and `method="union"` explicitly rejects such roundtrips instead of returning a false physical reference. Input/output cardinality limits are not backend time/memory bounds.

`bay_skin` supplies86 external render triangles. Outer glazing, reveals and opaque surfaces coincide with the physical components; inner walls/floors/back surfaces are omitted. This is an open render product, never collision and never a radiance/shadow/general-visibility admission. Near/inside retains the closed physical components and actual passage. Sampled exterior role/normal/position controls cover both shapes and depths; they do not prove complete shaded-image equivalence.

`upper_facade_cells.propose` is the existing bounded residential placement shared with balconies: same entrance frame, stable grid, source fit, self-wing/context tree and conservative1.2m opposing-projection margin. Balcony limits/behaviour remain unchanged. Bay programmes use one vertical stack over all upper storeys of sources with at most12rows, at most11cells, with full storey-height intervals and0.90m depth. The stable source seed chooses rectangular/chamfered/balcony preference; a rejected bay programme can retain the earlier balcony programme. This is a marked procedural interpretation, never a supplied architectural style. `tile_upper_cells` composes either programme with existing entry/shop/window cells, reusing sparse complements and rising-wall clipping. No duplicate grid/tree/room planner or new native/shared reservation contract.

`window_bay_geometry` binds displayed panes through the same procedural full-glass facade callback as existing windows (the raw physical component role remains3); the early unbound transmission render appeared almost white and is a rejected picture control. It cuts the existing full wall cell using the installed `OpeningPanel`, joins the projection floor/ceiling to a simple inside pocket without duplicate slab area, and retains source/roof/ground-floor fields. `verify_window_bays` checks two actual source programmes, actual other-cell/ground-floor material/depth/metric-UV fields and a supported1.85m path with0.30m torso/0.10m foot radii. Closed back, missing floor, wrong datum, too-shallow projection, known upper neighbor and capacity rejection have counterexamples. This local arrival is not a complete storey/stair/room/controller proof.

Run `.venv` with `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings`: `python -m unittest test_window_bays verify_window_bays verify_balcony_cells verify_shopfronts verify_facade_entrances -q`. `window_bay_probe.py --output <system-temp>` emits actual before/near/whole/opposite/compact inputs and plan/section/2.5D diagnostics; render the geometry JSONs with shared `blender_city.py`. Only the diagnostic hides ceilings and ghosts glazing; functional checks and Blender inputs retain the actual products. `window_bay_cost` compares equal full prism/valid-union solids over21warm runs per case, and records cheaper **unequal** skins separately, including rejected union controls. Output/traced peak and known position arrays/copies are scoped; backend allocations remain unknown.

`data/window-bays.json` retains two actual OpenFreeMap sources, their attributes/context/footprints and versioned provider receipts; one is the earlier roof control, one is selected from the same Paris census for its default chamfered programme. Earlier entry/shop/balcony probes suppress later window bays to preserve their isolated comparisons. SIGGRAPH2003 pp1/5 were optically inspected for coherent facade-column programmes. The existing hash-checked Ghidra `prepareSplit`/extrusion exports show scope preparation/primitive dispatch, with unresolved types/indirect calls; no dedicated Esri bay-window oracle or copied assets/code is claimed. Research pictures stay local; whole architecture families, night/LOD transitions and native/device/frame integration remain open.


## Bounded distant window-bay surfaces (#428)

The measured #408 Paris study used1272distant86-triangle skins. `bay_flat` retains the projecting rectangular/chamfered outline, floor/ceiling exterior and all opaque material bands, but puts panes on the outer planes and omits reveals. It removes only attachment pieces behind the original opaque source cell, giving46triangles. The caller must explicitly supply `back_covered=True`. Source-cell coverage is part of actual `window_bay_geometry` consumption; the library does not infer world occlusion. Physical parts, plans/IDs, cell grids and source/reservation semantics remain unchanged. The old86skin remains available, and its construction now uses the same direct outline rather than building hidden wall/posts/pane sections.

Only Y≤0 exposed surfaces have a two-sided positional correspondence: panes/reveals move along their respective normals by at most `(wall_m-glass_m)/2`. Behind-source pieces can be farther from the new surface and are deliberately outside that claim. No light/shadow, interior, topology or general LOD-image equivalence follows. `facade_window_bays.flat_error` reuses the existing `recess_error` over all eight conservative box corners, both signs of all three pane normals and the camera movement radius. It bounds projected displacement in pixels before template construction. The0.5px threshold is unchanged; over-limit views keep86triangles,≥8px/inside keeps full physical parts, and<0.5px-width products remain deferred. A real140°/edge view exceeds0.5px and takes the86erfallback.

Normal tests run with `.venv` and `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings`: `python -m unittest test_window_bay_flat test_window_bays verify_window_bay_flat verify_window_bays verify_balcony_cells verify_shopfronts verify_facade_entrances -q`. The flat controls sample both exposed directions using standard GEOS planar triangle distance, test explicit back cover/early capacity, check actual pane projections under motion/front/opposite/wide-angle views, retain existing material/depth/metric-UV fields and prove exact per-cell40triangle savings in the actual consumer. These are specific geometric and consumer controls, not a global simplification optimum or native/device admission.

`window_bay_flat_probe.py --output <system-temp>` first generates the existing whole-source controls/cost matrix, then paired actual86/46far source inputs and a real wide-angle fallback. Its slice/error/work plots use the generated triangles and declared camera, not edited renderer images. Render JSONs with shared `blender_city.py`; whole source studies use `city_scene.py`. Costs compare current86/46render products separately from closed parts/valid unions,21warm runs per case, known output/materialization/copy counts and traced peaks. Backend/Python-list intermediates remain explicitly unenumerated. No shader/material parameter changes or new cache/engine contract.

Local SIGGRAPH1996 *Simplification Envelopes* pp1/3 were optically reviewed for two-sided geometric bounds and their limits. This recipe is a restricted analytic surface correspondence, not that paper's general topology-preserving envelope algorithm. The local Esri `reduceGeometry` reference describes edge collapses controlled by a vertex-reduction fraction and removal of component tags; it gives no corresponding projected-pixel contract. Existing SDK/Ghidra primitive references remain separate; no dedicated Esri window-bay LOD oracle is claimed. Research images stay local; master owns integrated/native images and admission.

### Functional corner and diagonal upper storeys (#442)

`stepped_form_profiles.py` extends the existing `stepped_form_plan.choose` with
orthogonal adjacent-side and diagonal upper-storey cuts. A separate stable
`/upper-form` seed stream chooses the programme; camera, iteration order and
facade-detail choices do not change its identity. Three widths, one/two tiers and
four orientations bound each new search to 24 contour alternatives. Diagonal
setback distances are twice the shared base widths to expose substantial corner
terraces. Source footprints/holes, supplied top height, room/core identities and
lower storeys remain the existing plan's responsibility.

`plan_for(row, profile="legacy")` retains the previous default. A requested new
profile must pass the existing complete exterior section and actual framed
near-access/terrace checks. Otherwise the previous search runs and records
`requested_profile`/`profile_fallback_reason`. The tile consumer uses this explicit
source programme on its existing bounded candidate set; this does not expand the
eight-candidate experiment or claim an entire architectural family.

Fixed actual source controls: `data/stepped-profiles.json`. Run
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings .venv/bin/python -m unittest verify_stepped_profiles verify_stepped_contours verify_structural_shell_controls -q`.
Freeze the implementation, then run `stepped_profile_probe.py --output <system-temp>`
for independent actual contour/room diagrams and paired whole/opposite Blender
inputs. Render each geometry JSON using shared `blender_city.py` at 1920×1080.
Primary/SDK/Ghidra scope: `doc/references/buildings/esri/stepped-profiles-sdk-3.4.12206.md`.
Research outputs stay local; master owns integrated Places and native admission.

`stepped_profile_cost.py --output <system-temp-json>` compares the same 24
orthogonal contour alternatives against one source-frame clipping rectangle,
21 warm runs per method/source. The single-box alternative uses 36 boolean
intersections versus `72 + 24*storeys` in the admitted two-half-plane path.
Local preliminary contour medians were 3.9–6.0 versus 19.8–34.4 ms with at most
4.6e-12 m² symmetric difference. This is a contour result only; the box path
has not passed complete room/portal/native-section reconstruction. The current
full-plan admission path remains the verified implementation. Its seconds of
room/clearance construction dominate these milliseconds; no whole-generator or
frame speedup is inferred. Output arrays/coordinate observations, traced Python
peak and GEOS exclusions are explicit in the benchmark.

### Reuse validated stepped near assemblies (#460)

`stepped_source.plan_and_near` returns the chosen plan and the exact physical
near product already constructed by its successful validation callback. The
final portal graph is checked after metadata completion. The previous
`plan_for` result and default remain available. No global cache or second plan
is introduced. The tile consumer reuses this product for resolved opening
components or an inside-source view; otherwise it retains the compact exterior.

`stepped_facade_details` uses the existing 0.5 px threshold and conservative
0.25 m opening-component radius, motion and expanded frustum. A full near
assembly additionally requires an opening width of at least 8 px, matching the
existing bay full-detail class; inside-source views override this width limit. This component
criterion is not an error bound for the entire interior/shadow/image LOD.
Presentation accepts at most 512 logical openings and 65,536 near triangles;
a local presentation failure records a compact fallback without discarding
the valid stepped source. Near products contain the existing complete simple
rooms and stairs, so their drawing cost exceeds the compact hull. Native/device
admission and a more selective exterior product remain separate work.

Presentation shares complete position/index/normal buffers with the validated product.
Projection still materializes its selected glass triangles; those temporary
copies are distinct from result-buffer sharing.
Metric glass UVs use the existing gravity projection; the existing opaque
procedural full-glass facade binding gives consistent exterior diagnostics.
This does not admit transparent or illuminated interiors. Walls retain the
supplied parent material; no shader/material parameter owner changes.

Run `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings .venv/bin/python -m unittest verify_stepped_facade_details verify_facade_bands verify_stepped_contours verify_place_scene verify_flat_places -q`.
Freeze source, run `stepped_facade_probe.py --output <system-temp>`, then render
its geometry JSONs using shared `blender_city.py`. It compares identical sources,
rooms and cameras at whole/opposite/close/far scales; source fixture from #442.
`stepped_facade_sections.py --output <system-temp>` cuts the exact original
compact/physical triangles horizontally and vertically through one window with
a sill and one terrace window without it. Local AABB filtering only limits plot
candidates; it does not rebuild a facade or weld component vertices.
`stepped_product_cost.py --case 0 --method reuse --repeats 3 --output <json>`
compares the identical retained physical product against `--method rebuild`;
repeat both methods for case 2. Counts cover successful owned near-result arrays,
not failed candidates, backend allocations or all temporary UV arrays.

Frozen runtime `3e0e80b6c`: 40 source controls and 28 installed-wheel controls
pass without skips. The unchanged wheel's 79 building modules match the source
exactly. Final Paris has the same 988 sources and all mesh arrays/dtypes, camera
and material bindings as #442: 332,807 triangles, all four stepped hosts compact.
Source/procedure fields match except measured build times and the added demand
report. Whole/close/opposite physical study inputs are exact to the actual
1920×1080 images produced on `c9e1c651a`; those images are retained with their
original production receipts, not relabelled as new renders. The radius-only
515,351-triangle city trial remains a rejected overdetail cost comparison.

Three nonisolated normal-priority runs per method/source have byte-identical
physical results. Source 0 saves one assembly and 8,393,264 owned result bytes;
source 2 saves one assembly and 9,519,672 bytes. Local median elapsed times are
28.286→21.010 s and 21.929→14.191 s respectively. Process peaks do not improve
uniformly (253,040→263,432 KiB and 267,924→254,704 KiB); these small samples
include concurrent host work and establish neither a device/frame budget nor
a general peak-memory improvement. Native exterior adoption remains separate.

### Broken urban hip profiles (#469)

`broken_hip.surface` clips the existing conformal hip surface into two height
zones using the installed `Slab` surface cutter. It maps both zones through one
continuous global height function, preserving the source eave/top and existing
triangle UV interpolation/face ownership while rebuilding geometric normals.
The initial separate split/scale/emit composition left an unmatched edge on one
actual Paris source; it is a rejected alternative, not a valid general baseline.
No epsilon position welding or bounding-box footprint replacement is used.
The component currently accepts UV0 and known face fields; extra vertex channels
reject explicitly. Its conservative three-times-input triangle cap precedes
source preparation. A source plateau on the profile knot rejects instead of
duplicating its coplanar surface.

The ordinary tile consumer infers this profile for a bounded subset of its
existing urban hip estimates based on height and height/sqrt(area); no source-ID,
place-name or camera-dependent style. Existing roof attributes prevent this
addition. The 15% base-height knot maps to 65% final rise, leaving a steeper lower
zone and shallower upper one. Profile construction/closure failure retains the
existing hip and a reason. Existing subpixel need precedes roof construction;
no added native/shared/material API. These remain exterior envelopes, with no
new attic/use/interior/controller/night or complete architecture-family claim.

Run `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings .venv/bin/python -m unittest verify_broken_hips verify_urban_roofs verify_roof_envelopes -q`.
Freeze source and run `broken_hip_probe.py --output <system-temp>` for exact
sections, plans/2.5D and paired whole/opposite Blender inputs at 1920×1080. The
probe deliberately profiles all three fixed component controls; only its
`automatic_profile` field establishes current morphology-policy selection.
Frozen runtime `cfb90060f` and comparison `dabb16bec` pass 27 source and 27
installed-wheel controls without skips; all 79 unchanged building library
modules match the installed wheel exactly. Three source plan/section/2.5D
studies and six actual whole/opposite before/after Blender images at
1920×1080 were optically opened, followed by the whole Paris comparison.
The Rome pair is a forced component control, not automatic policy coverage.

Paris retains all 988 sources, 210 constructed profiles, 778 unchanged source
prototype records (except build times), and the same camera/material/source
bindings. For the 210 hosts only the profile and triangle counts change; all
remaining source and procedure fields match except build times. Final triangle
count is 375,043 versus 332,807 (+42,236 / 12.69%). The controlled whole build
returned in 213.645 s at 645,228 KiB process peak under concurrent host work;
this is not a native/frame/device budget. Its arrays/dtypes exactly match the
first trial, whose process ended 143 after writing its results. Controlled
city/render runners explicitly flush and exit only after the domain function
returns; no native/backend cleanup admission is inferred from that exit.

`broken_hip_compare.py --output <json> --repeats 21` compares direct retained
triangle cuts against a full logical split/emit followed by the same global
height map. This improved logical baseline differs from the rejected separately
scaled scopes: all three real sources pass closed-solid/volume/section checks.
Two warmups precede timing; tracing runs separately. Output-array bytes and
cutter source-triangle visits are scoped observations; native/GEOS allocations,
cumulative allocation/copy traffic and device work remain unenumerated.

Over 21 measured runs, direct versus logical surface medians were 4.08/11.60 ms
(rectangle), 73.55/171.70 and 9.69/22.11 ms (Paris), 106.88/267.05 ms (Rome).
Corresponding final arrays have identical byte counts and 24/513/65/668
triangles; volume errors are zero and maximum section difference is
1.8e-14 m². Both scan 2× source triangles; the direct path avoids two logical
face reconstructions and two additional `Shape.from_mesh` geometries. Known
successful owned shape-result arrays total 29,638/102,997, 3,650/12,945 and
39,712/135,236 bytes on the real sources. Those counts omit output buffers,
other temporaries, native allocations and cumulative copy traffic. Local
nonisolated wall times do not establish a global or device speedup. Traced
peaks improve only within the separately measured Python/NumPy scope.

### Joined facade hierarchies (#474)

`outshine_ref.buildings.facade_frames.FacadeFrame` describes a bounded regular
lattice in local U/V/outward metres: 2..33 immutable vertical/horizontal bar
centres and at most256 rectangular apertures. `frame_surface` emits the joined
closed reference or, with `back_covered=True`, only the source-attached outside.
Covered backs are omitted before geometry materialization. Fronts use the
existing constrained polygon/holes triangulator; no epsilon position welding.
Actual geometric normals and metric face UVs accompany the result. Closed
and outer counts are `12+20*K` and `10+14*K` triangles for K apertures.
The outer product is never collision geometry.

`facade_frame_programme` groups existing window/storey axes, using paired
residential piers or storey grids for mixed/institutional uses. It preserves
the actual first-storey entry/projection facade, leaves physical window modules
including casings/sills free, and starts above the ground programme. At most
four statically ranked non-entry facades fit a camera-independent4096-triangle
source budget. Supplied source/materials, roof and opening plans remain shared.
Known height-overlapping neighbors/other source wings reject locally. Source
and transport/terrain contacts outside that bounded source context are not
admitted by this experiment.

Detail need precedes template/triangulation work; deferred logical plans remain
present. Valid hierarchies replace the previous decorative bands on their
edges, rather than overlapping them. Local construction failure retains the
previous complete facade and a reason. `facade_parts.append` combines frames
and remaining bands once, removing the initially redundant full-body append;
both actual source outputs were array/dtype-exact before and after that change.
This is a component geometry criterion, not a full image/shadow/native LOD bound.

`data/facade-frames.json` preserves two real Paris source/context cases and their
four-metre neighbor context with provider receipt hashes. Selection used large
free non-entry elevations; it is not a place-specific style rule. Run with
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings .venv/bin/python -m unittest test_facade_frames verify_facade_frames verify_facade_bands verify_tile_facades -q`.
Freeze source, then `facade_frame_probe.py --output <system-temp>` for actual
plan/mesh-section/2.5D and identical whole/opposite/close/far Blender inputs.
`facade_frame_cost.py --output <json> --repeats 21` compares identical2D regions
against vector/GEOS bar union, plus independently equivalent closed solids.
Generic3D union additionally carries per-bar provenance and omits render UV/
normal fields; those full-construction phases are scoped separately, not equal
attribute pipelines. Traced peaks/output fields exclude backend/cumulative
allocation/copy totals.

Frozen runtime `12fec1ac8`: 63 source and 63 fresh-wheel tests passed without
skips; all80 packaged building modules matched source bytes. Actual physical
sections/2.5D and eight whole/opposite/near Blender images were opened at
1920×1080, plus the whole Paris comparison. The unchanged988-source city grew
from375043 to378857 triangles (+3814,1.017%): 307 logical frame elevations on
225 hosts,26 expanded outer products (6602 triangles),281 deferred. Existing
source/function plans, roofs, rooms and steps matched after excluding timing
fields and the optional frame plan. Disabling frames reproduced every old
city mesh array and dtype exactly. Only old decorative-band records changed.
Facade depth/grouping is visible nearby; the city-image difference remains
small (0.9982% changed pixels), not a completed architecture-family catalogue.

For the actual11-/63-aperture examples,21 measured region runs gave broadcast
p50 0.068/0.083 ms, meshgrid0.166/0.261 ms, GEOS bar union1.469/4.614 ms.
Separate traced peaks were4632/11384,8849/14011,4460/5324 bytes respectively;
the native GEOS heap is excluded, so these are not total-memory rankings.
The reference solids agreed independently within3.4e-15 m³. Closed232/1272
triangles become outer164/892; hidden backs of68/380 triangles never exist.
Those final arrays occupy48024/263304 versus33948/184644 bytes, including
normals/UV/materials/roles. Full geometry, provenance and attribute costs differ
from generic3D union and must not be compared as identical render pipelines.
The actual city build took175.852 s, process peak628544 KiB on this nonisolated
host; neither is a native frame/device budget or a measured speedup.
Diagnostic cuts were corrected in `de7e03d54`; the distant camera in
`0ab9d9a5a` keeps the small building inside the camera range. No complete
architecture family/night/controller/device or full image/shadow LOD claim.

### Repeated industrial roof sections (#480)

`outshine_ref.buildings.section_roofs.RoofSection` holds immutable metre-valued
run/height knots and an ENU XY frame. `section_surface` clips the actual polygon
with holes into profile intervals, triangulates with batched GEOS/CDT and emits
an upward surface with interval IDs, geometric normals and developed metre UVs.
`heightfield_envelope` supplies the existing closed reference. At most65 knots,
256 boundary vertices and the conservative triangle budget are admitted before
clipping. Outer cuts extend past the source domain to avoid roundoff slivers;
interior section boundaries are shared, with no epsilon position welding.

`industrial_roof_choice` infers2..8 symmetric/asymmetric roof bays for bounded
ordinary industrial-context sources. It preserves footprint, provider top,
supplied roof fields and special-structure exclusions. The programme reserves
roof rise before facade/storey planning. Existing selected generic near rooms
remain attached with the same IDs and sampled physical access; local failures
retain the previous functional flat model. Ordinary industrial facades can use
the existing entrance/landing/arrival selector. This does not supply loading
doors, industrial room programmes or glazed clerestories.

`data/industrial-roofs.json` contains three actual width-spaced Grohnde sources
and bounded neighbor/context/provider receipts. No city/ID-dependent recipe.
Run `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings .venv/bin/python -m unittest test_section_roofs verify_industrial_roofs -q`.
Freeze source, then `industrial_roof_probe.py --output <system-temp> --tile`
for sections/2.5D and whole/opposite/far inputs; `--roof-end --tile` adds gable
comparisons and frames the industrial group more closely. All provider sources
remain in the whole-tile model even where the camera excludes distant outliers.
`roof_section_compare.py --output <json> --repeats 21` checks scalar/batched
full arrays and dtypes, including normals/UV/material/interval ownership.

Runtime `44a1b1008`:78 source and78 wheel checks passed, no skips,81 packaged
building modules byte-exact. On the actual4/8/16-interval roofs, batched p50
was0.538/0.635/0.946 ms versus scalar0.677/0.980/1.725 ms (21 measured runs,
2 warmups, nonisolated). Both methods process the same intervals; Python-to-GEOS
dispatch is2 versus8/16/32. Separate traced peaks15579/19515/28187 versus
16083/20939/31779 bytes exclude the native heap. Final outputs1680/3360/7140
bytes; local XY copy plus final packed XYZ writes960/1920/4080 bytes per
method, excluding other temporary/internal/allocator copies. No native speedup.
The complete37-source tile has16 new roof plans (8 per kind), no roof/facade
failure or source omission,2724→3610 triangles (+886). Near5-/15-room source
controls preserve room IDs and physical access. Facade/roof shapes improve;
complete industrial architectural families and broader detail/night/LOD/device
quality remain open. Fifteen actual1920×1080 Blender images (individual whole/
opposite/gable/far and the focused complete-tile pair) and three2400×1350
section/2.5D diagrams were opened. The close tile pair changes1.1555% of pixels;
shape gain is clear on individual gable views. Neutral materials and missing
industrial openings/details remain a substantial visual limit. Final model
equality and original render/camera bindings are recorded in the PR.

### Simple usable industrial halls (#489)

`industrial_hall_plan` uses the existing provider footprint and industrial
facade/entrance selector for a bounded single-room interpretation: ground-level,
800–15000m², one existing facade row and an inferred section roof. Small existing
room programmes remain unchanged. The same cells own the personnel door, a
2.8–3.8m loading opening and up to96 actual windows; blocked/superseded slots have
explicit reasons. No place/ID rule or measured industrial use is asserted.
`data/industrial-halls.json` pins three accepted real source/context examples
and a real footprint with no admissible loading cell, including provider bytes.

`industrial_hall_shell` reuses the original typed heightfield envelope, cuts one
room and the actual openings in one CSG operation, and adds a three-metre inner
ramp from ground0 to floor0.16m. Existing upper roof geometry remains physically
unchanged; Boolean output ordering/roundoff need not match the original stream.
The earlier exterior-only exact-array roof control explicitly bypasses this
new hollow-hall product; actual hall roof/height checks remain separate.
`industrial_hall_details` composes existing personnel leaves, window frames and
thin glazing, plus a source-bound exterior-mounted roller, U-track and housing.
There is no fake back wall behind the gate and no interior furnishing. Actual
pawn/vehicle-sized sampled clearances are geometry evidence, not a controller
or road/terrain/turning-route admission. Known buildings and own other wings
limit provisional exterior detail/site clearance; Transport reservations remain
owned by the common spatial contract.

`tile_industrial_halls` keeps logical hall/room/opening identities independent
of gate/door state and view. A whole-host frustum/movement test plus8px gate
threshold runs before cuts/templates; an inside camera requests detail. A single
transient typed roof reference avoids rebuilding it, with no tile-wide mesh
cache. Far products keep logical plans with explicitly deferred hall geometry;
full image/shadow/transition equivalence is unproved. Local failures retain the
prior facade and a reason. Gate fractions and existing hinged-door degrees are
separate inputs. No shared/native/material/reservations API changed.

Run with `.venv` and `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings`:
`python -m unittest test_roller_gates verify_industrial_halls verify_industrial_roofs -q`.
After freezing source, `industrial_hall_probe.py --output <system-temp> --tile`
produces actual2D/sections/2.5D and1920×1080 before/after/state/interior/opposite/
whole-tile Blender inputs. Render geometry JSONs with shared `blender_city.py`.
Images stay local. `roller_gate_cost.py --output <json>` compares direct cap
extrusion, generic Shape extrusion and section-heightfield solids with equal
physical/UV/normal requirements, Native differences and21 warm repetitions.
Output writes, traced live allocations/peak are explicit; total/native-heap
allocations/copies and frame/device speedup are not claimed.

SIGGRAPH2006 Procedural Modeling of Buildings p5 (snapping/alignment/occlusion)
was optically inspected; prior shared extrusion/heightfield/CSG kernels implement
these local relationships. SDK3.4.12206 `libcom.esri.prt.core.so` SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`
was freshly examined with Ghidra at Processor::extrude004b5210 and
Processor::subtract00541230 (empty/invalid input guards, transform and Boolean
operation dispatch). This bounds primitive-level reference use; no Esri source,
roller simulation, ready hall recipe or oracle-render equivalence is claimed.

Runtime62bd10218 passes116 source checks; the final room-identity correction
4e5828277 passes all6 affected source controls and the fresh wheel passes the
same116-check set, no skips. All82 installed building modules are byte-exact;
wheel SHA256 `aac088cf20ef7b05728ba62cdc57d85735fccb92a71e4650d9085d3d4d3315b6`.
Fifteen final models retain every geometry stream/dtype, camera and material
binding exactly against the earlier frozen render inputs. Thirteen actual
1920×1080 Blender images and three2400×1350 plan/section/2.5D diagrams were
opened. Eleven images use the original4be795a59 inputs, accepted only through
that exact equality proof; the closer tile pair uses4e5828277 inputs. Render
processes return0 after successful Eevee completion; the existing `bpy` cleanup
crash is bypassed with `os._exit(0)` only after render returns, no normal-teardown
claim. This is llvmpipe reference rendering, not an RTX/device budget result.

In the closer complete37-source comparison, six near halls replace former
facades,32445→60123 triangles (+27678); eight logical halls, one unsuitable
loading case with a reason, no source omissions/roof/facade errors. Source IDs,
properties, footprints, heights, classifications and provenance remain exact.
The wide camera retains3610 triangles with hall geometry deferred. The closer
tile pair changes10417/2073600 pixels (0.5024%); gate/window differences read
more clearly in the individual near shots. Broad facade/roof detail, roof glass,
credible material/weather variation, exterior transport connection and native/
night/complete-family/full-image LOD quality remain open.

Equal physical sheets at opening0/.35/.5 use168/112/88 triangles for both direct
and generic Shape extrusion versus324/212/164 for strip-heightfield closure.
Measured direct p50 is2.442/3.184/1.859ms, Shape30.660/18.976/16.629ms and
strips18.779/13.355/10.232ms over21 runs (2 warmups, shared-host load). Direct
traced peaks81852/56316/45956bytes versus423939/291121/233911 (Shape) and
416879/279735/220959 (strips) exclude native allocations. Equal volume, surface
area and developed UV edge lengths are independently checked. Final buffer
bytes/writes and live traced allocation counts are in the reproducible JSON;
these timings do not imply a native frame/device factor.
## Industrial rooflight research

Actual source-bound roof openings, closed glass/rims, physical counterexamples,
paired Blender cameras and equal-volume frame/whole-hall cost comparisons:
[industrial_rooflights.md](industrial_rooflights.md) (#500). Experimental variant;
default hall/recipe v1 and compact/far admission remain unchanged/open.

Complete immutable hall assets, independent physical/render products, exact
legacy replay and the whole offline 37-source Grohnde tile comparison:
[prepared_hall.md](prepared_hall.md) (#500). Camera changes select retained
geometry; diagnostic export still copies full bodies. Eight glazed halls replace
the previous six, without claiming a complete whole-building LOD or native cache.

Source-cell industrial facade articulation, protected functional openings,
shared closed/exterior buffers and true near/opposite/whole-tile comparisons:
[industrial_relief.md](industrial_relief.md) (#519). Three seeded bay hierarchies
use existing metal-frame materials. Geometry-only omission can lose distant
metal signal. The corrected default retains shared distant front surfaces:
[industrial_relief_signal.md](industrial_relief_signal.md) (#523), with actual
full/omitted/projected comparisons and near/grazing negatives. Complete physical
products remain available; general radiance/shadow/temporal LOD remains open.

Actual provider-derived cooling-tower intakes, crossed supports, collector and
upper rims: [cooling_tower.md](cooling_tower.md) (#526). Stable source programmes,
independent connected physical unions, finite intake sweeps and full37-source
comparisons accompany near/opposite/elevated1080p rasters. Further equipment,
complete access/interiors, general special-family coverage and native cache/
frame admission remain open.

Source-contained repeated upper wings and compact podium masses, real terrace
access, retained near/compact products and equal reconstruction/binding costs:
[podium.md](podium.md) (#533). Source-stable new mass selection complements the
existing fallback; broader facade variety and general cache/LOD admission
remain open.

Broad paired-pier/storey-spandrel composition on retained window axes, exact
closed/outer/front products and full current-city comparison:
[clad_facades.md](clad_facades.md) (#543). Existing source eligibility remains;
219 sources gain stronger visible hierarchy. Near counterexamples prevent a
general front-only LOD claim; full architecture-family variety remains open.

Contained parapets and recessed service enclosures, closed physical products,
retained near-window hosts and material-preserving coarse roof surfaces:
[roof_crowns.md](roof_crowns.md) (#551). The whole988-source window retains all
prior masses and gains47 roof programmes at+1.18% triangles; broader architecture
families, roof access/detail and general native/cache/LOD admission remain open.

Source-stable base/body/top facade scopes with narrow vertical windows or broad
horizontal glazing, retaining the existing entry/projection elevation:
[elevations.md](elevations.md) (#574). The whole988-source window gains91 composed
elevations on44 sources; actual near crops and complete city pairs are checked.
These are two bounded facade programmes, not complete architecture families.

Attached source-contained roof volumes and real near window pockets:
[dormers.md](dormers.md) (#585). The whole988-source window gains3097 dormers on368
roofs, retaining earlier source/function/facade/roof owners. This is one repeated
box-dormer programme; broader whole-building forms and coherent variety remain open.

Source-stable asymmetric terraces and paired upper wings, with simple open floors
and actual stair/entry components before detailed room fitting, are prototyped in
[terraced_mass.md](terraced_mass.md) (#618). Independent sections, closed component
roundtrips and access controls accompany explicit cutaway renders. The new path
now has a physical facade layer in [terraced_facades.md](terraced_facades.md):
aligned actual windows, frames, bands/piers and complete ground-arrival controls.
The logical plan now precedes expensive physical expansion; a direct, batched
exterior and one retained lazy physical product are compared in
[terraced_facade_products.md](terraced_facade_products.md). Roof/terrace programmes,
selected near components, general LOD/cache admission and complete-city selection
remain open; existing functional products retain their own checks.

Protected setback decks with level actual portals and continuous core-to-exterior
access are documented in [terraced_terraces.md](terraced_terraces.md) (#618).
Closed guards/copings are shared by direct/near products; unserved roof ledges
remain explicit. Highest-roof access, broader whole-source architecture selection
and general runtime budgets remain open.

Plan work before broad source trials is reduced with exact product replay in
[terraced_source_demand.md](terraced_source_demand.md) (#618). A complete988-source
census protects prior functional owners and identifies direct candidates; their
physical/function and whole-city products are separate subsequent studies.

The complete proposed cohort now has a separate strict
[physical admission study](terraced_admission.md), with preserved actual source
failures, independent shell sweeps, full component checks and continuous declared
ground/terrace routes. Failed new assets retain their prior source route; this is
separate from whole-city imagery and native/controller admission.

The [ground-approach correction](terraced_ground.md) uses the same conservative
actor radius at courtyard walls and stair obstacles. Original blocked routes
remain negative controls; an additional valid stair guards against unnecessary
changes to its orientation. The report owns the complete cohort replay and scope.

The [complete terraced city comparison](terraced_city.md) applies the successful
new candidates while preserving existing owners and rejected fallbacks. Actual
normal/full-near1920×1080 images, source completeness, precise index-width replay
and scoped costs are checked locally; general runtime budgets and architectural
finish/diversity remain open.

The [coupled highest-roof study](terraced_crowns.md) reserves source height before
storeys/windows and composes vertical parapets, copings and optional technical
blocks. Direct roof skins and independent roof/body demand avoid expanding a
distant body's complete near asset for a visible roof. The report separates
all71 roof-component checks from the three changed-body controls and records
actual paired images, full/far/mixed costs and remaining functional/visual gaps.

The [complete coupled city follow-up](terraced_crowned_city.md) checks all71
changed bodies and their full draw products before whole-window composition.
It retains a real null-topology counterexample, an exact cancellation guard and
oriented contour recovery. The standalone contact probe reproduces its paired
models without the whole source window. Current whole-city detail selection,
actual1080p images and scoped cost/quality limits are recorded separately.

The [optional thin-glass study](window_glass.md) exposes the existing real room
depth through closed window panes, with an independent Mitsuba scalar-model
check and explicit near/front/grazing/inside/whole camera controls. Default
materials/daylight remain; roughness, screen refraction and fully transparent
geometry controls distinguish the approximation from the original material.
