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

Frozen-run measurements, source commit and selected artifact hashes follow in
the linked delivery PR after the fresh generation is independently replayed.
