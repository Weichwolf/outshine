# Explicit regional positions and actual mixed rendering

Python reference for #717/PR #720. The earlier
[whole countercase and height studies](stair_seed_dependencies_comparison.md)
retain their original scope. The functional106-source/65-building/four-plant
world belongs to [the service delivery](stair_service_comparison.md), not to a
new regional physical-world admission. Shared library/native types are unchanged.

## Ownership before conversion

`position_payload` subtracts an explicit binary64 owner origin before Float32
conversion. A stored neighboring Float32 payload owns its shared boundary.
`encode_regions` either copies that payload with its original origin, or rebases
only points whose reconstructed binary64 position remains exactly equal.
Limits remain65536 points,32 neighbor groups and33 origins. Ambiguous/overlapping
owners, invalid IDs, unrepresentable required single-origin seams and incomplete
streams reject. A general spatial partition, geodetic/world frame protocol and
native camera/previous-camera implementation are still #717/#568/#48.

The common `regional_render_frame` serves both the Workbench and actual Eevee
consumers. `origin - render_origin` is computed in Double before Blender stores
the object translation as Float32. The camera eye/target use the same frame.
Local rotation/scale and supplied vertex payloads stay unchanged. Actual imported
positions, matrices, corner normals and linear face albedo have separate receipts.
Legacy callers retain their previous camera-verification convention and images.

An origin must belong to its source product; it is not a free bounds-centering
optimization. A new countercase on the complete base body has six nonpositive
faces after the existing absolute Float32 draw conversion, but29 after a naive
bounding-box-center origin. A36-origin comparison still has at least one failure.
Those are draw-encoding observations, not source-Double or accepted physical
geometry failures. The old whole-plant display already explicitly distinguished
its Float32/subpixel conversion from physical source geometry. The new reference
does not grant physical admission to either draw mesh or relax its gates.

The checked mixed scene explicitly retains the old base's exact Float32 draw
coordinates in its existing frame. Each new plant receives a caller-supplied
local frame and must preserve every original face orientation. There is no
automatic fallback from a failed new frame to an admitted product.
`regional_scene_probe --origins` requires every original owner, including an
explicit `preserve_legacy_draw` boolean. Legacy mode checks exact old positions;
new mode rejects any nonpositive source-relative face. Source geometry is not
changed. Automatic safe origin choice for broader physical products remains open.

## Complete inputs, consumers and images

Frozen mixed producer/independent reader: `942a949614df645c279abfb7e62b668343819105`,
including the complete original `866a98d54` handoff and master`e0db346e4`.
Original scene and every mesh/frame input are hash-bound. The actual106/65/4
composition has five source owners,1210729 vertices and2297139 faces.
Each full plant exceeds65536 vertices. Bounded indexed draw parts retain original
vertex/face IDs; all source normals, material ownership and face colours remain.
The resulting37 objects contain all original faces, in order, and336 duplicated
boundary vertices. No source or face is omitted to meet the existing64-object
or65536-position bounds.

The separate reader checks complete original owner inventory, source mesh hashes,
all indexed faces, corner normals/albedo, nearest Float32 local rounding, shared
vertex bits across draw parts, exact preserved legacy positions and every new
face orientation. It ends terminal0 on all2297139 faces/37 objects. The original
1002-point/1896-face planar countercase separately passes all six full neighbor
stream exports/readbacks; the source fixture and scientific tolerances are unchanged.

Source controls:17 methods/6.203s, terminal0; isolated Installed library:
17/5.733s, terminal0. These include the actual Eevee normal-pixel control, original
regional/boundary negatives, the whole1002-point Blender import, unchanged legacy
input, exact global translation and two camera frames. The17-method renderer
closure is unchanged between its checked producer and942a94961. All218 installed
library Python files match the current source; no source-library fallback is used.

Fresh1280×720/16-sample actual Eevee images were rendered and opened. Complete
legacy Eevee against the pre-change renderer has zero differing pixels. The
legacy base-only Workbench pair also has zero differing pixels. Both explicit mixed
camera states are pixel-identical to their global translation by1073741824m in
each axis. All37 actual model matrices/payloads, normals/albedo and camera
matrices agree after the declared world-origin shift. A subsequent camera move
of(8,-4,1)m changes model translations by(-8,4,-1)m with unchanged local rotation,
scale and vertex buffers;151671 pixels change. These are Blender render/binding
proofs, not native GPU motion-vector or device evidence. The control camera is
rounded to1/8m to isolate global translation from binary64 decimal-input rounding;
the legacy comparison retains the original camera exactly.

The rejected bounds-centered trial also has pixel-identical translated whole
images despite its additional bad faces. Thus whole-image equality alone cannot
admit a geometry representation. The whole scene remains sparse, pale and
visually coarse; the four complete physical plants are mostly subpixel here.
There is no new morphology, density, general LOD or artistic-quality admission.

## Scoped costs and reproduction

Shared Linux i5-1135G7, Python3.13.5/NumPy2.5.4, Blender5.2.2LTS/llvmpipe,
BLAS/OMP1, LP2. Tests/rendering use nice10; encoding comparison uses normal
priority, five unprofiled repeats and separate Memray1.20 allocator replays.
Native two-job analysis and other work share the host. No isolated speed ranking.

For the original1002-point/106-boundary input, retained outputs are8016B positions
plus16/32B origins and optional1002B owner IDs. Copy retains9050B in all three
neighbor cases; exact rebasing retains8032B for the legacy neighbor and9050B for
west/east. Median copy/rebase times in microseconds: legacy164/215, west200/259,
east192/253. Rebase trades extra work for a smaller legacy stream; it saves no
retained bytes for the two non-exact neighbors. Both methods reconstruct the
same full geometry. No global optimality or runtime profile is inferred.

| Neighbor/method | Allocation requests | Cumulative requested B | Tracked peak B |
|---|---:|---:|---:|
| Legacy copy |609|93324|42096|
| Legacy rebase |602|105913|41904|
| West/east copy |591|92172|41904|
| West/east rebase |647|115805|41904|

The encoding timer excludes source loading and peer encoding; decoding/hashing
are outside timers/trackers. Memray includes Python allocator events, excludes
native stacks. Requested bytes are not copied or simultaneously retained bytes.
For106 boundary points, rebasing additionally constructs1696B Double world and
restored arrays each, and848B Float32 rebase storage, besides masks/selection work.
Kernel/internal copy totals remain unmeasured.

The full mixed export plus quality/compression diagnostics takes10.620s; this is
not an isolated encoding time. Draw positions occupy14532780B plus888B origins,
including4032B duplicated boundary positions. Provenance IDs, other source arrays,
Blender copies and driver memory are separate. Original/current legacy and mixed
render observations are55.207/60.551s and3092552/2994944KiB process peak RSS;
different object layout and shared load prevent a memory/speed ranking. These
offline captures grant no world-loading/frame/device budget.

Use `.venv` and `PYTHONPATH=lib/src:lib/tests:test/experiments/transport`.
`regional_scene_probe.py --scene --origins --output` takes explicit complete
inputs; `regional_scene_verify.py --products --output` reads them independently.
Use `render_transport_research.py` for the generated scene JSON, then compare
with `test/scripts/pixels.py`; both current/next global-translation pairs must
remain exact. `stair_regional_snapshot.py` and its separate verifier reproduce
the versioned1002-point neighbor study. `stair_regional_positions_cost.py`
supports separate normal repetitions and allocation replays.

Private inputs/outputs, negative trial, actual captures, raw allocations, commands
and receipts: `/tmp/outshine-research-regional-positions-e0db346e4/`.
Original106-source inputs remain under
`/tmp/outshine-research-service-roads-c91a94d56/`.
No images are published. SIGGRAPH-first pipeline source: Chen2008
[p2/Figure2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
optically reopened; it motivates graph/geometry separation, not a precision proof.
The subtraction/rounding claims above are measured own arithmetic and actual
consumer controls. No foreign implementation is bundled.
