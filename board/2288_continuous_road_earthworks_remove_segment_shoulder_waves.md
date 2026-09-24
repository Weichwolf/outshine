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
`EarthworkPress::PressesAt` composes per-stamp cut/fill bids. A pinned DEM
probe at station 1780–1880 m, every 0.25 m at ±10 m lateral offset, found raw
height impulses of 0.5–1.5 mm but pressed impulses of 39–49 mm near station
1837 m. Stamp 978 ends at 1837.008 m; the winning bid switches to stamp 979
on one side, while the other side jumps within stamp 978's apron. At +12 m
another 47.5 mm impulse appears near a stamp transition; ±14/16 m are
unpressed. The discontinuity is in earthwork field evaluation, before terrain
mesh and lighting. The exact contribution of plane extrapolation versus apron
distance remains to be isolated.

## Contract and ownership

`generators/road` owns the grounded alignment-to-earthwork conversion.
`generators/terrain` owns one deterministic field evaluation at each DEM
node; the engine only transports immutable products. Road surface, contact
height and earthwork use the same published alignment revision and stations.
Do not tune the Hockenheim image by place, source Way ID, apron width alone,
lighting or material color.

Replace independent road-segment bids with one profiled-corridor bid per DEM
node. Use C1 alignment tangents for short cubic spans, and the tile candidate
grid to bound queries. Under pavement, the nearest finite span sets exact
contact height. In the verge and apron, integrate nearby same-route spans
with arc-length weights and a compact C2 taper, then fade grading to raw DEM.
This handles the pinned hairpin's 12 m turn radius, whose inner 10 m lateral
query approaches the medial axis and has no stable single closest station.
Route keys derive from source identity and ordered edge IDs; another route
cannot enter that height average. Source-edge and tessellation boundaries
must not change the field. Preserve the maximum earthwork and water/structure
exclusions. At multi-level crossings do not flatten under bridge decks or
over tunnel roofs; report rejected constraints with source edge and station.

The pinned regression scans ±8–17 m every 0.25 m over station 1780–1880 m.
It now measures at most 6.33 mm pressed second difference, versus 49 mm
before the profile path. A graded analytic corridor differs by 0.27 mm
between 2 m and 1 m subdivision and by roundoff under reversed input order.
No-road-earthworks rendering removes the visible bands; the profiled path
reduces but does not yet eliminate them in the high-camera PNG. Curved and
multi-level negative cases remain open.

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
