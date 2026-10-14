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

```sh
.venv/bin/python test/experiments/buildings/showcase.py --publish --serve
```

The showcase reads the checked current snapshot and serves it on localhost:8766.
New published states appear after manually reloading the page; no polling timer runs.
Outputs remain under `build/experiments/buildings/`. Its four pages are viewing
scales; native LOD boundaries and runtime budgets remain unproven.
