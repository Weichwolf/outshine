# Joint Vienna ground below the bridge/portal reference

Owner: [#299](https://github.com/Weichwolf/outshine/issues/299), under #20/#21.
Experimental Python geometry only; existing library and native products remain
unchanged. The new lower road heights are a separate plan, not old contact proofs.

## Construction and alternatives

The old lower roads cannot all keep their heights in a continuous terrain whose
gradient is at most 2/3: vertices 171/172 are 1.657585064 m apart with a
1.869841840 m height difference. Their entire straight connecting segment lies
inside the new ground domain. The necessary slope is at least 1.128051815.
This independent contradiction explains the failed fixed-contact solver.

The joint alternative keeps the old XY road faces and source IDs, allows their
heights to decrease, and fits road/soil vertices together to cached DEM. Road
grades remain 0.025, soil grades 2/3, normalized tolerance 1e-7. Upper bridge/C1
and portal geometry remains unchanged. A conforming partition fills the real
three-tile region, extended only for old retained road footprints, excluding
actual open portal trenches. The terrain body is a closed 0.25 m shell.

Affine upper/lower barriers constrain **all corners of every positive-area face
overlap**, including intersections of crossing triangle edges. Existing upper
deck undersides retain a 0.02 m soil separation; tunnel roofs require 2 m below
the soil shell underside. Independent normal/overlap checks replay the result;
solver completion/status alone is insufficient. Candidates violating unchanged
limits are rejected, without contraction toward a possibly infeasible datum.

The identical quadratic/conic problem compares equal vertex weights with lumped
triangle-area weights. Both satisfy the same geometric constraints. The simpler
equal-weight solution is selected: area weights improve DEM RMS by only about
1.6 mm in the initial comparison, at greater solve cost. No visible gain justified
paying that cost. A retaining wall is unnecessary here because the lower roads
have no newly required immutable consumer contacts. It remains an alternative
for future truly fixed contacts; no untested wall implementation is added.

The old exact boundary predicate falsely rejected two collinear clipped portal
edges due to floating-point construction. The existing retained-surface XY
tolerance of **1e-8 m** is applied to entire boundary segments. Edge manifoldness,
conforming coverage and independent domain-difference checks remain mandatory.
Height and slope tolerances are unchanged.

## Sources and scope

First reference: [Chen et al., SIGGRAPH 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page 2/Figure 2, separates network planning and actual 3D geometry. Terrain
excavation/embankment follows the design motivation in
[Galin et al., Eurographics 2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
section 6.2/page 9/Figure 20. Both pages were actually opened during this work.
The bounded joint conic fit and affine barrier construction are our own extension,
not a claim to reproduce either paper or CityEngine. No SDK runtime or foreign code.

Original OpenFreeMap snapshot and all three Vienna tiles are unchanged from
PR #294. Pinned fixture SHA256s: bridge `fcc91443d76d69223c8173061cf1f2f69d78446a9cc26fd58d9482c3e4f68db4`,
portal `f7b728c98e21bda79beb38cceea0e8152361af64451e11a73a8e4267b6793c56`.
Fresh receipts preserve DEM hashes, origin, old whole-body reference hashes and
all 3,259 per-source usage records. Only the existing 71 sources are used: filling
the ground does not generate the omitted traffic network.

The terrain remains coarse C0 facets and a rendering/collision shell, not a solid
earth excavation or complete portal retaining structure. Changed lower contacts
need new binding before consumer adoption. Piers, rail hardware, all tile traffic,
train routing, C1 soil and native/device costs are not established.

## Reproduction and image decision

See the explicit external-path command in [README](README.md).
Each method runs five times in its own process with 2 GiB address-space and
20 s CPU limits, an 8 s per-solve limit and a 25 s supervising wall timeout.
A small standard-library supervisor separates worker RSS from the larger
generation parent's inherited fork footprint. Report all five wall samples and
Linux worker peak RSS; no device or frame-rate inference.

The fresh before/after render uses exactly the same regional camera, palette,
Blender 4.3.2/Workbench and 1280×720 profile. No insets, cuts or exaggeration.
The first opened terrain view is darker, still sparse, and not a clear image
improvement. Geometric progress does not by itself satisfy the visual gate.
The newer publication rule in #47/AGENTS removes research galleries entirely:
all these images stay local; only master publishes integrated Places. No
replacement gallery is created.

## Frozen result

Fresh generation and 66 affected tests pass on clean
`3784b07cf65124492de89fe803a78723b3a8b40b`. All numerical functions are at most
120 lines; Ruff, formatting, diff and documentation checks pass. The eight old
portal counterexamples still use the original hash-checked external bridge
reference. No pins, old recipe tests or public library APIs changed.
Environment: Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Clarabel 0.11.1,
Shapely 2.2.0/GEOS 3.14.1, Pillow 12.3.0; Blender 4.3.2/Workbench.

| Variant | Median of five fit wall times, ms | Worker peak, MiB | Area-weighted DEM vertex RMS, m | Independent geometric result |
|---|---:|---:|---:|---|
| Fixed old contacts |59.303|91.168|absent|Impossible; independent contact contradiction |
| Equal weights, selected |197.380|94.719|0.6629034923|Pass |
| Area weights |255.154|94.711|0.6612803221|Pass |

Five equal-weight samples: 198.570, 197.622, 197.070, 197.380, 196.884 ms;
area-weight samples: 255.154, 264.811, 248.909, 250.320, 275.692 ms.
Small offline case on this Linux host, not native/device/frame costs. Worker RSS
includes interpreter/solver and all five runs; generator/Blender process peaks,
GPU memory and GPU execution time are not measured. Full fresh comparison with
both renders took 13.325 s including setup, checks, workers and image saves.

The common surface covers 7,967,844.8026 m² with 1,022 vertices and 1,819 faces
(834 old traffic, 985 soil). Independent symmetric domain error is
6.57e-12 m². All old road XY/faces remain exact; their new heights decrease by
at most 2.152830 m. Maximum normalized grade is 1.000000000011, within the
unchanged 1e-7 tolerance. Whole lower-road/bridge underside clearance is
**4.534284 m**, all-soil/upper underside separation **0.02 m**, and roof cover
to the soil shell underside **2.999792 m**. Original upper contacts, 4.5 m tunnel
air and old independent recipe regression remain valid. Original source
ledger: 71 used /3,188 explained omissions.

Input SHA256 `e740d969b6b6baa230c50cb1c3475350d8610b1685119c2bb619732acde89442`;
assembled solid `02e2966f21537d6dffe096cfad9a7c3296a991b1c28a47aaf2a34c4730d11a17`.
Opened before PNG `ce1601a5946ab2a7b5b6050221409f52427688d02c01269c6b06aa0be3ca09a1`;
after PNG `b718e0be723909b3c74a00bedc7506bc043e0826f3b3e1189f41a081156dfb26`.
Both are exactly 1280×720 with identical eye/target/span. These local research
pictures explicitly fail the clear visual-improvement criterion and are not
published. The new common daylight/Blender 5.2.2 profile in #291 is separate.

Fresh inputs, both complete geometry candidates, closed solid, receipts, source
ledger, scene/camera proofs and images are preserved in
`/tmp/outshine-transport-ground-299-final/`. Terminal logs:
`/tmp/outshine-transport-ground-3784b07cf.log` and
`/tmp/outshine-transport-ground-3784b07cf-tests.log`.
