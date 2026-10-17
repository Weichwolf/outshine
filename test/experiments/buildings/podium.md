# Source-contained podium and upper-wing programmes

Issue [#533](https://github.com/Weichwolf/outshine/issues/533), exterior owner
[#32](https://github.com/Weichwolf/outshine/issues/32). Pure Python research;
no native, provider, reservation, shader or shared-library API change.

## Method

`podium_plan` composes existing connected storeys, rooms, circulation core,
openings, terraces and prism products. A one/two-storey source-shaped base
supports either an all-side inset upper mass or paired axial cuts forming an
upper wing. The upper profile is constructed once and repeated exactly for at
least two storeys. Courtyards, original ground footprint, source heights,
room/core identities and vertical portals remain. Widths1.8/2.8/4m and base
storeys1/2 are bounded procedural alternatives, not supplied architecture.

Existing core/corridor and finite door approach pads constrain the upper shape.
Canonical derived contours are clipped back to the original unsnapped source.
Only the first upper storey carries the terrace; subsequent contours have
identical WKB. Independent per-storey reconstruction must produce exactly the
same rooms, windows, walkable areas and elevations. Real terrace portals and
independent compact/physical mesh checks admit a complete programme.

`podium_products.prepare` retains an immutable logical plan, physical near
product, near presentation and compact exterior. Near presentation shares the
physical positions/faces/normals. `bound` copies material IDs while retaining
prepared geometry/UV streams. Camera demand selects these products; it never
selects a different mass. This is in-memory preparation, not a persistent cache
format or complete radiance/shadow/temporal LOD contract.

`tile_podiums.PodiumDetails` opts into the existing ordinary-building path.
Source attributes, contextual residential/commercial use, height9–35m,
area180–1600m² and bounded contour size determine eligibility. Explicit roof
properties, clipped sources and raised buildings are excluded. One quarter of
eligible stable source IDs request a programme; a separate seed chooses the
preferred kind. Failure tries the other kind, then preserves the valid existing
body with recorded reasons. Neither camera nor place name chooses a new mass.
The inherited legacy shortlist still has its previous camera policy; general
stable admission remains open under #33.

## Counterexamples and controls

`verify_podium` has nine controls, including analytic rotated rectangles,
four real source plans, independent repeated-storey reconstruction, invalid
requests, immutable products, exact retained heights/IDs, actual body/terrace
access and warm near→far→near reuse with preparation forbidden.

Removing the finite original-door approach pads makes the real compact podium
fail torso clearance; the actual reserved version passes. A courtyard wing
fails the unchanged independent surface-area gate despite matching volume;
it remains rejected. A two-group prism construction trial still fails that
source and introduces an unresolved Boolean normal on another; it is not used.
No tolerance or collision gate was relaxed.

The whole-city run found a real45.52m² terrace accompanied by an inaccessible
1.48e−8m² fragment. Previously only larger components were checked before
binding all portals. Every access record must now succeed; the original input
is versioned in `data/podium-counterexamples.json`. The geometry is preserved,
not erased with a new sliver tolerance. A mixed-input regression also keeps
valid landuse rows without building IDs out of roof admission.

## Reproduction

Use `.venv`, the existing experiment requirements and the installed library.
All outputs are private system-temp artifacts; no replacement gallery/server.

```sh
export PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings
.venv/bin/python -m unittest verify_podium -q
.venv/bin/python test/experiments/buildings/podium_probe.py --output /tmp/outshine-buildings-podium-models
.venv/bin/python test/experiments/buildings/podium_costs.py --output /tmp/outshine-buildings-podium-upper-costs
.venv/bin/python test/experiments/buildings/podium_asset_costs.py --output /tmp/outshine-buildings-podium-assets
.venv/bin/python test/experiments/buildings/podium_probe.py --city Paris --census /path/to/pinned-census.json --output /tmp/outshine-buildings-podium-city
```

`podium_probe` requires a clean frozen experiment revision. Its fixed inputs
are ordinary saved provider sources, not bespoke place geometry. It emits real
2D mesh sections and12 before/after Whole/Opposite/Terrace model inputs, with
paired cameras, materials and1920×1080 profiles. Render each geometry report
with `blender_city.py`; compare paired PNGs using `test/scripts/pixels.py` and
open the actual images. The city command uses an explicitly supplied pinned
census, preserves its full window and rejects any source omission. It uses
flat placement without DEM. Source acquisition is not part of the comparison.

## Measured scope and limits

Frozen `a5ac4a9c6`: independent-versus-shared repeated upper XY construction
has equal outputs on three sources. Shared versus independent p50 over21
subsequent calls is121.33/241.97ms,129.49/319.51ms and185.96/349.65ms.
Candidate calls1 versus4/5/5 are counted separately from timing. Traced peaks
are297099/374694,223396/294744 and456851/612519bytes. Unique geometry handles
are296/310,255/298 and473/492. These exclude base planning, search, portal
binding, 3D construction, native GEOS heap and renderer costs. Shared-host load
was10.42→12.40 on four logical CPUs; these are scoped trends, not device factors.

Frozen `dd3f44808`: complete preparation of the actual wing/podium took11.62/
14.88s with separate traced peaks30.11/42.56MB under shared-host load11.33→
12.32. Retained NumPy roots are14.65/20.75MB, excluding GEOS/Python heap.
Warm near material binding p50 is0.0769/0.0547ms versus1.3416/1.7273ms when
copying every prepared stream, with identical arrays/values. Explicit new
arrays are87952/123080bytes versus12225328/17108120bytes; compact binding
p50 is0.0146/0.0098ms. Each alternative copies the material-ID stream once;
the full-copy alternative additionally copies every other prepared stream.
These measure in-memory draw binding, not loading or
whole-building generation, frame time or GPU performance. Full preparation
remains expensive; narrow successful examples do not prove broad coverage.

The actual single-building near products have43976/61540 triangles versus
46212/61064 before. Their simple repeated window programmes remain visually
monotonous; broader facade composition, roof equipment, complete architecture
families and overall CityEngine-level variety remain open. The new long upper
masses and larger terraces improve form variation only.

## Frozen delivery evidence

`fb29fd0a0`: nine source controls passed65.006s; the same nine installed
controls passed63.625s. Twelve pre-existing stepped/profile controls passed
89.409s at `a5ac4a9c6`; the later tiny-component guard changes only admission,
and all12 final model streams replay those retained render inputs exactly.
After Transport PR#538, nine controls passed56.370s against the fresh package
at `34c23868c`: all201 modules byte-exact, wheel SHA256
`e15068518082f4e5fac8f6ceccfa6c65cc4f8f7491247734a6352d3287845105`.
Ruff format/static checks, diff checks and `make lint-docs` pass; native gates
are outside this Python delivery.

The full unchanged800×800m Paris window at `fb29fd0a0` completes with988/988
buildings and zero window omissions,378857→383423 triangles. Six pinned tile
receipts feed that window; it is not a rendering of all six complete tiles.
Existing2724 provider-preparation omission records remain unchanged. Of80
source-selected requests, eight programmes succeed (seven wings, one compact
podium);72 retain the prior body.111 rejection records include alternate kinds,
not111 omitted sources.33 sources have no complete base room/storey plan;
many others fail room clipping/connected access. This limits visible coverage.

Source attributes, original footprints/heights, IDs and flat placements match
for every source. Material definitions/bindings, camera and source receipts
match exactly. Only eight member records differ after excluding explicit
timing diagnostics. The actual1080p before/after city images were opened;
7075 pixels change(0.3412%), mean RGB error0.053591/255. The modest image
change reflects the narrow admitted coverage, not citywide visual parity.
An exact triangle-multiset comparison includes every supplied position,
normal, UV, facade extent and material ID:374745 triangles match unchanged;
4112 disappear and8678 are added. Changed triangle centres lie within0.90m
of the eight source footprints before (existing projections) and0.001m after
(surface offset). These are observed distances, not admission tolerances.

All14 actual rasters use Blender5.2.2/Eevee16 with llvmpipe LLVM19.1.7.
Geometry reports, complete mesh streams, source normals/UVs, input/image hashes,
fixed camera/material/light bindings and pixel comparisons were checked.
Single-building reported ground spans differ below a nanometre; actual
Float32 plane span/centre are identical. The raster wrapper terminates after
successful saving to avoid the known package teardown crash; no normal-teardown
or hardware GPU claim. One interrupted city attempt ended SIGTERM/143;
the separate terminal receipt records successful exit0/657.12s for the full
model/export run. This includes construction, not network acquisition or a
warm cache/load/frame result.

Current private artifacts:

- `/tmp/outshine-buildings-podium-output`: four2D diagrams,12 actual rasters,
  model streams and raster receipts; final exact replay in
  `/tmp/outshine-buildings-podium-final-replay/exact-replay.json`.
- `/tmp/outshine-buildings-podium-city-final-output`: complete city models,
  two actual rasters, source audit and terminal/raster receipts.
- `/tmp/outshine-buildings-podium-cost-output` and
  `/tmp/outshine-buildings-podium-asset-cost-final-output`: scoped raw costs.
- `/tmp/outshine-buildings-podium-visual-audit.json` and
  `/tmp/outshine-buildings-podium-city-pixels.txt`: checked image comparisons.
- `/tmp/outshine-buildings-podium-esri-ee6da7bbe`: bounded local Ghidra export.
- `/tmp/outshine-buildings-podium-wheel-34c23868c`: wheel, installed package
  and byte/import receipt. These private paths are evidence, not dependencies
  of the procedures or versioned fixed controls.

## References

SIGGRAPH2003 *Instant Architecture* attributed split/component composition,
page4/Figures5–6, was optically inspected. Existing SDK envelope/setback
references and a fresh bounded Ghidra examination distinguish geometric
setbacks from functional room planning:
[SDK observation](../../../doc/references/buildings/esri/podium-sdk-3.4.12206.md).
No ready Esri architecture recipe, complete source or runtime equivalence is
claimed; all programme code here is our own composition of existing operators.
