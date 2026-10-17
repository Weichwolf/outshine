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

Frozen `b87d926ad`: complete preparation of the actual wing/podium took12.01/
14.61s with separate traced peaks30.11/42.56MB under shared-host load12.06→
13.21. Retained NumPy roots are14.65/20.75MB, excluding GEOS/Python heap.
Warm near material binding p50 is0.0446/0.0550ms versus1.3762/1.6800ms when
copying every prepared stream, with identical arrays/values. Explicit new
arrays are87952/123080bytes versus12225328/17108120bytes; compact binding
p50 is0.0080/0.0097ms. These measure in-memory draw binding, not loading or
whole-building generation, frame time or GPU performance. Full preparation
remains expensive; narrow successful examples do not prove broad coverage.

The actual single-building near products have43976/61540 triangles versus
46212/61064 before. Their simple repeated window programmes remain visually
monotonous; broader facade composition, roof equipment, complete architecture
families and overall CityEngine-level variety remain open. The new long upper
masses and larger terraces improve form variation only.

## References

SIGGRAPH2003 *Instant Architecture* attributed split/component composition,
page4/Figures5–6, was optically inspected. Existing SDK envelope/setback
references and a fresh bounded Ghidra examination distinguish geometric
setbacks from functional room planning:
[SDK observation](../../../doc/references/buildings/esri/podium-sdk-3.4.12206.md).
No ready Esri architecture recipe, complete source or runtime equivalence is
claimed; all programme code here is our own composition of existing operators.
