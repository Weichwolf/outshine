# Early facade replacement

Follow-up to the [checked replacement reference](facade_programmes.md), #774.
The same ribbon-screen and vertical-slot programmes now omit old owned trim
before coarse or physical construction. Original openings, floors, core,
portals, terraces, roofs and source heights remain. The component results below
are complete; whole988-source application is still being checked in PR #799.
This report grants no native, device, cache or general LOD admission.

## Construction and preserved ownership

`programme_products` selects the absence of old trim before creating its source
owner. Existing callers default to legacy behavior; empty programme plans keep
the complete legacy body. Foreign source IDs reject before geometry. The
source-owned facade component is the only trim scope: material9 in unrelated
construction is retained. New closed layers also replace decorative collision.

Cascade bands and compact accent quads are skipped outright. Paired-wing panes
and mullions retain their original part IDs by counting skipped pier polygons,
including portal-clipped parts, without extruding them. These identity counts
remain necessary work. Direct front/outer/closed products share the same wall
plan. Selected component IDs are validated before layer expansion; physical
function remains independent of the draw selection.

The compact replacement reference drops unreferenced old trim vertices before
joining. All normalized draw arrays, normals, UVs and physical frame part IDs
match the early path exactly. Original structural geometry is not snapped or
replanned. The handed-off micrometre model for new decorative profiles and all
existing physical tolerances remain unchanged.

SIGGRAPH-first sources, optically reopened during this continuation:
[Instant Architecture2003, p3/Figure4](../../../doc/references/buildings/siggraph/2003-instant-architecture.pdf)
for attribute/shape hierarchy, and
[Procedural Modeling of Buildings2006, p5/Figures9–11](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf)
for alignment and bounded occlusion reasoning. The
[pinned SDK/Ghidra evidence](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md)
supports existing split/component/extrude primitives. No foreign implementation
or complete Esri architecture recipe is copied or claimed.

## Correctness and actual images

Frozen producer: `9926986a60d0bd8ac394ee1334affb6133e65643`, including the
protected `b9f64f809` handoff. The main branch subsequently incorporates master's
`725c124c2`; Python modelling files are unchanged by that native water advance.
The detached producer and its running gates retain their original commit.

Source `verify_facade_programmes verify_bay_facades verify_facade_omission`:
14methods/1223.339s, terminal0. Isolated Installed library `verify_facade_omission`:
3methods/352.903s, terminal0. This covers14 original/roof-coupled opening plans,
four complete early physical/programme cases, exact retained structural/frame
arrays, all near/far body/roof combinations, empty/three selections, protected
apertures and continuous routes, positive closed volumes, foreign sources,
capacity failures and empty-programme fallback. The original hierarchy and
shifted-opening/portal/wall negatives remain. Default legacy cases also pass.

Fresh original body/roof/draw census:71/71, terminal0,1286.514s after source
setup,425552KiB worker RSS peak. This is the baseline for subsequent full city
checks, not an approval of the changed whole-window facade products.

All six newly generated hero models match every handed-off draw array exactly.
Eighteen original captures are verified against actual image hashes, exact
current mesh bytes, equal camera/material/ground inputs and unchanged rendering
dependencies; original render provenance is retained. Four new1920×1080
Eevee/16-sample captures were rendered and opened: ribbonWhole, verticalNear,
legacyWhole and verticalOpposite. The first two are pixel-identical to their
replacement-reference captures. Before/after whole and opposite views show the
coherent grouping; pale glazing, fixed window grids and plain roofs still
dominate repetition. Blender5.2.2LTS/llvmpipe software rendering is not a GPU or
native Places budget proof. Images remain private.

## Same-result work and costs

Linux i5-1135G7, Python3.13.5, NumPy2.5.4, Shapely2.2.0/GEOS3.14.1,
Manifold3.5.4. One BLAS/OMP thread. Normal cost processes use normal priority;
tests, census, images and profiling use nice10. Native three-job lint and our
independent checks share the host. Three fresh-owner calls per normal case
precede separate allocation and work-counter replays. Original source/roof/
programme planning and imports are excluded. A fresh coarse owner and requested
complete physical/collision/draw products are included.

All eight source/programme/near-far reference/early pairs have identical full
output hashes; their separate allocation/counter replays match as well.

| Source control | Avoided closed trim triangles | Avoided compact trim quads | Near retained NumPy reduction | Compact reduction |
|---|---:|---:|---:|---:|
|0, cascade|996|83|132468B|46812B|
|3, paired wings|5568|464|740544B|261696B|

Early construction emits zero of those old trim triangles/quads. Avoided closed
source-array payload is85656/478848B, obtained from actual emitted vertices,
faces and their unchanged typed buffers; compact payload plus owners accounts
for46812/261696B. These named constructed/output buffers are not a census of
all internal copies. Existing `np.r_` writes those source buffers; GEOS/kernel
copy totals remain unmeasured. Retained NumPy roots are distinct from heap/RSS.
For example, source0's removed closed payload is
`2324*3*8 + 996*(3*8+2+4) = 85656B`: Float64 vertices, Int64 faces,
UInt16 materials and Int32 parts. Its83 compact quads remove
`83*(6*88 + 2*(12+2+4)) = 46812B`, including triangle owners.

| Near source/programme | CPU median reference/early s | Wall median reference/early s |
|---|---:|---:|
|0 ribbon|2.0230 /1.9990|2.1134 /2.1284|
|0 vertical|1.8714 /1.7049|1.9462 /1.7234|
|3 ribbon|3.3139 /3.2810|3.5069 /3.3914|
|3 vertical|3.2398 /2.9801|3.3548 /3.8366|

CPU medians are lower in these observations; wall times are not consistently
lower. There is no isolated ranking, global speed factor or runtime budget claim.

Separate Memray1.20.0: Python allocator events enabled, native stacks disabled.
Requests include zero-byte events. Complete output identities match normal runs.

| Near trace | Requests reference/early | Cumulative requested B | Tracked heap peak B |
|---|---:|---:|---:|
|0 ribbon|5545606 /5387995|653287765 /640588750|44441303 /44298893|
|3 vertical|8812112 /8094318|1075165663 /1019266887|77213514 /76475197|

Requested bytes are not copied or simultaneously live bytes. Tracked heap,
referenced NumPy storage and whole-process RSS are not additive. Profiling
times are excluded from normal timing comparisons.

## Reproduction and preserved evidence

Use `.venv`, BLAS/OMP1 and
`PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets`.
Required existing dependencies include the declared building/roof packages,
Manifold3.5.4 and Blender/Memray for their respective diagnostics. Replace
`lib/src` with the isolated unchanged reference-wheel target for Installed checks.

`facade_omission_cost.py` takes `--source 0|3`, `--programme ribbon-screens|vertical-slots`,
`--mode compact|near`, `--method reference|early`, fresh `--output` and optional
`--trace`. `facade_programme_probe.py` emits actual complete hero bodies.
`terraced_crown_census.py --physical-roofs --full-bodies` checks the complete
original census. `facade_programme_city.py` takes that fresh `--audit`, the
original `--before`, one `--programme` and a fresh output directory. It checks
every new physical body/layer/roof and preserves all988 sources/protected owners.
Use `roof_crown_render.py` for actual exported near/whole/opposite products.

Original handoff roots remain protected:
`/tmp/outshine-buildings-city-rebuild-data-76f679351/`,
`/tmp/outshine-buildings-programme-models-d5e4b4182/`, and the actual legacy captures
in `/tmp/outshine-buildings-facade-city-controls-914073b2d/`.
New data, exact commands, logs, raw allocation traces, geometry/capture equality,
camera/import proofs and current images remain under
`/tmp/outshine-research-facade-programmes-84b784615/`.
Frozen source is `/tmp/outshine-facade-programmes-9926986a6`.
Original receipts/pins are not rewritten to admit new geometry.
