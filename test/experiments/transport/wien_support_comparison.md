# Whole-contact bridge supports on the Vienna Python network

Owner: [#326](https://github.com/Weichwolf/outshine/issues/326), parent #21.
The #303 regional image still showed unsupported bridge slabs. This experiment
adds physical procedural piers while preserving the actual bridge, tunnel,
ground, source usage and base material arrays. It does not prove load capacity,
foundations, safe spans, abutments or a complete worldwide bridge generator.

## Procedure and references

First reference: Chen et al., [SIGGRAPH 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page 2/Figure 2, separates network planning and actual 3D geometry. Galin et al.,
[Eurographics 2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
page 8/Figures 17–18, uses generic parameterized bridge/road/tunnel models and
terrain-relative segmentation. That page was optically inspected again for this
work; the SIGGRAPH diagram was inspected in the preceding work. Neither paper
supplies our pier dimensions, structural calculations or reconstruction evidence.

Original retained deck axes own station identities and orientation. Explicit
recipe: 1.6×2 m rectangular footprints, at least 2 m height, nominal stations
every 24 m starting at 12 m. A fixed choice tests only that station. The bounded
shifted choice tries 0,−3,+3,−6,+6 m in that order, retaining the first admissible
position. Piers must avoid the full lower traffic footprint and portal/trench/
tunnel-roof projections with 0.5 m clearance, and every already accepted pier.
This is a conservative geometric exclusion, not a complete transport safety rule.

Both finite contact surfaces must cover the whole footprint. Their original
triangle edges and the footprint boundary enter one globally noded arrangement,
then polygonization and constrained triangulation. The shared XY mesh follows
every actual upper/lower crease. Explicit selected-surface samples provide only
Z; shared XY positions are retained. Upper and reversed lower caps plus directed
outer/hole walls close the body without interior walls. Positive height over
every affine cell and independent oriented-edge/volume checks are mandatory.

Independently clipping neighbouring source faces first failed on almost
coincident Vienna boundary intersections. The saved minimal real counterexample
[`lib/tests/fixtures/bridge-support-contact-vienna.json`](../../../lib/tests/fixtures/bridge-support-contact-vienna.json)
(moved with #332) fixes that regression. Original
edges are now noded together once, without precision reduction or a new weld.

## Fair real-input comparison

Both methods use the same 307 nominal stations on 19 retained bridge-source
axes, totalling 7,414.019 m including parallel tracks. No new source acquisition,
source-specific special geometry, deck changes or flattened terrain.

| Same inputs and physical acceptance | Fixed | Locally shifted |
|---|---:|---:|
| Accepted physical piers |262|288|
| Candidate trials |307|430|
| Accepted-pier pair checks |36,211|45,012|
| Source-face candidates in accepted prisms |836|923|
| Noded segments in accepted prisms |6,374|7,104|
| Upper contact faces |1,198|1,336|
| Emitted body vertices /faces |3,394 /5,740|3,758 /6,364|
| Normal measured planning wall, s |1.181|1.343|

Source-face/arrangement totals above count accepted prisms only; trial and pair
counts include attempted placements. Fixed omissions: 36 lower-traffic/portal,
2 outside finite contact and 7 existing-pier conflicts. Shifted omissions: all
19 lower-traffic/portal exclusions. Every station has its source ID and actual
trial decisions; all 19 original axes receive some piers. No source-usage record
is reclassified. The maximum within-source accepted-station gap remains 144 m,
so neither method establishes a complete structural support plan or safe spans.
The shifted method is selected for greater admitted coverage, not optimality.

Successful clean generation/presentation: `95fd421c04a0ec8be341581832a63d1184767754`.
Full generation including both methods, replay and closed-body exports:
14.525 s, 442.359 MiB process peak. Bounds: 3 GiB AS/90 CPU s; each plan has a
cooperative 20 s setting, at most 1,024 stations/5,120 trials/1,048,576 prior-pier
pair checks; each contact patch has at most 2,048 source faces, the arrangement
8,192 segments and 4,096 contact faces. No frame/device-budget claim.

Separate Memray1.20.0 cold planning calls on `b339a0a44` retain exactly the same
saved geometry. Imports, source decoding and both contact-index builds precede
tracking; packing and independent verification follow it. Tracking includes
Python/native allocations and changes timing; counts are not live memory.

| Isolated instrumented planning | Fixed | Locally shifted |
|---|---:|---:|
| Allocation events |3,339,934|3,706,092|
| Cumulative requested bytes |398,754,405|439,646,907|
| Tracked peak heap bytes |14,535,099|14,535,150|
| Instrumented total wall, s |6.575|6.070|
| Whole worker peak RSS, MiB |283.137|288.953|
| Owned emitted mesh array bytes |150,336|166,560|
| Additional owned cap array bytes |110,208|122,256|

The tracked heap is not full-process RSS; RSS includes loaded original scenes
and later exports. Array payloads quantify known ownership/copies, not every
internal copy. Planning currently allocates millions of small objects; it is an
offline Python reference, not admitted runtime code. The after body's complete
export payload grows from 28,697,232 to 29,143,808 bytes.

## Independent geometry and actual picture

Saved piers/caps are decoded and rebound to their original source stations.
Actual body caps, boundary-wall count, oriented closed volume, positive height,
complete recipe footprint, mutual separation, traffic/portal/world exclusions,
and both directions of whole-face contact separation are checked. Maximum
contact error: 1.706e−13 m in Float64; heights 2.246–9.576 m. Original base
positions, topology and material slots remain byte-identical. All 6,364 selected
pier faces survive the actual float32 presentation cast without degeneration or
inversion; maximum vertex displacement from that cast is 0.0002441 m. Float64
contact precision is not an assertion of exact rendered contacts.

All three fresh Blender5.2.2/Eevee pictures were opened: matched 1280×720
before/after and the same after camera in 1920×1080. Same shared #291 camera,
mesh/explicit-normal and daylight helpers, same AgX/material palette. Relative
to #303, orthographic span is moderately zoomed from 1,400 to 1,100 m; eye/target
stay fixed. Within the pair only actual piers change. No cuts, insets, artificial
ground, height exaggeration or place-specific decorative geometry.

Piers and the elevated bridge now read in the regional view. The finite border,
schematic low-contrast materials, incomplete pier coverage, missing foundations/
abutments, lower consumer binding and ground C1 remain open. Render wall/RSS:
before 25.666 s /2,047.102 MiB; after720 24.857 s /2,031.906 MiB; after1080
42.805 s /2,151.941 MiB. Actual backend is Mesa llvmpipe/LLVM19.1.7, 16 samples,
two threads. These are offline software presentation costs, not GPU frame time.

Source environment and receipt pins are inherited unchanged from #303. Required
packages: NumPy, SciPy, Shapely/GEOS, Clarabel, Pillow, optional Blender/bpy5.2.2;
Memray1.20.0 only for allocation probes. No new runtime or SDK dependency.
Commands are in [README](README.md). Current private originals/results:
`/tmp/outshine-transport-supports-326-95fd421c0/`; source inputs stay in the
preserved #303 result and original bridge archive. Allocation traces/reports:
`/tmp/outshine-transport-support-cost-326-{fixed,shifted}/`. All images stay local.
