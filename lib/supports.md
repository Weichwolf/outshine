# Whole support contact volumes

Owner: [#332](https://github.com/Weichwolf/outshine/issues/332), transport #50/#21.
This installs the whole-contact construction verified in #326/PR #330.

```python
from outshine_ref.transport.supports import SupportVolumeBuilder, SupportBudget

builder = SupportVolumeBuilder(lower_mesh, upper_mesh, SupportBudget())
volume, report = builder.build(footprint, minimum_height_m=2.0)
if volume is not None:
    body = volume.body
    bottom = volume.lower_contact
    top = volume.upper_contact
```

The caller supplies immutable `contracts.Mesh` height fields in the same local
XY/Z metres and a finite valid XY `shapely.Polygon`, including holes. Frame,
source IDs, surface kind/choice, dimensions, physical recipes and reservations
remain caller-owned. There is no automatic ground/highest-surface selection,
traffic exclusion, bridge planning, structural capacity or foundation design.

Preparation retains both immutable source meshes and builds spatial indices.
`preparation_seconds` measures that separately; its work is bounded by source
face counts. Reuse a builder across calls. It is immutable; its private indices
are not shared mutable caller buffers. Release it when generation ends. The
Vienna caller releases these indices before independent verification/exports.

`build` returns `(SupportVolume, report)` or `(None, report)` with `valid=False`
and a concrete reason. Invalid numerical/polygon parameters and degenerate or
vertical input faces raise `ValueError`; a wrong budget type raises `TypeError`.
Coordinate-budget preflight can return absence before detailed polygon checking.
Missing finite contact, locally overlapping/nonconforming source faces,
insufficient whole height, construction failures and exhausted budgets return
no partial unchecked body. An ambiguous distant region does not invalidate a
disjoint admissible footprint.

Original patch edges and the footprint boundary enter one noded arrangement.
Every actual crease partitions both caps; sampled Z never overwrites owned XY.
The resulting body has top faces, reversed bottom faces and outward outer/hole
walls, with no interior walls. All returned Mesh buffers are owned/read-only.
The lower/upper contact XY and topology agree by value; they do not alias the
input meshes. End results remain valid after releasing the builder.

Defaults: at most 262144 input faces per surface, 2048 selected patch faces,
64 footprint coordinates, 8192 arrangement segments, 4096 contact faces,
65536 interface candidates and 1000000 sampling candidates. Positive integer
limits have explicit ceilings in `SupportBudget`; per-call `seconds=2.0` is
cooperative, at most 60 s. GEOS/numerical calls can overrun a step. Allocation/work
limits and isolated worker limits remain separate from a hard wall-time claim.

Before noding, original straight segments enter a bounded intersection query.
Each pair can add at most two cut endpoints to each participant, so
`input_segment_count + 4*pair_count` conservatively bounds the emitted segment
count. Endpoint/collinear pairs also count. Reject before arrangement emission
when that bound exceeds the segment budget. Each query holds at most the bounded
input-segment count; the output limit is also checked after GEOS. This is an own
conservative work admission, not an optimal or exact arrangement-size estimate.

Selected source patches must cover the whole requested footprint to 1e−8 m²;
output coverage has the same limit. Sampling uses the shared `SurfaceSampler`
XY 1e−8 m/height 1e−7 m consistency rules. On each common affine cell, the minimum
upper-minus-lower height occurs at a vertex; all vertices must satisfy the
explicit positive `minimum_height_m`. Independent world/source/clearance and
closed-volume replay remains the caller's responsibility. This is Float64
geometry; renderer/device casts require their own checks.

No new package or SDK dependency. Runtime needs the existing NumPy/Shapely
reference dependencies. Primary references, analytic/real controls, fresh-wheel
evidence, complete Vienna identity, render comparison and measured costs are in
[the migration comparison](../test/experiments/transport/support_library_comparison.md).
