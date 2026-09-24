Type: defect
State: active
Architecture: ready
Parent: 2281
Depends:
Priority: P0
Area: generators, terrain, engine
Tags: road, earthworks, terrain, hockenheim, image

# Continuous road earthworks remove segment shoulder waves

## Problem and evidence

The Hockenheim lap near station 1833 m has repeated dark/light bands on the
green shoulders beside the gray sourced road. Turning off the shadow stage
leaves the 320 x 30 road-region pixels unchanged. A terrain-only provider with
the same pinned OSM route, no vector source and no vegetation still shows the
bands. MVT overlap and shadow-map aliasing therefore do not explain them.
`RoadSurfaceBuilder::EarthworkFor` emits one trapezoid `EarthworkStamp` per
approximately 2 m road segment, each with its own plane and 6 m apron.
`EarthworkPress::PressesAt` composes per-stamp cut/fill bids. Discontinuous
apron distances or grade extrapolation at stamp joins is a testable hypothesis,
not yet a proven cause.

## Contract and ownership

`generators/road` owns the grounded alignment-to-earthwork conversion.
`generators/terrain` owns one deterministic field evaluation at each DEM
node; the engine only transports immutable products. Road surface, contact
height and earthwork use the same published alignment revision and stations.
Do not tune the Hockenheim image by place, source Way ID, apron width alone,
lighting or material color.

Replace independent road-segment bids with a bounded profiled corridor
primitive. Store centerline station, ENU position, bed height and half-width
knots; query candidate spans through a spatial index. At a ground point choose
the nearest finite centerline span, interpolate bed height at its station, and
derive signed lateral clearance from the continuous width profile. Clamp at
true route endpoints; adjacent source-edge joins are interior. Partition long
routes into bounded chunks with neighbor halos, keeping the geometric answer
independent of chunk and source-edge boundaries. Apply one cut/fill batter from
distance outside the road/verge, preserving the existing maximum earthwork
limit and water/structure exclusion. Crossings at different levels stay
separate by their structure constraints; do not flatten under bridge decks or
over tunnel roofs. Report rejected constraints with source-edge/station.

First measure current stamp and pressed DEM values at fixed lateral offsets
through station 1833 m. Show whether maxima/minima align with stamp ends.
If this falsifies the stamp hypothesis, record the measured cause and amend
this contract before implementation.

## Falsifiable acceptance

- Synthetic straight, curved and graded alignments with irregular source-edge
  partition: identical geometry sampled with different 2 m stamp/chunk
  boundaries yields the same pressed heights to floating-point tolerance.
  For analytic resampling, error is bounded by measured chord/grade error.
- Sample left and right shoulder at fixed signed offsets across every seam.
  Height and first-difference residuals stay within the analytic profile and
  batter bounds; no periodic impulse at segment boundaries. Road clearance
  remains nonnegative and render/contact geometry shares the same station.
- Negatives: bridge over water leaves the water channel open; tunnel cover is
  retained; disconnected XY-crossing roads are not fused; a rejected local
  cut/fill does not partially publish. Ordering and chunk size do not change
  result. Peak scratch memory and work per DEM node are bounded and measured.
- Render the public Hockenheim scenario with and without vector provider at
  91.7 s and at a high overhead camera. Open both PNGs; the repeated shoulder
  waves disappear while road silhouette, route station and river/structures
  remain. Report p50/p95/p99 frame time and cold cached preparation cost.
- `make format`, focused road/earthwork suites and `make lint` pass.
