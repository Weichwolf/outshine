Type: defect
State: active
Architecture: ready
Parent: 2281
Depends:
Priority: P0
Area: generators, road, simulation
Tags: alignment, arc-length, hockenheim, driving

# Road stations preserve continuous motion across arc-table samples

## Problem and evidence

`RoadAlignmentBuilder` accumulates chord lengths at <=0.5 m intervals.
`RoadAlignment::SampleEdge` linearly interpolates parameter within each
distance interval. The inverse has different derivatives on either side of
a table knot even when the underlying cubic is smooth. In the pinned real-DEM
Hockenheim alignment, the largest sampled vertical curvature is 0.04882 1/m
at station 1557 m with a 0.5 m stencil, versus 0.01091 1/m at 2 m and
0.00542 1/m at 5 m. The moving public client trace peaks at 36.86 m/s²
vertical eye acceleration near station 1556.77 m, speed about 24.8 m/s;
five frames exceed 1 g. A straight cubic with constant speed is a counterexample
to blaming every table: its chord-linear inverse is exact. Real vertical grade
and the camera normal may also contribute, so this cause remains falsifiable.

## Contract and ownership

`generators/road/RoadAlignment` owns the arc-length mapping; the engine and
client consume its immutable pose API. Build one strictly increasing distance
table from a fixed-order numerical integral of the cubic's speed |C'(t)|,
not chord lengths. Invert the same integral inside the selected interval by
a bounded safeguarded Newton solve with bisection fallback. Endpoints return
exact table parameters. Reject nonfinite speed, zero-length or nonmonotone
intervals with the source edge ID. Keep source node positions, source IDs,
widths and topology; do not smooth DEM heights as a proxy for this fix.

The numerical tolerance is in metres and the iteration cap is fixed. The
mapping is monotone, bounded to [0,1], and gives the same unit tangent from
either side of an internal table knot. At source-edge joins, the pre-existing
C1 geometric tangent remains continuous; a remaining acceleration spike must
be attributed to actual vertical geometry or camera motion, then solved in
the road-grade contract rather than hidden by lowering speed or samples.

## Falsifiable acceptance

- Analytic straight/graded curves return exact stations, endpoints and source
  IDs; a curved cubic agrees with an independent fine numerical integral.
- Extreme curvature, reversed/zero derivatives and nonfinite inputs refuse or
  remain bounded; no inversion crosses a table interval or loops unboundedly.
- Pinned Hockenheim DEM keeps all 267 connected edges. Measure 0.5/2/5 m
  vertical curvature and the public 60 Hz camera's acceleration before/after.
  The knot-driven spike must fall materially; a residual over 1 g is an open
  grade-design defect, never declared physically plausible by this WI.
- Compare full-lap station monotonicity, contact/render geometry, source
  provenance, PNGs, p50/p95/p99 and peak bytes. The changed arc length is
  reported, not hidden behind an old hardcoded station value.
- Run `make format`, focused road/Hockenheim suites and `make lint`.
