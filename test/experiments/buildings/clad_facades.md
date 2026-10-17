# Broad pier and spandrel facade composition

Issue [#543](https://github.com/Weichwolf/outshine/issues/543), exterior owner
[#32](https://github.com/Weichwolf/outshine/issues/32). Pure Python research;
no native, provider, reservation, shared-library or shader API change.

## Method and preserved inputs

`clad_facade_plan` composes broad paired piers or storey spandrels on existing
window/storey axes. Source family and a stable source-ID seed choose the
programme. The camera chooses only a draw product. Existing ground-floor
entries, shops and projection programmes remain; the entry facade is excluded
because it also owns the existing bay/balcony stacks. Up to four other
elevations use the existing4096-triangle per-source outer budget. Ineligible,
crowded or over-budget elevations preserve the previous valid fallback.

The actual window module supplies casing/sill bounds, expanded by a procedural
4cm clearance. These reserved regions are subtracted before extrusion. Joined
bars remain inside the original facade extent, below the eave; source-wing and
height-overlapping neighbor contacts use the existing independent checks.
This is not a reconstruction of supplied facade plans or arbitrary portals.

`clad_facade_mesh.surface` uses existing polygon/holes triangulation, boundary
sides and metric triangle products. Its explicit modes are closed reference,
covered-back outer and front only. Front mode constructs no boundary sides or
hidden back. One region determines all products; actual front positions,
normals and UVs match exactly. Existing frame metal material10 is reused,
with no new shader/material parameter contract. Cached local templates are
immutable; placement still allocates world positions/normals/materials and
facade extents. This is not persistent asset loading or complete building LOD.

`CladFacades` optionally replaces the existing frame stage. The default
`FacadeDetails` path remains unchanged. Three small hooks in `FrameDetails`
separate programme choice, product and description; existing six controls
remain valid. New logical records use `facade_clad_plan`: `source_frame` names
the original axes/bounds, while `composition.region_wkb` is the actual new
surface. Old frozen image reports use the previous diagnostic field name;
the final model replay proves unchanged numeric streams after this naming fix.

## Real inputs and checks

The existing two Paris frame controls are joined by one source selected from
the same pinned census for its stable vertical-programme seed and large
existing facade. Original attributes, geometry, four-metre neighbor context
and six provider receipts are versioned in `data/clad-facades.json`. The
selection is diagnostic sampling; product code contains no place/source-ID
special geometry. Both programme kinds run on every actual control facade.

Eight new controls check closed directed edges, volume, normals, metric UVs,
real casing/sill regions and aperture rays, exact front products, no far-side
construction, two distinct programmes, height translation, preserved entry/
shop/bay/balcony fields, immutable warm reuse and failed contact/invalid input
cases. With six existing frame controls,14 source tests pass4.037s at
`da73ed203`.14 installed tests pass4.830s at `4db8fe635`, using the fresh
`a79216814` package:205 modules byte-exact, wheel SHA256
`05a9ae4cf1f648b7637fc1df1e294fe92451f259d39957bff8d87394d14d6182`.
Platform: Linux x86_64, Python3.13.5. Native/device gates are outside this work.

## Whole source window and actual images

Frozen `5e334aa11` constructs both complete988-building800×800m Paris window
passes with zero window omissions. The same eight prepared podium/upper-body
assets from #533 are shared between passes. All source attributes, footprints,
heights, IDs and flat placements remain exact, as do camera, six source
receipts and material definitions/bindings. Existing provider-preparation
omission records remain unchanged; this is not all six complete source tiles.
The prior383423-triangle city replays exactly, including every mesh field.

The same219 source buildings receive298 new elevations (153 pier,145 spandrel)
versus300 previously planned frames.25 use outer products and273 retain front
surfaces at this camera. All219 previously eligible frame sources remain;
this expands visible architectural signal, not provider classification.
Total triangles383423→424225. The actual city image changes54546 pixels
(2.6305%), mean RGB error0.369696/255. The full model/export comparison exits0
in818.27s; it includes two constructions/exports and preparation, not network
acquisition, warm loading or frame time.

Three2D region/protected-aperture diagrams precede34 new1920×1080 actual
Blender5.2.2/Eevee16 rasters: three sources, Whole/Opposite/Near before/after,
three far alternatives per source, six forced-near controls and one new city
image. The prior actual city raster is reused only after exact complete mesh,
camera and binding replay. Geometry/normal/UV import, report/input/image hashes
and paired camera/material/light settings are checked. Final `4db8fe635`
single-source/negative exports replay all33 original numeric model streams
exactly. Single-source geometry producers are `6c1165cd2`/`1cb734c4b`, renderers
`6c1165cd2`/`a79216814`; city geometry/renderer is `5e334aa11`.
Later changes seal templates, add a vertical fixture and clarify
metadata, without changing emitted numeric geometry. Renderer backend is
llvmpipe LLVM19.1.7; the wrapper exits after a successful save to avoid known
package teardown trouble. No hardware GPU or normal teardown claim.

For the three far tests, front versus omission mean RGB errors inside the
independently projected full-building bounding box are0.6305/3.0727,
1.3115/5.9156 and0.1472/1.2053 of255. Front worst errors21/28/21 are smaller
than omission57/62/51. Global errors and ROI are both recorded; ROI is based
on actual mesh/camera projection plus one raster pixel, not error-dependent
cropping. Small global background differences can otherwise enlarge a changed
pixel bbox and dilute its mean. These are RGB-byte observations for fixed
daylight/cameras, not linear-radiance bounds.

Forcing front-only at near distance produces worst errors132/131/53 and
55243/59857/31655 changed pixels: visible relief and shading are lost. It is
an intentional counterexample, not a default product. General grazing, night,
shadow, temporal transitions, cache policy and native interaction remain open
under #33/#48. The city still has repetitive windows, large empty roofs and
insufficient complete architecture-family variety; CityEngine quality is not
achieved by these two programmes.

## Equal-output costs

Frozen `ff8aecf97`, two real sources × both programmes, exact complete front
positions/faces/normals/UV/material/facade-extent arrays. Direct front versus
construct-all-sides/filter p50 over21 subsequent calls:

| Case/programme | Direct [ms] | Filter outer [ms] | Direct/filter traced peak [bytes] |
|---|---:|---:|---:|
|0 pier|6.283|7.853|197532 /202776|
|0 spandrel|38.148|39.441|1023288 /1018488|
|1 pier|5.766|7.316|167784 /174480|
|1 spandrel|27.949|29.991|757104 /757104|

Direct boundary-side calls are zero; the other method builds each region's
sides before discarding them. Example:132 output triangles versus488
materialized triangles including filtered copies. Triangulation still
dominates; direct construction does not consistently reduce traced peak.
Warm reuse/place p50 is0.026/0.069/0.029/0.044ms versus rebuilding/placing
9.925/51.591/7.548/24.562ms. These are scoped trends under shared-host
load14.93→15.18 on four logical CPUs, not a general hardware speedup.
Source/body/plan/contact preparation, renderer and export are excluded.
Traced peaks exclude native GEOS/compiler heap; explicit output bytes and
materialized triangle work do not measure all traffic or instructions.

## Reproduction and retained evidence

Use `.venv` and the existing experiment requirements. Frozen clean sources are
required for model/cost evidence. Outputs stay in system temp; no gallery/server.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/assets:test/experiments/buildings
.venv/bin/python -m unittest verify_clad_facades verify_facade_frames -q
.venv/bin/python test/experiments/buildings/clad_facade_probe.py --output /tmp/outshine-buildings-clad-models
.venv/bin/python test/experiments/buildings/clad_facade_probe.py --negative-near --output /tmp/outshine-buildings-clad-negative
.venv/bin/python test/experiments/buildings/clad_facade_probe.py --census /path/to/pinned-census.json --output /tmp/outshine-buildings-clad-city
.venv/bin/python test/experiments/buildings/clad_facade_costs.py --output /tmp/outshine-buildings-clad-costs
.venv/bin/python test/experiments/buildings/clad_facade_metrics.py --models /tmp/outshine-buildings-clad-models --negative /tmp/outshine-buildings-clad-negative --output /tmp/outshine-buildings-clad-metrics.json
```

Render emitted geometry reports with `blender_city.py`; open PNGs and use
`test/scripts/pixels.py before.png after.png`. Current private artifacts are
`/tmp/outshine-buildings-clad-model-output`, `clad-third-output`,
`clad-negative-output`, `clad-city-output` (same prefix), `clad-cost-output`,
`clad-projected-metrics.json`, `clad-exact-replay.json` and
`clad-wheel-a79216814`. These paths are evidence, not procedure dependencies.

SIGGRAPH2003 *Instant Architecture* p4/Figures5–6 and SIGGRAPH2006 *Procedural
Modeling of Buildings* p3/Figure4 were optically inspected for attributed
split/repeat/component composition. Existing bounded SDK/Ghidra body exports
were reread: `splitXYZ` (`004f0900`,1588 instructions) sets up mesh tracking,
cuts vertex/edge/faces and appends cut meshes; extrude/reference coverage and
limits are in [facade-hierarchies-sdk-3.4.12206.md](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md).
These observations support primitive composition, not ready Esri facade
recipes, verified inferred types or numeric equivalence. All programme code
is our own composition of existing operators.
