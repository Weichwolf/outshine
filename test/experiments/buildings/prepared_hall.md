# Retained complete industrial hall assets

Owner: [#500](https://github.com/Weichwolf/outshine/issues/500).
The previous v2 whole-tile path retained eight logical glazed halls but emitted
only six physical glazed bodies. Repeating a view/state assembly also rebuilt
unchanged windows, ramp, door frames and roof panels. This experiment prepares
complete source-owned hall geometry once and selects existing draw references.
The ordinary v1/v2 builders and shared recipe/library semantics stay compatible.

## Asset, instance and export

`prepared_hall.prepare_asset` requires matching v2 programme, shell and logical
glazing ports in one frame, with unique bounded source-owned fields. It retains
the complete body, fixed details, full 44-triangle physical roof panels and
direct 20-triangle render sheets. Default closed personnel/gate leaves and eight
metric bounding corners per field are also prepared. Mappings and arrays are
immutable. Both panel forms remain useful: full panels belong to the physical
product even when sheets are selected for rendering.

`HallInstance.update` checks strict finite gate/door state and projects the
prepared bounds using the existing conservative geometry policy. Camera-only
updates neither plan sources nor emit mesh arrays. An unchanged state and
selection returns the same draw record. A changed door angle emits only its leaf;
a changed gate lift emits full and flat gate leaves when nonempty. Static parts
remain shared. Invalid updates preserve the previous instance.

Collision always uses full roof panels and full ribbed gates, independently of
camera/render selection. This intentionally improves on the old diagnostic
assembly, which could use a view-dependent flat gate for collision too. Independent
loading/personnel rays, supported ramps and internal routes check physical
function. The render-only open sheets must never become collision solids.
The existing 0.06 m / projected default 0.5 px roof bound remains a geometry
bound, not a bound on normals, radiance, glass, shadows or temporal transitions.

`prepared_hall_tile.prepare_halls` seals the complete source snapshot and builds
eligible assets. `RetainedDetails` consumes those exact snapshot rows; foreign
same-ID rows reject instead of silently using stale geometry. Changed source
content requires a new asset; graph fingerprints change while logical room IDs
remain stable. Unsupported/rejected hall candidates retain the existing ordinary
builder and a reason. This is a hall study, not a prepared whole-world cache.

The explicit full-field diagnostic reference uses the same eight bodies, state,
materials and gate detail as the adaptive reference. `append` and the existing
whole-tile constructor still copy complete bodies when exporting diagnostic
NPZs. That export is separately measured; it is not a proposed frame loop.
No persistence serializer, native API or new source-provider semantics is added.

## Whole source and picture evidence

The fixed original 9441-byte OpenFreeMap tile and camera live in
[`data/provider/grohnde/`](data/provider/grohnde/README.md), with receipt, hash and
attribution. Existing decoders accept an explicit source directory; private
caches and network access are unnecessary. All 37 original building sources
remain, with unchanged IDs, properties, footprints, heights, classes and palette.
Eight eligible halls retain all 228 glazing fields; one unsuitable loading
candidate and one ineligible multistorey candidate keep ordinary geometry with
reasons. They are not omitted source buildings.

| Same complete source tile | Constructed glazed halls | Fields | Triangles |
|---|---:|---:|---:|
| Previous ordinary v2 | 6 | 148 | 72523 |
| Retained full field reference | 8 | 228 | 90273 |
| Retained adaptive field reference | 8 | 228 | 84801 |

The equal eight-hall field reduction saves 5472 triangles, about 6.1% of this
complete tile. Comparing the six-hall baseline directly would mix different
coverage. Complete physical bodies remain stored and drawn at full resolution;
this is not a complete distant-building LOD.

Two actual 1920×1080, 16-sample Eevee images at `43b847f4a` were opened alongside
the prior whole-tile image. Backend: llvmpipe LLVM 19.1.7; both processes ended0
after the established flush/post-render teardown workaround. The old baseline
arrays, dtypes, camera and palette replay exactly. Full versus adaptive changes
3259/2073600 pixels, worst channel19/255, mean channel0.002233/255. New full versus
previous v2 changes only674 pixels, concentrated in the two distant halls.
This is a small coverage/function improvement, not high architectural variety
or a dramatic picture improvement. Gray/repetitive facades and coarse special
structures remain visibly below the architectural quality target.

## Reproduction and limits

Use `.venv` and an explicit system-temp output directory on a clean frozen commit:

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/assets:test/experiments/buildings
.venv/bin/python -m unittest verify_prepared_hall verify_prepared_hall_tile -q
.venv/bin/python test/experiments/buildings/prepared_hall_tile_probe.py --output <system-temp>/models
.venv/bin/python test/experiments/buildings/prepared_hall_costs.py --output <system-temp>/costs
.venv/bin/python test/experiments/buildings/blender_city.py <system-temp>/models/GrohndeRetainedAdaptive-geometry.json --output <system-temp>/renders --samples 16
```

The existing `blender_city.py` consumes the exact frozen mesh. On this machine
the known bpy teardown failure is bypassed only after `render` returns and both
streams flush, with `os._exit(0)` in a small caller. No normal-teardown or GPU
admission is inferred. Use `test/scripts/pixels.py` for the paired PNG comparison.
Images stay private in system temp; no gallery or server is added.

Checks cover exact existing render products across actual halls, distances and
gate/door states; unchanged references with real builders patched to fail;
transactional invalid updates; ownership; source invalidation; whole-source
accounting; independent physical routes; and actual lightwell rays on all eight
halls. Legacy render-input replay guards the shared helper extraction.

The cost probe checks exact arrays/dtypes outside timing for repeated assembly,
retained references and diagnostic export at the same source/camera/state.
It separates one-time graph/body and additional prepared-part construction,
first/21 subsequent calls, traced peak and unique geometry-array bytes. Camera
motion still projects bounds and allocates small decision arrays; only new
geometry work is zero. Retaining both roof forms has a memory cost. Graph,
source/solid backend, templates and Python-object memory are outside the unique
geometry count; traced peak does not measure all native allocations/traffic.
No frame, GPU, target-device or serialized asset-cache result is claimed.

Frozen `43b847f4a`, normal priority on the shared four-logical-CPU Linux host,
21 subsequent calls. Load averages were10.94/11.14/10.75 at the start and
11.05/11.13/10.77 at the end; no exclusive host result. Values below are
median milliseconds for the camera sequence, then traced peak bytes from one
separate following camera call:

| Fields | Repeated assembly ms / peak bytes | Retained references ms / peak bytes | Diagnostic export ms / peak bytes |
|---|---:|---:|---:|
|55|232.564 / 4880303|0.285 / 29951|1.901 / 3122423|
|16|131.173 / 2402511|0.252 / 10208|1.054 / 1471248|
|46|254.293 / 4765630|0.307 / 25190|2.164 / 3006850|

One-time graph/body times were174.152/83.846/172.439ms; additional prepared
parts cost270.007/132.613/255.474ms. Unique retained geometry occupies
3011172/1401708/2859368 bytes, of which the complete bodies occupy
1158148/489836/1078640 bytes. Bound arrays add10656/3168/8928 bytes. These
counts include both full/sheet geometry but exclude the other retained objects
listed above. Camera changes copy zero geometry bytes; selection still visits
444/132/372 bound points. State-changing reference medians are3.550/3.449/3.513ms
because moving leaves are rebuilt; unchanged static parts are shared.
This removes measured repeated work, not the one-time asset cost or complete
body draw/transfer cost. The cost JSON retains all21 samples and scoped peaks.

SIGGRAPH2003 *Instant Architecture*, pp3–4, motivates component replacement;
SIGGRAPH1996 *Simplification Envelopes* motivates restricted geometric bounds.
The local PDFs and their actual figures were optically inspected for the prior
rooflight studies. Existing hash-bound SDK/Ghidra primitive observations are
reused; no new reverse-engineered recipe or Esri-render equivalence is claimed.
Full-body simplification, stable transitions, shadow/night selection and broader
functional architecture remain outside this bounded delivery.
