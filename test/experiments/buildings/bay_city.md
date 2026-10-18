# Facade fields in the complete source window

Issue [#741](https://github.com/Weichwolf/outshine/issues/741), following the
[ribbon/paired-bay controls](bay_facades.md). `bay_city.py` applies both existing
programmes to the independently checked coupled buildings, preserving the
complete previous draw as an exact prefix. It adds no new architecture family,
provider API, native renderer or automatic cache/LOD policy.

## Source reconstruction and physical boundary

After reboot the old private `/tmp` products were absent. The versioned earlier
reports remain historical evidence. The complete census and body/roof audit are
rebuilt from the existing provider snapshot, without refreshing sources or
changing original gates. The census again contains988 sources, the original80
requests,12 protected functional owners,71 proposed bodies and one retained
failed candidate; programme counts remain40 cascade/31 paired wings.

The existing full audit still covers each coupled body, roof and complete draw.
`bay_city.load` retains `admitted` and `unchanged_modelling`: a partial audit,
changed source, lost functional owner, different physical receipt or changed
mesh cannot supply the comparison. No receipt is rewritten to admit changed
code. The fresh base uses the unmodified previous whole-city producer.

`bay_city_checks.inspect` additionally compiles every added layer as a closed
solid, checks positive volume against area×depth at1e-7m³, intersects protected
regions at1e-10m², and tests actual source window/portal/entry openings with3×3
compiled rays. Back-cap samples must be inside the corresponding source wall
and within0.01m of its boundary. Existing continuous finite-actor entry and
terrace route tubes are checked against actual closed additions. These are the
existing scoped tolerances; arbitrary roaming/controller, Float32 runtime,
shadow, radiance and device admission remain separate.

The existing base-body demand chooses outer layers for expanded bodies and
direct fronts for compact bodies. Both retain the coloured front fields; no
far-side construction is performed for a front request. This is a geometry
comparison, not evidence that one position criterion admits shading changes.
Every candidate and rejected decorative cell is reported. Rejected cells keep
their original source wall/window, never remove a building.

All six original numeric arrays remain byte-exact prefixes, including original
index values, signed zero and dtype. Comparison scratch is bounded to1MiB,
rather than copying both complete arrays into Python byte strings. Original
windows, bodies, roofs, heights, materials and functional owners remain intact.
The private60005/60006 metallic-roughness bindings are collision-checked.

The existing split/component/extrude primitives remain as in the preceding
study. Local SIGGRAPH2003 Instant Architecture PDF-page3/Figure4 was optically
reopened for facade hierarchy/attribute separation. The pinned SDK/Ghidra
primitive references are not a supplied Esri architecture recipe.

## Complete result and actual images

Frozen physical/base producer:76f679351. The census takes25.743s after source
setup; the complete scoped body/roof/draw audit passes71/71,221 roof components
in716.518s, worker peak424068KiB. The full old/base reconstruction takes788.636s
after source/view setup, peak3852436KiB, including old fitting, before assembly,
new bodies, hashes and exports. These are diagnostic costs under shared load,
not runtime budgets or comparable construction-only measurements. The rebuilt
before NPZ again matches historicald48fe4dccaa47639fc2a43f06b99cf1f9040e47e61445696254c68f30fba6b37.

The first field run checked71 ribbon sources, then failed on an auxiliary
landuse row without a building ID. No complete result was claimed. The corrected
producer7bac9024e filters the declared layer before reading building identity.
A real compact-body/mixed-landuse control reproduces this boundary. The public
loader still requires the full988-source/71-body audit; internal composition
checks its complete admitted membership. Original gates are unchanged.

Both corrected whole-window variants finish0 with all988 sources,12 protected
functional owners, and byte-exact original numeric prefixes. Sixty sources use
outer products,11 direct fronts in both variants. Rejected decorative cells
keep the old facade; all71 bodies receive valid fields. All12664 added closed
layers pass, including370521 actual aperture rays and218264 wall-back samples.
Maximum layer-volume discrepancy is2.632e-14m³, below the unchanged1e-7m³ gate.

| Complete scene | Draw triangles | Draw-array MiB | Cells / rejected cells |
|---|---:|---:|---:|
|Coupled base|2656495|701.357|—|
|Ribbon fields|2855019|753.990|3038 /1494|
|Paired bays|2984853|788.412|4813 /650|

Field stages, summed over the same71 sources:

| Programme | Plan s | Closed diagnostics s | Draw expansion s | With join/check/export s |
|---|---:|---:|---:|---:|
|Ribbon|20.266|28.098|7.349|75.350|
|Paired|22.660|60.856|13.716|117.700|

Plan time includes the coupled body/terrace plan. Diagnostics are separated from
expansion; the last column also includes whole-array assembly, prefix checking,
framing, hashing and compressed export. Whole field-worker peak2459596KiB
includes the loaded base, plans, diagnostics and multiple exported products.
Draw-array bytes are retained numeric payload, not heap/VRAM/residency. Complete
allocation/copy traffic is unmeasured; this offline append is no runtime cache
or lower-memory claim. The preceding equivalent front/outer construction study
remains the operation-level alternative comparison, unchanged by composition.

Five new checks pass on frozen7bac9024e; the unchanged six comprehensive facade
methods also pass in the earlier59.214s combined run, covering28 actual plans.
The mixed-data counterexample has its own failing then corrected control; the
full corrected142-source/programme run is independent positive coverage. Three
existing affected Transport prism controls pass. Format/Ruff and diff checks
pass; native/device gates are outside this Python-only change.

The18 Near/Whole/Opposite control images and two elevation sheets are freshly
regenerated on914073b2d and all optically opened. Original mesh prefixes, file/
image/report hashes, imported triangle counts and paired camera/light/glass
settings are checked. The four actual1920×1080 whole-window images are also
freshly rendered and opened: old pipeline, coupled base, ribbon and paired.
The two base renders retain renderer914073b2d provenance; field renders use
7bac9024e. The renderer/probe/modelling chain itself is unchanged. All complete
images use the same camera, daylight, ground and source-glass parameters; the
coupled/field trio retains identical panes. Every intended triangle is imported.
Known shader/shadow/allocation error strings are absent from these capture logs.

Linux8-logical-CPU/32GiB-class host, Python3.13.5, NumPy2.5.4, Shapely2.2.0,
Manifold3.5.4, Blender5.2.2LTS/Eevee16, Xvfb/llvmpipe LLVM19.1.7, two render
threads per worker. Two nice10 captures run concurrently; source composition
and other agents vary the load. Hero renders39.227–60.680s, worker peaks
1959852–2434728KiB. The coupled/ribbon/paired whole renders take68.899/78.493/
79.819s, peaks4838576/5174588/5420356KiB. Peaks include software-driver resources;
these are not RTX/Cycles timings, an isolated ranking, GPU memory or frame time.

Near and opposite captures show coherent horizontal fields and deeper paired
rims/shadows. In the full window, those groupings remain visible on the71 new
bodies, especially the larger central elevations. The overall gain is bounded:
the city remains pale and repetitive, with large bare roofs, common window
rhythms and overlapping old bands/piers. `test/scripts/pixels.py` measures36165 changed pixels for ribbons and42007
for paired bays against the same coupled base (1.744%/2.026%); mean RGB-channel
difference0.271867/0.296792 of255. No pure-/near-black pixels appear. These
single-view differences describe image change, not a universal quality or LOD
score. Neither complete architecture-family variety nor CityEngine-level visual
quality is established. Further coherent
exterior programmes remain #618/#32; runtime/cache/radiance/LOD admission #33.

## Reproduction

A clean frozen checkout, `.venv` and the existing hash-bound provider source
cache are required. No historical private model or audit is required: run all
four explicit stages. Keep the producer frozen until each stage finishes.

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
.venv/bin/python test/experiments/buildings/terraced_mass_census.py --output /tmp/outshine-buildings-facade-city/census.json
.venv/bin/python test/experiments/buildings/terraced_crown_census.py --census /tmp/outshine-buildings-facade-city/census.json --physical-roofs --full-bodies --output /tmp/outshine-buildings-facade-city/cohort.json
.venv/bin/python test/experiments/buildings/terraced_city.py --audit /tmp/outshine-buildings-facade-city/cohort.json --crowns --output /tmp/outshine-buildings-facade-city/base
.venv/bin/python test/experiments/buildings/bay_city.py --base /tmp/outshine-buildings-facade-city/base/CrownedAfter-geometry.json --audit /tmp/outshine-buildings-facade-city/cohort.json --output /tmp/outshine-buildings-facade-city/fields
LP_NUM_THREADS=2 LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a .venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-facade-city/fields/CityBays-geometry.json --output /tmp/outshine-buildings-facade-city/images --samples 16
.venv/bin/python -W error -m unittest verify_bay_city verify_bay_facades
```

Repeat the renderer with the base and ribbon geometry for the paired images.
All outputs stay private in `/tmp`; no gallery, server or published image.

Current private evidence roots: `/tmp/outshine-buildings-city-rebuild-data-76f679351`
(census, full audit, exact base models),
`/tmp/outshine-buildings-facade-city-controls-914073b2d` (18 captures, elevations,
assessment), `/tmp/outshine-buildings-facade-city-models-914073b2d` (actual base
captures), and `/tmp/outshine-buildings-facade-city-models-7bac9024e` (both field
models, captures, measurements and complete assessment). Original provider
inputs remain in the existing source cache, separate from these private outputs.
