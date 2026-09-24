Type: feature
State: open
Architecture: ready
Parent: 2173
Depends: 2278
Priority: P1
Area: world, data, navigation, engine, streaming
Tags: osm, worldwide, source-cells, residency

# Geodetic OSM cells stream independently of render tiles

## Problem and boundary

WI 2278's `OsmTransportLoader` accepts at most four local XML chunks and
publishes one regional graph. This is sufficient for the early Hockenheim
route, but increasing its chunk cap or declaring thousands of files in one
scenario cannot make a worldwide source. Semantic graph residency must be
independent of `Ground::VectorStreetGraph`, render LOD and camera tile eviction.
Preserve the regional adapter; introduce cell identity and bounded residency
before extending route distance or source acquisition.

## Architecture decision

1. `world/data` owns `GeoCellId(level,x,y)` with `level <= 24` and each axis
   below `2^level`. It partitions longitude `[-180,180)` and latitude
   `[-90,90]`; x wraps at the antimeridian, y does not wrap at a pole. Bounds
   are half-open except the outer north/pole edge. This is a source index,
   not a metre grid or the Mercator `TileId` used by DEM/vector rendering.
2. A versioned `OsmCellSource` contract maps `(dataset, revision, GeoCellId)`
   to bounded byte acquisition or a typed missing/error result. The initial
   local fixture adapter may use an indexed manifest; it cannot read or parse
   on the simulation thread. A single source declaration names the catalog,
   not one `SourceProvider` per cell. Specify ownership, path resolution and
   per-cell byte/hash pins in that manifest before parser implementation.
3. `world/navigation` owns immutable per-cell semantic products and stable
   cross-cell OSM IDs. Equal overlapping objects deduplicate; conflicting IDs
   reject replacement. A cell with unresolved way/relation references declares
   neighbor dependencies and cannot silently publish disconnected edges.
   A route pins its required graph cells; visual tile eviction is irrelevant.
4. `engine/streaming` schedules cells for camera and active route corridors
   with bounded admission, stale-request cancellation and explicit retention
   budgets. Candidate publication is atomic per coherent source revision;
   overload defers work rather than blocking a frame. No global `OsmElements`
   merge and no graph reconstruction from rendered street meshes.

## First executable slice and negative controls

- Implement and test only `GeoCellId` bounds, address normalization and a
  small immutable local catalog lookup in `world/data`; leave Engine wiring
  for the next slice. Verify exhaustive cover/disjointness at small levels,
  antimeridian wrap, both poles, invalid levels/indices and deterministic
  lookup independent of manifest row order. A deliberately Mercator-only
  implementation must fail the polar test.
- Then two adjacent source cells with one shared OSM node must preserve edge
  identity as focus crosses the seam; same XY without shared ID stays separate.
  Revision change, conflicting overlap and missing referenced node reject
  the candidate while the last valid route remains usable.
- Measure cold/warm cell load bytes/time, resident cells, queue depth and
  p50/p95/p99 frame time while moving out and back. `make format`, focused
  source/navigation/runtime suites and `LINT_JOBS=2 make lint` gate each code
  slice. Hockenheim camera acceptance remains WI 2260.
