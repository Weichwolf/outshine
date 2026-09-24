Type: defect
State: active
Architecture: ready
Parent: 2281
Depends:
Priority: P0
Area: generators, road, simulation
Tags: alignment, grade, hockenheim, driving

# Nonuniform OSM node spacing must not kink the road grade

## Proven defect

`RoadAlignmentBuilder::SolveTangents` normalizes the incoming and outgoing
chords, then averages their unit directions equally. This assumes equal node
spacing. In the pinned Hockenheim route, stations 1485.993, 1556.653,
1557.818 and 1575.881 m bracket chords of 70.66, 1.165 and 18.06 m.
The short chord drops 0.0614 m; the preceding long chord rises 0.2943 m.
Equal weights force a sharp vertical tangent change across 1.165 m.
The real-DEM test measures 0.04882 1/m vertical curvature on a 0.5 m
stencil at station 1557 m. A 60 Hz public-client trace peaks at 36.86 m/s²
vertical eye acceleration near station 1556.77 m; five frames exceed 1 g.

An independent fixed-order Gauss integration of cubic speed changed the lap
length by only 7 mm and left curvature at 0.04892 1/m. Arc-table inversion
is therefore not the cause of this spike. The sun-time experiment also leaves
the smaller visible road pattern at mark 4 ambiguous; do not claim its cause
from this kinematic defect.

## Decision

Keep node positions, terrain samples, source IDs and C1 cubics. At a node,
let `a` and `b` be positive incoming/outgoing chord lengths and `u` and `v`
their unit directions. Keep the horizontal components of the existing unit
tangent `c = normalize(u+v)`. The weighted direction
`w = normalize(b*u + a*v)` minimizes `|t-u|²/a + |t-v|²/b` before
normalization. Set the cubic's vertical derivative direction to
`|c_xy| * w_z / |w_xy|`, leaving `c_xy` unchanged. This puts a short edge's
grade transition onto its longer neighbour without altering the horizontal
Hermite curve. Equal lengths reduce to the existing tangent. Open endpoints
use their only chord. Reject zero/nonfinite lengths and near-vertical `w`;
retain the sharp-turn refusal. Weighting the full 3D tangent was rejected:
it moved the Hockenheim centerline over 1 m from source chords.

`generators/road` owns this geometric decision. It must not move DEM nodes
or choose a place-specific speed. The road/contact product, camera and
vehicle use the same resulting alignment. If the remaining vertical profile
still yields unsafe acceleration, create a separate shared-node grade solver
over the logical transport network; do not independently smooth each route
and split junction heights.

## Falsifiable acceptance

- Synthetic equal-length, sharply nonuniform and reversed graded chains
  preserve connected endpoints, C1 tangents, monotone station and source IDs.
  A short chord no longer concentrates the tangent change; bad turns refuse.
- Pinned Hockenheim DEM retains 267 edges and all source-node positions.
  Compare 0.5/2/5 m curvature and the moving camera's vertical acceleration
  against 0.04882/0.01091/0.00542 1/m and 36.86 m/s² respectively.
  A residual over 1 g remains a road-grade defect, not a success claim.
- Recheck road/terrain clearance, source provenance, PNGs at mark 4 and
  other turns, frame p50/p95/p99 and peak memory. No route-specific branch.
- Run `make format`, focused road/Hockenheim suites and `make lint`.
