# Direct exterior products before physical facade expansion

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following
[physical terraced facades](terraced_facades.md). The previous near product is
retained. A shared logical blueprint now precedes expensive floors, 3D Boolean
openings and frame solids. A separate direct exterior avoids those operations.
This is a bounded exterior experiment, not full architecture/cache/LOD acceptance.

## Construction and ownership

`terraced_facade_prepare.py` prepares the canonical local mass and stable window
plan. `build_facade(..., prepared=...)` validates that ownership before physical
construction. Window-capacity rejection now also precedes floor construction.
The unchanged physical product still owns the closed shell, stairs, arrival pad
and detailed frames/panes; its functional gates are retained.

`terraced_facade_surface.py` emits the complete exterior around the building:
profile wall panels with actual logical window/door apertures, flat glass/frame
regions, front faces of the existing bands/piers, original roof/terrace profiles,
underside and arrival pad. It retains source heights, courtyards, window IDs and
per-triangle owners. The open entrance is preserved. This draw surface replaces
neither physical collision nor the independent full near asset.

A first direct implementation at `919fb40b0` used a general polygon compiler for
every window/frame region. Fewer triangles did not make that implementation
cheap: on source3 it took1.478s, compared with0.993s for the higher-quality near
asset. The retained alternative batches equal window dimensions in
`terraced_facade_flat.py`: four rectangular frame-border regions, glass, mullion
and sill are expanded together from two dimension templates. General polygon
compilation is reserved for nonrectangular walls/pad. This avoids most Python
objects and tiny triangulation calls without changing the flat surface coverage.

Roof caps are triangulated in their original valid source polygons, then composed
with the locally generated exterior. Transforming a valid courtyard cap before
triangulation had produced a self-intersection in one actual case. No validity
repair or geometric tolerance was substituted for the original polygon.
Combined surface indices remain uint32 with an explicit capacity check; a replay
caught and removed an accidental64-bit promotion by a NumPy offset scalar.

`FacadeProducts` retains one blueprint, one immutable coarse product and at most
one lazily generated physical product. It decodes the source polygon once.
Rotation/movement select products without reparsing or replanning; explicit
physical demand creates the full near asset once. The lifetime is caller-owned;
there is no new global cache manager, scheduler or persistent asset format.

## Selection scope and physical controls

The existing conservative projection bound uses0.35m exterior displacement,
a fixed0.5px upper limit (or the caller's smaller limit), actual1920×1080
projection and movement extent. Interior camera positions force near demand.
The bound concerns exterior positions; it does not bound interior topology,
radiance, glass transmission, shadows, reflections or temporal transitions.
An invisible query may select the already resident coarse product, but does
not prove that unseen lighting/interaction work can be discarded. `resolve`
is a synchronous research helper, not permission to block a runtime frame.

Final frozen `33d435f17`:28 source and28 installed-wheel controls passed,
including all21 preceding physical/facade-arrival tests. Ruff check/format and
`make lint-docs` pass. Measured/rendered `b80e7a02b` differs only by import sorting
and equivalent merged test contexts; the module bodies also pass an AST replay
with those style changes normalized. All205 reference-library Python files at
the measured revision match the reused installed wheel; the later master update
adds an unrelated, unused vegetation module. Dependencies come from the main
`.venv`; no isolated environment is claimed.
An eight-program replay also matched every prior physical array and its
provenance, volume/area and window metadata exactly after blueprint extraction.

The7 new controls cover independent roof/underside area and source height,
wall-plus-glass area minus the real entrance, stable window ownership, unit
normals/read-only arrays, uint32 indices, forbidden floor/3D Boolean/detail
factories during coarse construction, capacity/wrong-owner rejection before
floor work, and rotation/movement/interior/physical reuse. Independent area
checks retain1e-7m² tolerances. Four actual sources × two programmes are eight
cases, not eight architecture families.

The discarded polygonal implementation is replayed against the batched variant
at the same source/window/material/depth planes. Triangle counts and IDs match;
maximum per-region symmetric-difference area is1.9004242624021117e-13m² across
all eight cases, below the unchanged1e-8m² check. This compares equivalent flat
coverage, not the different-quality physical near asset.

## Measured work and memory

Frozen `b80e7a02b`, Linux/x86-64, main `.venv`. Each method/source runs in a fresh
process; the table gives the median of three unprofiled builds. Imports, source
acquisition, mass planning and facade preparation are outside construction time;
preparation is reported separately (83.7/170.9ms in the two batch workers).
Host load was shared with verification work.
The near rows have additional physical/interior/depth quality and are context,
not a same-quality speed comparison. Polygonal/batched rows share flat coverage.

| Source / method | Triangles | Retained arrays MiB | Median ms | Allocation requests | Cumulative allocated MiB | Tracked heap peak MiB |
|---|---:|---:|---:|---:|---:|---:|
|0 near|41282|5.243|759.027|2506520|235.535|12.880|
|0 polygonal|5423|1.458|716.772|3591041|345.843|12.316|
|0 batched|5423|1.458|133.504|762291|80.410|5.337|
|0 resident query|5423|1.458|0.517|1238|0.136|0.011|
|3 near|81768|10.280|992.962|4775080|459.154|24.816|
|3 polygonal|10773|2.897|1478.487|7077678|680.068|24.364|
|3 batched|10773|2.897|248.343|1418193|149.215|10.426|
|3 resident query|10773|2.897|0.445|1238|0.136|0.011|

Memray1.20.0 traces a separate replay with Python allocator hooks and native
allocation interception, without native stacks. Original `.bin` traces and
`memray stats --json` results are retained in/tmp. Allocation requests and
cumulative bytes describe intercepted allocation traffic, not unique live bytes
or copy bandwidth. Resident-query heap excludes the already allocated asset;
its retained arrays remain in the table. Process-wide peak RSS spans imports,
normal/profile/counter replays:65.902–103.680MiB; it is not added to heap/output.
No GPU/A18/device frame, total allocator traffic or total copy bound is claimed.

At equal flat coverage the batch path is5.37×/5.95× faster, with78.77%/79.96%
fewer allocation requests. Direct polygon faces fall1181→92 /2321→150; their
input-coordinate buffers fall161376→32640 /319968→62784 bytes. Two flat dimension
templates replace per-window polygon regions. Final immutable-buffer writes are
1,529,286 /3,037,986 bytes; other copies remain unmeasured. These costs are still
local prototype measurements, not accepted whole-city runtime budgets.

## Actual picture comparison

Ten current1920×1080 images at `b80e7a02b` were rendered and optically opened:
four source/programme pairs at1200m and one close pair. Each pair uses the same
source/camera/material definitions/daylight. Blender5.2.2 LTS/Eevee,16 samples,
llvmpipe LLVM19.1.7 software backend; no GPU performance claim. Render receipts
verify every imported triangle, source/report/mesh/image hashes and projection.
All originals, inputs, terminal receipts and pixel diagnostics stay in/tmp.

The far images preserve the stepped silhouettes and repeated window pattern.
They are intentionally small in the full frame; this is a matched far-detail
control, not a close architectural showcase. The geometry-position bounds are
0.366–0.367px. `test/scripts/pixels.py` compares the actual full PNGs; a second
measurement uses the union of both projected geometry bounds plus4px, avoiding
dilution by the unchanged background. That region excludes cast shadows beyond
its bounds; full-frame statistics remain alongside it.

| Pair | Geometry-region pixels | Mean RGB error /255 | Worst channel error /255 |
|---|---:|---:|---:|
|0 cascade|2640|0.788|26|
|0 paired wings|2760|0.873|23|
|3 cascade|3355|0.898|21|
|3 paired wings|3355|1.004|32|
|forced close, full frame|2073600|4.641|151|

The close counter deliberately forces the direct product after selection has
requested near detail. Lost inset frame depth and sill volume are clearly
visible; this is a rejected use of the coarse representation. Even the physical
reference glass remains uniform here: through-glass rooms/night light are not
established. No new radiance/error gate is inferred from these few daytime
pairs, and no actual native LOD transition or complete-city gain is claimed.

## Remaining scope

Selected reusable near-window components remain open: a physical request still
builds the whole near building. Persistent cache/reload, asynchronous admission,
city hierarchy, shadow/reflection demand and native integration are unproven.
Roof/terrace portals, rails, richer architecture and the full988-source window
remain #618 work. This cost step creates room for that exterior development;
it does not complete #618 or establish CityEngine-level variety.

The local [SIGGRAPH2006 building paper](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf)
and [EGSR2010 facade encoding](../../../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf)
motivate plan-first composition and
compact repeated dimensions. Their inspected figures/equations do not constitute
an SDK oracle comparison or a claim of implementing their GPU encoders.

## Reproduction

Use `.venv` and set `TMPDIR=/tmp`, `XDG_CACHE_HOME=/tmp/outshine-buildings-cache`,
`PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache` and
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`.
Export the discarded implementation from the repository, then run in a frozen
checkout of the measured implementation:

```sh
git show 919fb40b0:test/experiments/buildings/terraced_facade_surface.py > /tmp/outshine-buildings-polygonal-reference.py
.venv/bin/python -m unittest -v verify_terraced_mass verify_terraced_mass_rooms verify_terraced_facades verify_terraced_facade_access verify_terraced_facade_products
.venv/bin/python test/experiments/buildings/terraced_facade_product_compare.py --reference-module /tmp/outshine-buildings-polygonal-reference.py --output /tmp/outshine-buildings-products/coverage.json
.venv/bin/python test/experiments/buildings/terraced_facade_product_cost.py --source 0 --programme cascade --method batch --repetitions 3 --trace /tmp/outshine-buildings-products/0-batch.bin --output /tmp/outshine-buildings-products/0-batch.json
.venv/bin/python -m memray stats --json -o /tmp/outshine-buildings-products/0-batch-stats.json /tmp/outshine-buildings-products/0-batch.bin
.venv/bin/python test/experiments/buildings/terraced_facade_product_probe.py --output /tmp/outshine-buildings-products/models
.venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-products/models/0-cascade-Direct-geometry.json --output /tmp/outshine-buildings-products/renders --samples 16
```

Repeat costs for sources0/3 and methods `near`, `polygonal`, `batch`, `reuse`;
`polygonal` requires `--reference-module` as above. Render all ten reports to
include both source/programme pairs and the explicitly forced-close counter.
For full-frame differences use `test/scripts/pixels.py <NearReference.png>
<Direct.png>` (or the explicitly forced-close pair). All research inputs/PNGs/logs
remain/tmp; no specialist gallery is published.
