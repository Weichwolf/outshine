# Whole elevated rail slab candidates

Owner: [#1768](https://github.com/Weichwolf/outshine/issues/1768), parent #434.
The verified114-source transport product still omits the supplied elevated
subway parts4935280960:0 and81129490:4/:5. Existing local-module replay marks
all its new surfaces as Ground. This experiment builds a **separate raised
slab candidate** against the actual held world; it never uses that Ground path.

## Whole sources and geometric contract

Use the existing source decoder, fixed original vector/frame/DEM bindings and
all three complete polylines. Original IDs, properties and Bridge/Layer hints
remain. Width3.8m comes from the existing subway profile. Thickness.6m,
longitudinal/cross grades.04/.03 and construction reserve.02m are explicit
procedural assumptions, not survey, statutory or structural specifications.
The group shares a physical slab; incidental footprint overlap creates no track
connection. A supplied endpoint coincidence is generalized-vector evidence,
not an asserted original OSM node, routing authority or outer continuation.

Existing GEOS corridors, constrained triangulation, conformance and closed-body
builders produce the whole surface/body. The existing2^-11m outward model keeps
source-plus-junction-pad coverage with a2.9296875mm bound and Float32-exact XY
within the existing local range. No original source is clipped to an overlap.

A full profile test exposes a real error in simply unioning flat-capped bands:
at the supplied0.356° turn,1.56560m of the3.8m bisector cross-section is outside
the union, despite its closed body and complete individual source coverage.
Take the convex hull of both original end profiles and the bisector profile as
a small endpoint pad, through the existing `junction_pads_xy_m` contract.
The actual complete cross-section is then covered in Double and Float32.
Simple two-source endpoint turns up to30° are supported; sharper/interior/
branch cases explicitly decline. The existing eight-pad-per-source cap remains.

## Whole lower world and alternatives

The actual held114-source chain has29 road assets and124 supports. Metadata
queries select two whole assets/33 source owners/9560 body facets within the
existing16-asset/64-source/32768-facet limits. Terrain query selects567 faces;
whole footprint coverage is checked in both precisions. Old support envelopes
are checked conservatively; none overlap this candidate. All200 original road
arrays retain their actual byte hashes; terrain and old supports are untouched.

Clip every relevant affine lower facet against the whole new footprint, retaining
all positive-area intersection vertices, including interior peaks and holes.
For an affine candidate, upper-minus-lower height is affine on each clipped
polygon, so its extrema occur at its vertices. These vertices generate8764
height constraints. Float32-rounded lower surfaces and neighbors that could
enter through XY rounding are included before selection/fit, with existing
rounding bounds. New underside must clear held terrain by2.5m and every old
traffic facet by its source-owned clearance; construction reserve is additional.

Compare a constant plane from the direct maximum with a three-variable affine
plane minimizing area-mean height under the same lower/profile constraints.
HiGHS dual simplex is bounded to2s/100 iterations. The latter solves only that
fixed family; no global road optimum or numerical error certificate is claimed.
Actual closed-body topology, source widths, whole junction cross-sections,
face gradients and vertical-clearance kernels independently inspect both exports.
A failed alternative retains its witness and cannot discard a valid flat result.

**Select the constant candidate:** height168.8590294406m in this source datum,
24 top vertices/22 faces,48 body vertices/92 faces,4278.05865m². Minimum terrain
clearance is2.52000043m Double/2.52000376m Float32; actual profile gradient is0.
The tilted candidate lowers area-mean height by.10812546m but violates the actual
Float32 profile on junction face8: normalized maximum3.13139604. It is rejected,
not repaired by relaxing the profile. Its coefficients and rejected mesh remain
as a reproducible counterexample. The constant candidate requires no solver or
per-constraint design matrix. No equal-quality speedup claim uses the rejected path.

## Costs and evidence

Frozen42704b89b, Linux/i5-1135G7, Python3.13.5/NumPy2.5.4/Shapely2.2.0/
SciPy1.18.1, single BLAS/OMP thread. Nine alternating normal-priority rounds after
warmup, concurrent work/load7.44/7.43/12.75; separate Memray1.20 traces include
Python allocators. Decoded held inputs are outside timing/tracking.

|Phase |CPU median ms |Wall median ms |Allocations |Requested B |Tracked peak B |
|---|---:|---:|---:|---:|---:|
|Shared lower-world preparation |240.255 |266.177 |733440 |103714390 |932384 |
|Flat fit/body |1.369 |1.374 |1701 |615934 |283864 |
|Rejected tilted fit/body |59.072 |59.530 |197301 |40173496 |16154308 |
|Independent flat admission |443.401 |486.442 |993634 |110889728 |543014 |

Held old road arrays987493B, constraints210336B. Explicit copied-payload subset:
186192B Double→Float32→Double vertices,215208B old support casts/stack,
210336B constraint stack,576B per-candidate surface copy. Other internal
NumPy/GEOS/HiGHS/body/audit/IO copying is unmeasured. Requested bytes, retained
arrays and RSS are different quantities and are not added together.
A separate normal process uses1.563CPU/1.577wall s decode and.879CPU/.880wall s
analysis,152133632B OS peak; tracing/report-reading peak158593024B is separate.
These are offline Python/reference costs, not runtime/native/device budgets.

30 targeted checks pass, including an interior terrain peak, clipped affine
extrema, capacity before expansion, actual Float32 lower heights, source width,
closed-body/profile rejection, the real small-turn gap and whole-pad repair,
retained valid candidates and the fixed real Float32 tilted counterexample.
SIGGRAPH first: [Chen2008, PDFp2/Fig2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
optically inspected graph/geometry separation; [Galin2010, PDFp8/Figs17–18](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
optically inspected profile and terrain-relative bridge generation. Our affine
clearance constraints/pad are explicit local procedures, not their whole method.

Four actual before/after detail and whole1280×720 images were rendered and opened.
The complete preceding scene arrays remain exact prefixes. Blender5.2.2/EEVEE,
16samples/two threads, IrisXe OpenGL: all four render processes terminal0,
3.09–3.94s per render; no Outshine Vulkan/p99 proof. The slab is visibly added
with its ends included. Missing new supports/hardware, open outer ends, sparse
old roads, coarse terrain and simple diagnostic materials remain visible.
No README/doc images or galleries are published.

## Reproduction and remaining work

Use the existing.venv; set
`PYTHONPATH=lib/src:test/experiments/transport:test/experiments/render` and
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`. Freeze actual sources first.

```sh
python -B -m unittest elevated_rail_checks surface_solid_checks shared_bridge_checks
python -B -m elevated_rail_study --resident <verified102> --prior-module <105> --prior-module <106> --prior-module <107> --prior-module <113> --prior-module <114> --output /tmp/outshine-rail/run
python -B -m elevated_rail_display --study /tmp/outshine-rail/run --baseline-scene <verified114-display>/whole-scene.json --output /tmp/outshine-rail/display
python -B test/experiments/transport/render_transport_research.py /tmp/outshine-rail/display/whole-flat-scene.json
```

The study records every concrete resident/prior path and report SHA; prepared
products follow #1754's declared lineage and remain protected inputs, not hidden
normal-test dependencies. Local qualification:
`/tmp/outshine-research-raised-rail-f83a85837-evidence/` (`outshine-accepted`,
`outshine-accepted-display`, `accepted-cost`, terminal logs and source witnesses).
Fresh private system-temp outputs only. Piers/foundations, rail hardware,
curvature/C1, external routes, Buildings/Plants and elevated-product replay,
native/Source/World admission, actual device p99 and UMA remain open. This slab
candidate is not a completed functional railway or a114→117 World admission.
