# Whole ring approach planned before freezing (#709, #710)

The [previous 86-source specimen](stair_closed_ground_comparison.md) gains the
whole original **254.696881 m** road `103612620:13`. Both ring sources and this
approach are now one jointly constructed component on the unchanged 84-source
baseline: **87 whole original sources**. This component remains separate from
older ground traffic. This is not a full world generator or legal routing graph.

Source prefix is `openfreemap:14/8937/5678:transportation:`. The joint module uses
`1574593312:0` (145.343730 m minor one-way ring), `103612620:13` (whole approach)
and `80922970:246` (180.578558 m footway ring). Original MVT/metric axes, tags,
source directions and identifiers remain intact. The approach meets the minor
ring at original node `[1594,2345]`; its footway crossing has no shared MVT node.
The recipe is [stair-ring-joint-approach-vienna.json](fixtures/stair-ring-joint-approach-vienna.json).
PBF/DEM/frame pins and original inputs are in the linked closed-ground report.

## Why construction order matters

The frozen 86-source attempt correctly refuses undeclared footway reuse. Declaring
that geometric contact still correctly refuses an old minor-ring facet in the
new road frame: Double normalized grade **1.278454**, Float32 **0.725146**.
Float32 passing cannot certify Double. A separate trial flattens the known ring
junction before freezing it, but then incompatible fixed road/footway heights
require **0.165368339 m** across an edge allowing at most **0.140648891 m**
under the nominal road limits (**0.140311223 m** with the existing solver reserve).
For frame A and horizontal edge d, the maximum permitted height difference is
`norm(solve(A.T,d))`, by the dual norm of `norm(A*gradient) <= 1`.
These are fixed geometric contradictions, not a solver timeout to retry.

The two exact captured countercases are versioned in
[ring-approach-contact-countercases.json](../../outshine/data/transport/ring-approach-contact-countercases.json).
Their temporary full inputs/products and raw witnesses remain retained. Those
unmerged incremental implementations were removed. The old 86-source references
are untouched; they are not silently repaired or relabelled as the new product.

## Joint bounded construction

A seed recipe may declare at most four `geometric_junctions`. Each requires two
whole selected sources, an explicit `minor_footway_crossing`/walk assumption,
ordinary exterior ground/access, one transverse original-axis intersection and
no original shared node. Unsupported/restricted/ambiguous contacts remain
refusals. Coordinates come from actual source axes, not distance-based snapping.
Original source graph nodes and procedural walking contacts remain separate.
No motor/cycle turn onto the footway is added; source along-directions stay intact.

Existing original-node pads and these geometric crossing level regions are built
before triangulation and height fitting. Touching regions merge transitively
(maximum 20 input regions), retaining every prior rather than averaging averages.
A cluster uses the arithmetic mean of its priors rounded to a Float32-representable
height within 5 mm. This is a finite plausible recipe, not a height optimum.
The real node/crossing share one level **163.8831329345703 m**, over **78.352365 m²**
of actual source surface. Whole Double/Float32 level error is zero.
The prior comes from original-axis DEM samples; original input heights are not
survey constraints. All three new sources are fitted together, rather than
forcing a later road between incompatible immutable products.

The joint cap has actual shared facets with both source owners. It contains the
whole ungridded source overlap **9.939314 m²**; actual coowned overlap is
**9.953038 m² / 12 facets** in both precisions. Each side remains within its
unchanged 2.9296875 mm source outward model. Equality to an intermediate snapped
polygon is not the contract: arrangement noding also snaps intersections. No
intersection-wide Hausdorff bound is inferred from individual source bounds.
Both whole through-profiles (road 5.5 m, footway 1.8 m) have positive-area caps on
opposite sides and complete Double/Float32 piecewise height traces.

Two source holes remain; 335 top vertices/454 top facets become a positive closed
670-vertex/1344-facet body, volumes **750.831424/750.831767 m³** (Double/Float32).
Actual road limits remain longitudinal 0.1/crossfall 0.03. Approach maxima D/Q:
longitudinal **0.013368618/0.013368595**, crossfall **0.013061147/0.013061218**.
Circle crossfall maxima are **0.018345113/0.018347398**. Both whole cyclic seams
remain checked. This joint fit changes new ring heights/triangulation relative
to the standalone 86-source version; it is not the standalone 5-mm post-fit repair.

SIGGRAPH first: Chen et al. 2008, [page 2/Figure 2](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
text and figure inspected locally: graph planning precedes 3D construction.
Existing GEOS overlay/constrained triangulation, source grid, shared cone fitting
and full actual-body/contact audits are reused. The specific joint level/crossing
contract is independently defined here, not a reproduction of the paper or SDK.
No foreign code, SDK/runtime dependency, native change or common lib type migration.

## Independent whole world and real consumers

Qualified code: `b6c4d705a1df2791bdf33a0c1bb81712ff0cc6c3`.
Commit `12af4b2f8` only strengthens the versioned nominal-limit counterexample;
its five focused controls pass again in both modes. Experiment procedures are unchanged.
**220 source + 220 installed controls** pass, with 119 matching reference imports
per mode. The existing 210-file shared library/wheel is unchanged from #708;
wheel SHA256 `e9e8b20e4c27e3bc1397d3e989ed2cbc2da7d255a3e2e360f2d1afefe75d29b6`.
Fresh installed generation and cold reading reproduce all seven artifacts exactly
(52 matching imports). An independent whole rebuild matches all module/layout/
field arrays and **33,628 XYZ terrain facets with exact attributes**.

The local field replaces 477 old facets with 1473/790 points, preserving 105 exact
boundary edges. All 18 earlier road assets and 18 patch meshes are identical
objects backed by immutable bytes; 32,155 active old faces remain outside explicit
removals. Context 1349 faces; 5 old assets/16 old source columns/1496 body facets;
64,736 B copied road working arrays. Local/source/solver/quality limits stay fixed.
Soil maxima D/Q **0.158921/0.300952** stay below 2/3; no constructed soil is added.
All 124 old pier contacts pass, maximum Float32 error **1.517014482e-5 m**.
424 points per precision give exact index/scan results: **10,846 versus 22,824,640**
face tests. Final source/installed state hash:
`7e2a273ecee37c5ab5922cbfb050fcb79b8c6b9bc03f2000e5fdad7f0312ae21`.

Actual spatial handoff has **7358 volumes**, including all **6904 old records
exactly**. The 12 coowned road/footway facets carry their actual maximum source
headroom, 4.5 m, without a new logical supplement. Earlier 15 logical headroom
records stay intact. Fresh current source/installed readers accept these volumes,
65 actual building bodies/reservations and four complete existing plants.
Building before/after body arrays and reservations remain byte-identical to both 84/86 inputs;
only context and ledger reports differ. All four plants retain SOURCE/SITE/pose/
form/normals; four current flattened terrain ordinals shift by -139 and eight
query durations differ. Flat ordinals are not persistent source IDs. All 394
original habitat parts remain accounted for.

Four fresh whole **1280×720** perspective PNGs were rendered and actually opened,
with identical camera/palette/backend before and after. The new approach is
visible at the ring in both views. The specimen remains sparse, low-contrast and
coarse in terrain; this is a functional extension, not an integrated city image.
Pixel comparison: overview 1744/921600 changed (0.1892%), mean RGB error 0.008300/255;
network 931 (0.1010%), 0.006500/255. Both have zero black/near-black pixels.
Images stay private in `/tmp`; master alone publishes presentation images.

## Equal-result costs and remaining work

| Variant | Median unprofiled s (min–max) | Profiled s | Allocation events | Cumulative requested B | Incremental tracked peak B | Process peak RSS B |
|---|---:|---:|---:|---:|---:|---:|
| local | 6.750608 (5.628358–7.514374) | 11.898901 | 24,253,682 | 3,385,463,978 | 5,143,446 | 156,577,792 |
| whole | 32.484938 (29.756357–32.890808) | 82.340698 | 170,360,294 | 19,274,716,346 | 59,727,827 | 224,628,736 |

Both alternatives reproduce result hash
`fa6f64932d6ff2e065f87dc7a8e54f1104a780866e08482463f8288a271862b5`.
The local step still requests **3.39 GB / 24.25 million allocation events**;
these costs do not meet a runtime budget. All eight construction commands and
both official stats commands finished with status 0 and complete fresh metadata.

Imports, source/resident preload, result hashing and export are excluded; each
variant constructs the same module/local-layout/local-field arrays. Local
composition preserves immutable outside; the whole control rebuilds/rechecks
unchanged world content too. This is not a comparison with an optimized native
runtime. The old sequential approaches have no valid equal 87-source result,
so their fast refusals are not a performance alternative.
Three unprofiled trials alternate local,whole,whole,local,local,whole; separate
subsequent Memray 1.20.0 traces include Python/native allocations without native
call stacks. Allocating-event totals and cumulative requested bytes are not live
RAM or copied bytes; incremental tracked peaks exclude preloaded residents.
Total copy traffic/bandwidth remains unmeasured. Whole road working copies are
reported above; world-control outside copying remains diagnostic.
Measurement runs use normal priority, gates/render/readers nice 10. Other agents
share the WSL2 i5-1135G7/four exposed CPUs/16,190,028 KiB host. Load before each
worker is retained; these timings are neither isolated nor A18/RTX acceptance.
Python 3.13.5/NumPy 2.2.4/SciPy 1.18.1/Clarabel 0.11.1/Shapely 2.2.0/GEOS 3.14.1
build the pinned arrays; controls/actual consumers also use NumPy 2.5.4.

Private root `/tmp/outshine-transport-ring-f146de561/` retains original inputs,
failed frozen counterexamples, current module/whole products, consumer/installed
receipts, traces/stats, 3D images and terminal commands. Public reproduction uses
`stair_local_generate.py` and `stair_local_verify.py` with the new versioned joint
fixture and the resident78 + prior79/82/84 chain; `stair_local_reference.py` uses
the verified whole84 baseline. `stair_local_cost.py --mode local|whole` compares
the same fixture/chain/baseline with fresh explicit `/tmp` outputs. Existing
consumer/display/render entrypoints are unchanged; full original paths/pins are
in the previous report. No accepted receipt or reference pin is overwritten.

Unplanned sources remain: 1570 original lines and all 71 areas. Automatic
crossing/component selection, other level/profile families, cyclic cached-seam
attachments, regional frames/ports/LOD, vehicle dynamics and persistent world
planning remain open. General binary64 topology/affine Float32 envelope proof is
still #568. #434/#50/#48 and the worldwide goal remain active; no native/device/
frame/world budget is established by this specimen.
