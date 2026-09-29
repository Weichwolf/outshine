Type: feature
State: active
Architecture: ready
Parent: 2173
Depends:
Priority: P0
Area: world, data, navigation, engine, streaming
Tags: osm, worldwide, source-cells, residency

# Geodetic OSM cells stream independently of render tiles

## Problem and boundary
`OsmTransportLoader` accepts four local XML chunks and publishes one regional graph.
Worldwide semantic residency must be independent of VectorStreetGraph, render LOD
and camera tile eviction; a larger chunk cap does not establish that ownership.
Preserve the regional adapter and connect the same original elements to building
generation. Existing source identity and transport publication from 2278 are usable;
completion of its deferred driving acceptance does not block the visual milestone.

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
3. `world/data` owns immutable decoded source regions and typed cross-cell OSM IDs.
   `world/navigation` derives transport; `world/ground` supplies native building inputs
   to the existing generators. Equal overlapping objects deduplicate; conflicting IDs
   reject replacement. A cell with unresolved way/relation references declares
   neighbor dependencies and cannot silently publish disconnected edges.
   A route pins its required graph cells; visual tile eviction is irrelevant.
4. `engine/streaming` schedules cells for camera and active route corridors
   with bounded admission, stale-request cancellation and explicit retention
   budgets. Candidate publication is atomic per coherent source revision;
   overload defers work rather than blocking a frame. No global `OsmElements`
   merge and no graph reconstruction from rendered street meshes.
## Camera turns and storage hierarchy

Streaming demand covers all azimuths around the camera, independently of the render
frustum. Distance-appropriate render products remain resident before visibility changes;
a fast 180-degree turn must not expose holes or wait for disk, decode or generation.
Keep the configured horizon through resident distant representations. Detailed nearby
products, movement prefetch and eviction hysteresis share a bounded working set.
`engine/streaming` owns demand and retention; the renderer culls drawing independently.
SSD holds downloaded original source bytes only, never generated geometry, LODs or
material products. RAM holds bounded source indices, decode/upload staging and active
CPU products; GPU residency holds immediately drawable representations. Derive disk
capacity from source reuse and byte sizes, IO admission from latency and throughput;
a fixed quota that repeatedly evicts the active area is not an accepted budget.
Acceptance includes rapid full rotations and movement back across cell boundaries:
complete images, no frame-thread IO, bounded transient overlap and measured p99.

## First complete delivery

- Connect two adjacent original-source regions through the existing reader and source
  identity to native transport and building products in outshine-client. Preserve IDs,
  tags, relation roles, holes and building parts through generation. A source address or
  manifest lookup alone does not complete the delivery. No Place-specific geometry.
- Spatial responses may contain incomplete distant relations. Retain unresolved references
  and compute closure for the consuming product: building multipolygons/parts and selected
  transport connections must be complete; unrelated remote route members must not block
  an otherwise complete building. Missing required dependencies remain Pending or fail
  explicitly. Fully declared local chunk sets retain their existing strict contract.
- Equal overlapping OSM objects deduplicate by typed identity; conflicting revisions reject
  replacement. Shared nodes preserve network connectivity across the seam; equal coordinates
  without shared IDs do not establish identity. Publication retains the previous valid world
  until the replacement and its dependencies are complete.
- Original source objects replace reduced map-tile structures in the connected region;
  no duplicate overlay or silent fallback to missing semantics. A tagged chimney must reach
  its own generator instead of the generic windowed facade. Unconsumed tags remain available.
- Measure cold/warm source bytes, complete preload, resident bytes and the fixed-station
  360-degree turn. Format, source/navigation/generator suites, full lint and opened Places
  gate the integrated delivery. Regional source tests supplement all-Place acceptance;
  Hockenheim driving and a completed worldwide router are not prerequisites.

## Active implementation boundary

- `OsmSourceSnapshot` is shared source ownership, not a transport publication prerequisite.
  Spatial source readiness precedes independent building/transport derivation; the existing
  OsmTransportLoader always builds a globally validated graph and cannot gate spatial buildings.
- `BuildingField::Footprint::FirstPoint` indexes `OsmField::Points`, FirstHole its Rings;
  Laying passes both to BuildingStampJob. Native products must own/share ring coordinates
  and revision. Meshing, footprints, terrain stamps and GroundDiagnostics consume that same
  immutable geometry owner; no copied MVT surrogate and no index across unrelated buffers.
- `OsmBuildingFootprints` now owns closed ways and outer/inner multipolygon chains, pins
  the source snapshot and retains typed IDs and tags. Product-root closure is implemented.
  Point structures and type=building groups remain unsupported; part ownership and semantics
  remain open. Preserve strict local complete-set and graph validation.
- `RawTile`, `StructurePlan` and terrain stamps now retain inner rings and minimum height.
  Courtyards bypass solid aggregation; their roofs and floors preserve all inner boundaries.
  Native source ownership, precise roof forms and building classes still need connection.
  Do not encode original objects into reduced vector-tile properties as an intermediate fix.
- `StructureBuildQueue` admits source-backed building jobs with the existing DEM sampling,
  cancellation, cell batching and atomic replacement. Source coverage owns replacement
  selection; render-tile overlap must not duplicate an original building. Keep existing
  roads until their original-source replacement is complete and visually verified.
- First visible acceptance: a source-tagged chimney has no residential windows; a courtyard
  remains open and a raised building part preserves its clearance. The same two-region
  scene retains shared-node road connectivity and unchanged content during a full turn.
  A parser-only success or a source accessor without this client path is not completion.
## Native geometry requirements
`StructurePlan`/`BuildingShape::MassOf` preserve exterior, holes and raised-part clearance at every LOD.
Raised parts have no plinth, pavement or ground stamp; aggregation cannot fill holes or clearance.
Mapbox Earcut supplies perforated roof/floor triangles before roof-crease clipping; massing stays unsplit.
Mesh and EarthworkStamp holes share pinned points and source revision; native multipolygons need containment ownership.
Exact roof semantics still need BuildingShape::Order; PitchedShare is not the source roof family.

`world/ground/OsmBuildingHeights` reads metric height/min_height and levels/min_level
from retained original tags. Missing, valid explicit and malformed/duplicate values remain
separate; units normalize at this boundary, raw strings stay in the pinned source.
`OsmBuildingFootprints::Heights` exposes those values to native job construction. No tile
sentinel becomes measured height. Contradictory intervals are resolved only by a declared
generator policy; parsing neither adds heights nor discards the original building.
