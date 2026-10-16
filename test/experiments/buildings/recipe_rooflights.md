# Explicit glazed industrial hall v2

Owner: [#500](https://github.com/Weichwolf/outshine/issues/500). The normal hall
builder now accepts `HallDetails(entrances, states, version="2")`. It retains
the existing source footprint/height, generic room and functional access, adds
camera-independent rooflight rectangles and cuts their actual lightwells once.
Details use the installed direct closed 44-triangle panel primitive. Default
version `"1"` and its loadable definition retain their semantics.

The same v2 has a small loadable definition in
[`buildings-industrial-hall-v2.json`](../assets/recipes/buildings-industrial-hall-v2.json).
It uses the unchanged experimental graph kernel from #494/#483. There is no
new shared library/native/cache ABI or central index. New domain ports are
`buildings.rooflight-plan/1`, `buildings.rooflight-panels/1` and explicitly
`buildings.hall-shell/2`; the altered shell is not disguised as shell v1.

## Preparation and retained products

With `.venv` and `PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings`:

```python
from recipe_hall_operators import inputs_for
from recipe_rooflight_operators import prepare
from recipe_rooflight_products import assemble

graph = prepare()
inputs = inputs_for(provider_row, neighbors)
logical = graph.evaluate(inputs, outputs=("programme", "rooflights"))
# Only choice, programme and rooflight plan run: no envelope, CSG or panels.
retained = graph.evaluate(inputs, outputs=("programme", "rooflights", "shell"))
mesh, parts, receipt = assemble(retained, view, opened=0.5, staff_angle=90)
```

These are alternative requests: calling `evaluate` again repeats its requested
work. It has no inter-call memoization. A retained five-operator result supplies
the complete glazed physical body and descriptors. `assemble` changes view/gate/
door detail without repeating source planning or CSG; absent retained panels are
emitted directly. A six-operator full request retains those panels too. Requesting
only `panels` runs four operators without building an envelope or CSG shell.
No persistence/asset-loader behaviour is inferred from this in-memory experiment.

Normal tile composition selects the same version without prototype patches:

```python
owner = SteppedDetails(rows, view)
owner.facades.halls = HallDetails(owner.facades.entrances, version="2")
mesh, source = construct(place, prepared=(rows, receipt), ordinary_builder=owner)
```

The existing tile demand keeps its complete logical glazing before deferring
hall geometry. Its fallback compact roof is still opaque: this is an explicitly
open far-product limitation, not a valid transition or full-world LOD admission.
The small graph supports requested outputs, not automatic camera-distance policy.

## Actual controls and scope

Three stored provider halls retain 55/16/46 fields, original properties/height/
footprint and room/access IDs. New controls compare the real normal tile adapter,
typed graph and independent direct prepared construction, including actual
arrays/dtypes, physical collision/access for three gate states, true roof holes,
height invalidation and stable identities. Missing capacity rejects before mesh
work; full byte reservation rejects before callbacks. Panel count, actual Float32,
nonfinite coordinates and mutation have negative controls. Eleven v2 controls
join the existing rooflight/hall/graph checks; terminal results belong to the PR.

Six fresh processes (two per source) reproduce logical/type/fingerprint/byte
receipts and all 720 compared body/panel array pairs including dtypes. The
existing actual direct-closed near and far render inputs replay exactly against
v2 (all arrays/dtypes/source palette). Six source/camera inputs are exported;
two fresh actual 1920×1080 Eevee images are optically checked. Old images remain
old references. The final whole-tile probe separately checks every original
source member, attribute, height interval, footprint, classification and palette;
JSON sequence normalization compares stored lists with live tuples. It excludes
only the deliberately changed procedural prototype metadata from that source
comparison. Independent normal-adapter/graph tests cover actual hall semantics.

The whole original Grohnde tile retains 37 sources, eight logical halls, six
constructed halls and 228 planned fields; constructed halls emit 148 fields.
It uses 72,523 triangles like the complete CGA reference, versus 60,123 for the
earlier opaque hall tile. The previous unsuitable loading entry keeps its reason.
The optional contact-skin reduction remains a separate #507 experiment.

## Equal-product cost comparison

Frozen `0dab01b5c`, normal-priority shared four-logical-CPU Linux host with the
nice-10 source checks running concurrently. Each method runs 21 warm repetitions.
The direct domain chain and graph produce the same logical and complete outputs.
Milliseconds below are medians; byte accounting is known unique arrays plus
encoded descriptors, not total Python/native heap or all copied bytes:

| Source | Fields | Logical domain ms | Logical graph ms | Full domain ms | Full graph ms | Logical accounted bytes | Full accounted bytes |
|---|---|---|---|---|---|---|---|
|0|55|30.925|31.597|165.224|178.179|60494|1988187|
|1|16|23.369|40.669|82.072|87.311|26811|794496|
|2|46|35.266|36.148|162.346|164.896|55308|1812097|

Logical graph evaluation executes 3 instead of 6 operators. Traced peaks are
258,899/92,825/225,975 bytes logically versus 2,857,157/1,224,015/2,659,066 fully.
These are scope-specific observations under shared load, not an A18/RTX/CPU-frame
budget or uniform graph speedup. Graph hashing/type accounting adds work. Template
cold costs remain separately recorded by #507; body CSG, library internals,
allocation count and total traffic are not silently removed from the comparison.

## Reproduction

Use a frozen checkout, existing stored provider inputs and explicit temp outputs:

```sh
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python -m unittest verify_recipe_rooflights verify_rooflight_surfaces verify_industrial_rooflights verify_industrial_halls verify_recipe_halls verify_recipe_graph -q
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python test/experiments/buildings/recipe_rooflight_probe.py --output /tmp/outshine-buildings-hall-v2 --references /tmp/outshine-buildings-rooflights-cameras-output
PYTHONPATH=lib/src:test/experiments/assets:test/experiments/buildings .venv/bin/python test/experiments/buildings/recipe_rooflight_tile.py --reference /tmp/outshine-buildings-rooflights-tile-output/GrohndeRooflightsGlazed-geometry.json --output /tmp/outshine-buildings-hall-v2
```

Source reference inputs are reproducible with [industrial_rooflights.md](industrial_rooflights.md).
The geometric/component reference remains SIGGRAPH 2003 *Instant Architecture*,
page4 optically inspected, and the prior bounded SDK/Ghidra primitive study;
no ready Esri hall recipe, full CGA implementation or new Esri oracle is claimed.
Complete direct far roofs, per-field distance demand, transition/shadow/night
bounds, broad architectural families and native integration remain open in #500/#32.
