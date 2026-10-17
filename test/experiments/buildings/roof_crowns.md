# Contained roof crowns and retained near facades

Owner [#551](https://github.com/Weichwolf/outshine/issues/551), exterior parent
[#32](https://github.com/Weichwolf/outshine/issues/32). Pure Python experiments;
no native, provider, reservation, material or shared-library API change.

## Method

`roof_crown_plan` partitions the actual source polygon into coping, inner guard
face and deck. A second programme adds a recessed solid service enclosure.
Courtyards remain void. Mitred offsets at0.16/0.28m define the perimeter strip;
a1.1m guard and up to3m service rise fit below the supplied top. These dimensions
and service use are procedural assumptions, not supplied roof plans or rooms.
The deck reserves height before facade/window layout. Supplied levels retain at
least2.64m per level below it. A finite axis-aligned candidate search places the
service rectangle with1.5m deck clearance; failure falls back to the parapet.
It does not implement Esri's largest-rectangle operator or claim optimality.

Admission uses source/form/context and stable source-ID seeds, not the camera:
previously flat, ordinary ground-level bodies,800<area<=6000m²,4.5<=height<=60m,
at most80 boundary coordinates, and residential/mixed/commercial/institutional
family. Clipped/hidden/raised sources, supplied roof fields, existing nonflat,
special and detailed mass/room programmes keep their previous owners. The
<=800m² generic near-room scope remains untouched. The optional city adapter
retains the existing PodiumDetails owner and all its mass assets.

`roof_crown_mesh` triangulates upward fields and applies the existing closed
heightfield operator. Both physical products are prepared and immutable: full
heightfield and flat coarse roof with the same2D material regions. Existing
roof9/metal10 materials are reused. The conservative displacement check covers
the whole source box, including ground level, and retains the0.5px threshold.
Physical/logical products remain available independently of the draw choice.
This is in-memory product reuse, not serialized cache loading.

`roof_crown_present` separates the rectangular facade body at the deck datum
from the blank upper wall band and roof. The default facade path still owns
windows, entries, shops, projections, bands and broad cladding. Joining the
prepared skin afterwards reconstructs the complete physical volume. It avoids
hidden internal roof caps and keeps the existing rectangular near-window host.
Full and coarse physical references remain independently closed.

An actual counterexample demonstrated why that split matters: passing the whole
upper wall directly to the old rectangular-host test suppressed all17 near
window cavities on the sampled large building,31746→5999 triangles. The final
sample restores all17 hosts without rejections,31938 triangles. The initial
actual image/input is retained privately; the stopped render run was not a pass.

Supplied decimal tops5.2/6.2/6.3/7.2m also exposed inverse-rounding loss when
computing `(height-guard)+guard`. Parapets now attain the original top directly;
the exact source-height contract and tolerance are unchanged.

## Real inputs and independent checks

Two prior large cladding fixtures plus two pinned actual service programmes
cover four source forms, including concave courtyards and a low two-hole plan.
`data/roof-crowns.json` retains original attributes, geometry, four-metre neighbor
context and six provider receipts. `data/roof-crown-city-window.json` makes the
complete prior Paris selection reproducible; no private census-file dependency.
No place/source-ID special geometry appears in the product code.

Nine new controls check directed closure, independent Manifold volume/surface,
parallel-offset height sections and integrated volume, unit normals, metric UVs,
holes, complete/disjoint material coverage, stable source choice, supplied fields
and level restrictions, source immutability, exact old nonflat facade output,
near cavities/access, closed physical versus assembled body/skin equivalence,
warm product reuse, narrow/low/invalid fallback and exact decimal top heights.
With14 existing frame/cladding controls:23 source tests pass9.609s before the
identical `b4fd2764a` code freeze;23 installed tests pass8.325s at that commit.
Linux x86_64, Python3.13.5, `.venv`.

The fresh `3110c86ec` wheel contains205 byte-exact installed modules, SHA256
`c938b469f241e98d72bbd74aca366403f2e0a8f6f4ba146eb3cfa8cb8f568abd`.
The library bytes remain unchanged through final replay `ae1f57a16`.
Actual geometry/render/cost producer: frozen `20d33ebb2`. Final code replays all
20 single-model numeric streams, cameras, material bindings and crown plans
exactly. All47 actual city crowns already attained their source top exactly;
the later decimal correction changes none of those input branches. This is
explicit reuse of matching evidence, not a new full-city/runtime render claim.

## Whole window and actual images

Both complete passes retain988/988 sources with zero window omissions. Original
attributes, IDs, footprints, height intervals and flat placements remain exact,
as do camera, six receipts, material definitions and facade bindings. All eight
prepared podium/wing assets and all12 previous mass/function records remain,
with only measured timing fields differing. The same prepared podium assets
are shared between passes.47 crowns:40 parapets and7 service programmes; all
47 use the full product in this city view. Other941 sources retain their owner.
Triangles424225→429229, +5004/+1.1796%. Full two-pass cold model/export completes
with exit0 in868.848s under shared host load; this includes the existing mass
preparation and is not a warm load or native/frame time.

Nineteen new actual1920×1080 rasters were opened:18 single/control views and the
new complete city. Two identical single baselines and the previous complete
city baseline were also opened and reused only after exact numeric render-input,
camera and material comparisons. Together these are20 matched single/control
views and two whole-city views; four actual2D diagrams were opened as well.
Renderer: Blender5.2.2 LTS, Eevee16, Mesa llvmpipe LLVM19.1.7 software backend.
The successful-save wrapper exits0 before known bpy cleanup trouble; this is
not normal teardown or GPU/device evidence. `roof_crown_render.py` exposes that
same explicit termination as a reproducible CLI.

The new city has78118 different pixels,3.7673% of1920×1080; mean absolute RGB
error0.823365/255. This measures change, not quality. Opened images show more
readable perimeter endings and recessed roof enclosures. Large roofs still
remain sparse, the enclosures plain, and facade/body repetition substantial.
There is no claim of CityEngine-level architecture variety from two programmes.

Far ROI comes from the full mesh/camera projection, not the observed difference.
At selected geometric errors0.4155/0.3667px, full/coarse mean RGB error inside
the independently projected ROI is1.0235/1.5174 of255; worst31/18. Forced near
coarse controls are visibly wrong: mean2.0473/10.5991, worst115/135,19003/153171
different ROI pixels. Position bounds are not radiance/shadow/night/temporal
guarantees. Native/device/cache/continuous-interaction acceptance stays open.

## Scoped alternatives and work

`roof_crown_costs` compares direct coarse construction with building the full top,
flattening it and completing the same occupied/material surfaces. Independent
volume/surface/material-region checks pass; both have metric UVs but different
chart orientation, so textured/anisotropic shader equivalence is not claimed.
Four actual source forms use the service programme as a common stress condition.
Each method has21 warm calls plus a separate traced-memory call.

| Source control | Direct coarse p50 ms | Flatten full top p50 ms | Direct/alternative triangles | Direct/alternative traced peak B |
|---|---:|---:|---:|---:|
| 0 | 18.000 | 25.476 | 288/416 | 285568/392056 |
| 1 | 11.223 | 15.898 | 156/220 | 161888/216468 |
| 2 | 9.886 | 12.690 | 124/172 | 130472/172408 |
| 3 | 28.162 | 40.960 | 452/660 | 438636/611340 |

Reusing body/skin products and binding/concatenating their bare presentation
streams takes p50 .041/.035/.035/.039ms versus rebuilding only the required
coarse body/skin27.357/16.764/14.028/44.201ms. Final arrays match exactly; final
streams still allocate/copy their six arrays. Preparation of both closed products
and all body/skin parts takes53.847/32.487/28.425/84.709ms from an already chosen
logical crown. Resident mesh/topology bytes, output bytes/counts and triangle
work are separately recorded in the private JSON. No all-allocation count is
inferred. Peaks cover traced Python/NumPy allocations, not GEOS/native/process
heap. Source/context preparation, serialization, facade detail, export and
rendering are outside these scoped methods. Load10.95→11.27 on4 logical CPUs:
these are shared-host comparisons, not isolated device timings.

## References, reproduction and retained evidence

[SDK/Ghidra and optically checked SIGGRAPH reference](../../../doc/references/buildings/esri/roof-crowns-sdk-3.4.12206.md).
Offsets are primitive references; the architectural programme and bounded service
placement are our own. No Esri source, recipe library or full equivalence claimed.

Use `.venv`, `TMPDIR=/tmp`, `PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache` and
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`:

```sh
.venv/bin/python -W error -m unittest verify_roof_crowns verify_clad_facades verify_facade_frames
.venv/bin/python test/experiments/buildings/roof_crown_probe.py --output /tmp/outshine-buildings-crowns-models
.venv/bin/python test/experiments/buildings/roof_crown_city.py --output /tmp/outshine-buildings-crowns-city
.venv/bin/python test/experiments/buildings/roof_crown_costs.py --output /tmp/outshine-buildings-crowns-costs.json
.venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-crowns-models/0NearAfter-geometry.json --output /tmp/outshine-buildings-crowns-models/renders
```

Current private evidence: `/tmp/outshine-buildings-crown-fixed-model-output/`,
`/tmp/outshine-buildings-crown-fixed-city-output/`,
`/tmp/outshine-buildings-crown-fixed-cost-output/costs.json`,
`/tmp/outshine-buildings-crown-review.json`,
`/tmp/outshine-buildings-crown-final-exact-replay.json`,
`/tmp/outshine-buildings-crown-baseline-reuse.json`,
`/tmp/outshine-buildings-crown-city-baseline-exact.json` and wheel receipt under
`/tmp/outshine-buildings-crown-wheel-3110c86ec/`.
The original full city raster/input remains in
`/tmp/outshine-buildings-clad-city-output/`; two reused single baselines and the
needed first-version near counterexample remain in
`/tmp/outshine-buildings-crown-model-output/`. `roof_crown_review.py` checks actual
render hashes, source/mass retention and RGB-byte comparisons. Private outputs
and duplicate frozen worktrees are cleaned after delivery; needed inputs,
counterexamples, pins and current exact render chains remain.
