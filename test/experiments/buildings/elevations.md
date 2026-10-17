# Source-stable base, body and top facade scopes

Owner #574, parent #32. Pure Python experiment, no new public library or native API.
This adds two bounded facade programmes, not complete architectural families.

## Result and remaining image gap

The previous broad cladding still repeated one window grid across whole elevations.
Suitable free elevations now have three source-stable vertical scopes with distinct
window widths, heights and column counts. Residential controls use tall narrow
punched windows; commercial/institutional controls and selected mixed-use sources
use broad horizontal glazing. A different base and upper row make the hierarchy
readable. The original entry elevation retains its shop/balcony/bay programme.

The matched whole988-source Paris window changes44 sources,91 elevations and273
scopes:83 vertical-punched and8 horizontal-ribbon elevations. No source is omitted.
All eight existing podium/wing assets and twelve old mass/function records survive
exactly except timing, as do source IDs, properties, footprints, height intervals,
colours, placement, protected entrance/shop/projection and roof plans.
Triangles429229→421543, minus7686. This reflects different facade geometry, including
replaced old cladding, and is not an equal-quality algorithmic performance claim.

All22 actual1920×1080 Blender rasters and two2D opening plans were opened: paired
near/opposite/elevated/far views of two real sources, paired facade crops, and the
whole city. The narrow and wide openings are visibly distinct; near crops retain
real recesses/frames. Whole-city variation improves locally but remains small:
51997 changed RGB pixels out of2073600, mean absolute RGB difference0.505491/255.
Pixel difference is not itself a quality score. The neutral city still lacks rich,
coherent massing, material composition, roof hardware and complete architecture
families. Large glass fields need richer subdivisions. Simple dark window backs
are not a completed interior or night-light programme. CityEngine level remains open.

## Construction and contracts

`elevation_plan` partitions each suitable rectangular elevation into contiguous
base/body/top zones. Storey count is retained; each resulting storey stays at least
2.64m high. Width must be at least12m, resolved levels3..16, ordinary source area
800..6000m². Supplied window fields, unsupported roofs, clipped/raised/short/low
sources and blocked elevations retain the previous owner. These are deliberately
bounded eligibility rules, not coverage of all world buildings.

All source elevations use common horizontal cut planes. `elevation_cut` reuses the
existing CGA open-surface slab cutter on actual triangles and retains exact cap and
corner points. Binary scale with zero origin avoids the one-ULP cap displacement
caused by tight decimal scope normalisation. Independent native manifold, volume,
area and symmetric-difference checks reject open seams. No tolerance was relaxed.
An earlier quad reconstruction was discarded because it lost original roof joins.

Scope-local metrical UVs feed the existing procedural facade shader. Near views use
the same existing recess operators with explicit vertical scope bounds. Classic
frames/bands remain on unmodified elevations, without being duplicated across the
new scopes. Two default-preserving hooks in `FacadeDetails` allow single entry
planning and scoped recess dispatch. Later facade failure retains the original
exterior and cannot advertise an unapplied architecture programme.

The logical programmes of the controls are identical across all five camera views,
including the old hip-roof coarse path. This is not proof of universal persistent
cache, LOD/shadow/night/temporal behaviour. Source eligibility is camera independent;
the inherited source/function owners retain their previous experimental limits.

Local SIGGRAPH page3/Figure4 was optically inspected; bounded fresh Ghidra inspection
of the SDK split functions is documented in
[the SDK reference](../../../doc/references/buildings/esri/elevation-scopes-sdk-3.4.12206.md).
No Esri source, complete recipe or render oracle is claimed.

## Functional and package evidence

Geometry/render producer527e62954; the additional cropped-view producer834ccf777
changes presentation only. The later failure guard adds an explicit negative case.
Actual source fields/arrays from all20 saved model/crop products were replayed
against the final implementation, without rewriting image receipts.
All44 saved city programmes have exactly their expected applied scope frames.

Source30 facade/roof/old-path tests and13 shared contract/pipeline tests pass at
ae7f80558. The fresh installed wheel passes all43 together. A previous installed
package also passed30 facade tests. The fresh package contains205 byte-exact modules,
including the newly merged constant height-query contract; the affine experiment
is not consumed by this facade feature. Original wheel receipt is retained under
`/tmp/outshine-buildings-elevations-wheel-ae7f80558/`.
Wheel SHA256 `a7b0ba01618cd35b5314a12319fc24d0cfd7b2d23e194ddc1031400f3c8ae13e`.

The seven new controls cover partitions/safe storeys/stable IDs/source immutability,
single entry planning and protected functions, independent closed-body comparison
and metric UVs, actual open near fronts, view-stable programmes, exact old low-body
fallback/invalid inputs, and no false applied-scope claim after a later failure.
Existing23 facade/crown/frame controls remain unchanged in specification.

## Scoped costs

ca4841e2d measures the same pre-opening bound wall products: one cold call,21 warm
calls and a separate tracemalloc sample. Source/roof setup, recesses, export and
rendering are outside these timings. Four logical CPUs, concurrent software render
and city work; load averages15.36/13.36/11.83. These are host research timings.

| Actual source control | Scalar choice p50 ms | Wall compile p50 ms | Slab triangle visits | Output bytes | Memray allocation events / requested bytes |
|---|---:|---:|---:|---:|---:|
|0 courtyard26m|1.782|123.877|192|114752|441994 /50130380|
|1 courtyard30m|1.742|67.453|96|56820|222081 /25023323|
|2 service22m|2.533|48.332|72|42476|167308 /18784648|
|3 mixed-use26m|0.849|58.333|78|114728|181515 /21814005|

Warm return of already resident immutable wall arrays takes roughly0.0001–0.0002ms,
with zero mesh copies. It still allocates a small Python return tuple:3–4 tracked
events,328–392 requested bytes. This is a bounded resident-product comparison, not
a persistent cache hit or whole-building load result. Recompiled arrays and frame
metadata are exact. Full output arrays give only a lower bound on compilation
writes; memray records total requested allocation traffic separately. Scoped
memray high-watermarks152994–503566bytes are not process/device peaks.
Repeated source-wall clipping is expensive in Python; no native frame, GPU, A18 or
total-memory admission follows. Runtime assets should reuse prepared products.

## Reproduce

Use `.venv`; set `TMPDIR=/tmp` and
`PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache`.
Set `PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`.

```sh
.venv/bin/python -W error -m unittest verify_elevations verify_roof_crowns verify_clad_facades verify_facade_frames test_reservation_query test_reference
.venv/bin/python test/experiments/buildings/elevation_probe.py --output /tmp/outshine-buildings-elevation-models
.venv/bin/python test/experiments/buildings/elevation_probe.py --detail-only --output /tmp/outshine-buildings-elevation-details
.venv/bin/python test/experiments/buildings/elevation_city.py --output /tmp/outshine-buildings-elevation-city
.venv/bin/python test/experiments/buildings/elevation_costs.py --allocations --output /tmp/outshine-buildings-elevation-costs.json
```

Freeze the producers before image/cost evidence. For every generated geometry JSON,
use `roof_crown_render.py --geometry <file> --output /tmp/<run>/renders`.
Blender5.2.2, Eevee16 samples, llvmpipe LLVM19.1.7 software rendering; the renderer
exits after verified save to avoid known bpy teardown failure. This does not prove
normal Blender teardown or a hardware rendering budget. Both city passes declare
the same10000m flat ground and preserve the original camera/light/base materials.
Whole two-pass model/export918.08s under shared load, not a cache-load benchmark.

Run `elevation_review.py --models <models> --details <details> --city <city>
--output /tmp/<review>.json` for actual raster hashes, matched settings and source
retention. Current originals are in `/tmp/outshine-buildings-elevations-model-527e62954/`,
`/tmp/outshine-buildings-elevations-detail-output-834ccf777/` and
`/tmp/outshine-buildings-elevations-city-527e62954/`. Costs and memray originals use
`/tmp/outshine-buildings-elevations-costs-ca4841e2d*`; review/replay receipts use
`/tmp/outshine-buildings-elevations-{review,final-replay}.json`. No research images
are published under `doc/`, and no showcase server is started.
