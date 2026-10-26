# Terraced mass and simple open-floor prototype

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), parent #32. This is
the first private experiment stage, not an integrated architecture family.
The previous podium path requires detailed room plans before fitting new upper
forms; only eight complete assets passed its existing whole-city controls.
This prototype plans exterior profiles and simple common access first.

## Method and scope

`terraced_mass_profiles.py` composes three nested profiles: asymmetric successive
setbacks (`cascade`) or two bars with a connecting wing, followed by one retained
bar (`paired-wings`). Supplied ground footprint, holes, heights, properties and
source identifiers remain. Profile proposals are bounded and unsuitable sources
are rejected. Source-local orientation and stable source seeds choose geometry;
neither place names nor cameras select architecture.

`terraced_mass_plan.py` assigns repeated storeys to these three profiles. It uses
existing facade-cell entrance selection and reserves a common real stair core.
Every profile must retain connected actor-clear space; a bounded ground route
connects the selected entry to the core. Supplied levels are honoured if safe.
`terraced_mass_geometry.py` directly unions three closed mass bands for the bare
exterior; it does not require a detailed room partition.

`terraced_mass_rooms.py` cuts simple open floors, actual slab apertures and the
ground door from that mass. Existing closed stair flights and intermediate
landings connect the floors. The resulting shell, stairs and arrival pad remain
three independently closed components. All construction uses one canonical core
scope, followed by one placement. Void metadata is placed from the actual local
cutters; offsets are not recomputed after placement.

`terraced_mass_collision.py` queries the interval union of those three decoded
solids. It does not remesh their touching surfaces. Triangle normals, metric UVs,
source identifiers and material owners are retained through the existing tools.
There is no persistent asset format or new shared library contract in this stage.

## References and rejected alternatives

The local [SIGGRAPH 2006 building paper](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
p.3/Fig.4, was visually inspected before the profile study. Existing bounded
[SDK/Ghidra setback evidence](../../../doc/references/buildings/esri/podium-sdk-3.4.12206.md)
informs scope/offset composition. Its geometric `core area` does not prove a
functional stair core. No proprietary implementation or CityEngine equivalence
is claimed.

World-coordinate stair construction initially lost approximately14mm of sampled
tread support after a Boolean roundtrip. Constructing only stairs locally was
insufficient for all core choices. One common local construction scope fixes the
component geometry. An additional final shell/stair/pad Boolean was rejected:
its first paired-wing case changed volume by0.00142124m³ on re-import. Independently
closed components pass the same1e-7m³ volume/1e-7m² surface gates and avoid that
unnecessary remeshing. These are not claimed to form one boundary mesh.

Repeating cavity offsets in world coordinates also changed a near-coincident
corner by0.00569416m² in an actual source. Actual local cutter metadata is now
placed once. The independent section oracle operates in the declared metric
scope and verifies the complete placed component and cutter coordinates as well.
No earlier geometric or physical tolerance was relaxed.

## Checks and actual pictures

Frozen procedure/test revision `d5ed8b6e4`:12 source and12 fresh-installed-wheel
tests pass. Four unchanged actual OpenFreeMap Paris fixtures from
`data/stepped-profiles.json` cover two-hof, single-hof and concave outlines;
additional analytic rectangle/concave mass controls and invalid/narrow/low inputs
remain. No fixture geometry or tags were invented for those real sources.

Checks include independent mass volume/surface formulae, profile sections and
heights, immutability/IDs, all slab-interval shell sections and integrated volume,
three closed component roundtrips and independent signed triangle volumes.
All872 actual stair poses pass the existing sampled foot/headroom gate. Straight
arrival and every ground-route segment pass the unchanged continuous prismatic
tube gate; stair hull disjointness composes that ground-route proof. Removing
actual stairs fails despite retaining the proposed path. Stair transitions remain
sampled, not a continuous native-controller proof.

Four actual1920×1080 Eevee images at `d215799ae` were opened: both programmes on
the first real source, each whole and explicitly clipped/capped through its core.
The subsequent cutter-metadata fix leaves their render arrays unchanged; replay
is checked separately. Sixteen earlier matched bare-mass before/after images and
eight plan diagrams at `8df5692d4` were also opened. Those earlier frames are
mass-study evidence, not pictures of the current functional plan or a city gain.
The current cutaways show open floors and real stairs; exteriors are still bare.
Software-render times are not GPU/native/frame-budget evidence.

Reproduce with `.venv`, `TMPDIR=/tmp`, and
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`:

```sh
.venv/bin/python -m unittest -v verify_terraced_mass verify_terraced_mass_rooms
.venv/bin/python test/experiments/buildings/terraced_mass_room_probe.py --output /tmp/outshine-buildings-openfloor
.venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-openfloor/cascadeCutaway-geometry.json --output /tmp/outshine-buildings-openfloor/renders
```

## Still open

The subsequent [physical facade study](terraced_facades.md) adds actual windows,
frames/panes, band/pier programmes and complete facade-aware ground-arrival checks.
The evidence above remains the original bare-mass/open-floor stage.

Broader facade programmes, roof edges, terrace portals/rails, neighboring access,
source-family selection in the complete988-source window, matching near/far
products, cost/work/allocation/byte/peak comparisons and library promotion remain
in #618. Previous complete podium/hall/facade/roof products retain their own path
and gates. No native integration, cache/gameplay admission, night-light quality,
worldwide coverage or CityEngine-level exterior quality follows from this study.
