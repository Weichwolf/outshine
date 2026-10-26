# Vienna ground traffic around the retained bridge/portal field

Owner: [#303](https://github.com/Weichwolf/outshine/issues/303), under #21.
Python experiment; source originals, declared omissions and the old accepted
products remain separate. No native runtime or complete worldwide-generator claim.

## Geometry and height planning

The former common-ground view from PR #302 still used only 71 of 3,259 source
records. The same archived three OpenFreeMap tiles contain much more ground
traffic. Existing component recipes plan that traffic, retaining source IDs and
attributes. Known old floor/level decisions are preserved. Actual portal air is
reserved with curb/endpoint-repair construction reach, followed by a whole
footprint test. Nonzero or ambiguous `level` tags without a selected surface
are now rejected by the shared flat planner as `deferred_floor_level`, rather
than flattened to the ground. Layer and indoor checks remain independent.

Two constructions compare clipping old/new cells with retriangulating their
identical physical traffic union. Both retain the old footprint. The selected
procedure instead reuses the union **as a domain**, avoiding an intermediate
traffic triangulation for its height plan.

A direct fit with a variable at every dense boundary vertex is retained only as
a bounded negative cost comparison. The selected height field reuses the old
coarse terrain support from PR #302, extended to a planning rectangle. The
rectangle is not rendered and acquires no additional source tile. Coarse faces
touching traffic receive conservative road-gradient constraints; roofs, old lower
surfaces and clipped upper/traffic intersections provide whole affine bounds.
The eight-second solve budget and actual final tolerances remain unchanged.

Road construction uses grade 0.0249 as a numerical construction margin; actual
emitted traffic is independently checked at the original 0.025 limit, soil at
2/3. Upper C1/portal geometry remains unchanged. Actual roads require 4.5 m under
the old deck underside, all soil 0.02 m below that underside, and roofs 2 m below
the 0.25 m soil shell underside. These are explicit procedural recipe values.

Rejected comparison constructions are recorded as absent and cannot prevent
the independent selected field from running. The direct dense construction may
fail its own degenerate-face guard before reaching its solver. No such failed
surface is exported or rendered.

Lifting unconstrained fine triangles across height-field creases is insufficient.
Cutting neighbours independently also failed the real coverage check. The final
method globally nodes the field edges, traffic boundary and actual world boundary,
polygonizes that arrangement, and triangulates its cells. Every emitted vertex
keeps its owned XY and queries only its height. Whole-face normals, complete
road/world domains, old footprint preservation, interior-crack absence, source
axes and actual clearances are replayed. Closed bodies and supplied flat corner
normals are exported for the renderer. Solver status alone proves nothing.

The real indexed bodies exposed Blender 5.2.2 custom-normal packing errors even
without degenerate float32 faces. Sharp edges and reapplying packed normals did
not preserve all supplied vectors. Transport therefore supplies its exact
FLOAT_VECTOR/CORNER normal attribute directly to the material's object-to-world
normal transform and Principled input. A real Eevee pixel control verifies the
attribute/shader path; import checks preserve float32 vectors exactly. Packed
mesh-normal deviations are measured and unused, never silently accepted as the
intended source normals. Shared camera/mesh/daylight APIs remain unchanged.

## References and provenance

First reference: [Chen et al., SIGGRAPH 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page 2/Figure 2, separates network and 3D geometry; [Galin et al., Eurographics 2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
section 6.2/page 9/Figure 20, motivates coordinated road/terrain construction.
Those pages were optically inspected in the preceding terrain work. The reduced
height basis is our bounded procedural approximation, not an implementation or
reproduction of those papers or CityEngine.

The arrangement uses the existing GEOS/Shapely primitives; see the primary
[GEOS overlay documentation](https://libgeos.org/doxygen/classgeos_1_1operation_1_1overlayng_1_1OverlayNG.html)
and [Shapely polygonization API](https://shapely.readthedocs.io/en/stable/reference/shapely.polygonize.html).
No coordinate-precision reduction, foreign source code or SDK runtime is added.

Archived complete capture SHA256:
`aaea9506c3459045f5f6c2eb2df4f96b222e47e1acd88f377ec66e9f70126475`.
Old height-support input:
`e740d969b6b6baa230c50cb1c3475350d8610b1685119c2bb619732acde89442`.
All receipt bytes and old bridge/portal hashes are checked. New source ledgers
cover the original 3,259 records once; every omitted source has a concrete reason.
The separate new lower plan needs consumer binding before library/native adoption.

## Verification and presentation

Reproduce with the explicit external paths in [README](README.md). Generation
requires fresh outputs and a clean frozen source. The geometry process has 3 GiB
address-space and 180 s CPU limits; its source worker separately has 2 GiB/60 s
CPU and a supervising wall timeout. Solver limits remain eight seconds. These are
offline experiments; no frame/device budget or full traffic-routing claim.

New 720p before/after pictures use identical regional camera and presentation.
Blender/bpy 5.2.2/Eevee consumes shared camera verification, original triangle and
supplied-normal import, and agreed AgX/daylight from #291. Material colours remain
procedural inspection choices. No ground is flattened or added for display.
One local 1920×1080 after picture uses the same camera for detail checking.
All images stay local; only master publishes integrated Places.

## Frozen result and image decision

Clean geometry generation: `cf33992d3706062b93d708a03fdb19547bdd570f`.
Clean presentation: `78b9202ffbbf9c51527bf10451e96cdfe0e06be3`.
Cost probes: `27d1e83cae896aef4305309a019152b0b65f9b58`.
86 affected geometry/library controls and 13 actual Blender controls pass,
including the real source-normal shader pixel, without skips. Ruff, formatting,
functions ≤120 lines, diff check and documentation lint pass. No native gate.

The complete ledger has **2,562 used, 19 duplicate-represented, 678 not used**.
All 2,510 new used/represented original axes lie inside the actual traffic
footprint. The old 71-source field remains included. Omissions are 367 levels,
117 polygons, 88 additional indoor recipes, 50 portal-air reservations, 31 old
Buildings floor decisions, **13 unsupported floor levels**, 8 route/missing
recipes and 4 other previously named unresolved sources. Nothing is silently
flattened or discarded.

The final common ground has **104,787 vertices /196,920 faces**, including
95,456 traffic faces. Complete traffic area is 759,339.3114 m². Independent
world/traffic/old-footprint area errors are zero. Maximum normalized emitted
grade is 0.999999999993. Whole road/bridge underside clearance is **4.941366 m**,
all-soil underside gap **0.020000011 m**, and roof cover to the soil shell
underside **2.999792 m**. Old upper contacts and tunnel air 4.5 m remain valid.
DEM area-weighted vertex RMS is **2.167086 m**, a sampled approximation metric,
not a uniform terrain-error bound or an optimum. New lower contacts differ
from the previous plan and still need consumer binding.

Both physical-union alternatives preserve the identical footprint: cell
clipping 91,585 faces /15.557 s, domain retriangulation 88,993 faces /18.162 s.
The selected procedure needs neither intermediate triangulation. It instead
solves **1,026 heights /2,046 face cones**, 13,861 affine rows and 59,997 sparse
nonzeros in **0.390 s total fit** (0.373 s solver). Its traffic mask is
conservative; actual emitted faces retain the original physical acceptance.
Full comparison/generation took 129.858 s and peaked at 542.934 MiB, including
rejected comparisons and geometry/normal exports. This is not the selected
procedure's isolated runtime cost or an A18/RTX budget admission.

An additional dense refinement probes **the exact accepted final mesh**, with
104,787 height variables /196,920 cones /1,836,297 nonzeros. It has a known valid
shared-field fallback. Under the same eight-second setting the instrumented
dense candidate reaches MaxTime and violates whole-face grade (9.358 normalized)
and an affine bound (0.782 m), so it is rejected. The solver setting is
cooperative and overran inside a step to 8.850 s; measured setup is separate.
No claim of mathematical infeasibility follows from this failure.

| Isolated cold fit, Memray 1.20.0 | Shared field | Dense refinement |
|---|---:|---:|
| Recorded allocation events |235,191|8,947,510|
| Cumulative allocated bytes |36,140,543|1,384,089,889|
| CSC constraint storage, bytes |724,072|22,454,716|
| Worker peak RSS, MiB |107.285|721.379|
| Instrumented total wall, s |0.533|10.894|
| Independently accepted candidate |yes|no|

These counts include Python/native allocations during the cold fit; they do
not cover the entire generation/renderer. Tracking changes timing. The two
bases sample their least-squares priors differently, so no objective-equivalence
or optimality claim is made. Both retain the same physical acceptance limits.
The exported after body contains 28,697,232 array bytes. Explicit source-XYZ
indexing copies 30,387,744 bytes and corner-normal repetition copies 15,193,872
bytes; these are measured known copies, not a complete pipeline copy count.

All three fresh pictures were actually opened: matched **1280×720** before/after
and the same after camera at **1920×1080**. The regional window includes about
570 added source axes. The network is visibly more complete while retaining the
bridge field. The materials remain schematic and low-contrast, and the ragged
finite source-border margin remains visible. Piers, rail hardware, general
portal retaining walls, C1 for the new ground network, train routing, complete
tile ownership/LOD and native integration remain open. No artificial world
objects, closeups, insets, display cuts or height exaggeration were added.

Blender 5.2.2/Eevee, 16 samples, Mesa llvmpipe/LLVM19.1.7: before 11.700 s /
1,762.42 MiB process peak; after 720p 21.194 s /1,978.54 MiB; after 1080p
27.790 s /2,150.83 MiB. These are offline software-presentation observations,
not GPU/device/frame times. Actual camera matrices, unchanged topology,
float32 positions, exact supplied normal attributes and shared-helper hashes
are saved. Material and light are identical across the matched pair.

Source environment: Python3.13.5, NumPy2.2.4, SciPy1.18.1, Clarabel0.11.1,
Shapely2.2.0/GEOS3.14.1; the existing component CDT adapter uses system
GEOS3.13.1-CAPI1.19.2. No SDK is loaded.

Actual common ground SHA256:
`16cd7e8b774136dc50e70e9549d376630b4f7bb88e221f4b46527c30367627e5`.
Before PNG: `bb6f8ff27fea5e5ea82cc01e6f50c70f9f873ba1932951e6b729afd62af5e33c`;
after720: `aa881441e310447e8a693bf9cfa88dfbdc75a60ed1231e24de4ec01f9cfa0d0d`;
after1080: `ae8e3fc11d173c8d62e42172ca84fd7ad4c4e06e84ecab718e784aff31826486`.
The source, actual meshes, complete ledgers and render/import/camera proofs stay
in `/tmp/outshine-transport-network-303-cf33992d3/`; allocation traces/stats and
the dense-refinement input stay in `/tmp/outshine-transport-network-cost-303/`.
Terminal logs use `/tmp/outshine-transport-network-{cf33992d3,render-78b9202ff}*`.
All images remain local. The actual shared-helper consumption is reported in #291.
