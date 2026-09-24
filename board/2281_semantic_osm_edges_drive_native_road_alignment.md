Type: feature
State: active
Architecture: ready
Parent: 2175
Depends:
Priority: P0
Area: world, navigation, generators, road, engine
Tags: osm, road, alignment, hockenheim

# Semantic OSM edges drive one native road alignment

## Trigger and current boundary

The pinned Hockenheim scenario publishes a named, directed 267-edge OSM route.
Its rendered road corridors still come from VersaTiles MVT, which omits the
raceway and has no proven mapping from feature IDs to OSM Way IDs. A camera
following the logical route would therefore have no matching rendered surface.
`Ground::StreetField` is a vector-tile rendering intermediate and must not
become the source-ID contract. No authored track spline or place-name branch.

## Ownership and immutable contract

- `world/navigation` converts OSM Way tags once into bounded native corridor
  properties on `TransportEdge`: facility class, usable width, paving and
  structure flags. Preserve Way ID, segment ordinal, direction, layer and
  source revision. Parse explicit width/lanes strictly; missing values use a
  documented class fallback, including `highway=raceway`, independent of place.
  Invalid tags report Way ID; no per-edge strings or XML in hot consumers.
- `generators/road` owns `RoadAlignmentBuilder` and immutable `RoadAlignment`.
  Input is one `TransportNetworkSnapshot` revision and pinned DEM samples from
  the ground candidate, never a mutable live `GroundQuery` borrowed by a
  worker. Each edge has double-precision station, centerline, tangent, width,
  bank and height/clearance constraints. The output maps every source edge ID
  to its interval and records the exact source/DEM revisions.
- A bounded local solve enforces common endpoint position and tangent where
  edges are logically connected. Grounded portions sample DEM; bridge decks
  clear underpasses/water and stamp only supports/approaches; tunnel lanes
  retain cover and open portals. Incomplete DEM or conflicting constraints
  leave the candidate pending/failed with IDs and residuals. Never invent an
  at-grade join at an XY crossing without a graph node.
- Road render and contact products sample the same published alignment and
  native material specification. The MVT corridor path may coexist only for
  edges with no semantic source coverage; a spatial overlap suppresses its
  duplicate visible road. Renderer/physics consume products, not OSM tags.

## Executable sequence and falsification

1. Start with analytic straight, curved, graded, at-grade junction and
   bridge-over-water cases. Prove edge-ID lookup, monotonic stations, tangent
   continuity, width and clearance under reversed member order and source
   revision change. Negatives: missing DEM, wrong-way edge, mismatched source
   revision, two unconnected XY crossing levels and impossible portal grade.
2. Build Hockenheim from the pinned OSM route and normal DEM candidate. All
   267 edges must map to one closed alignment; seam position/tangent/grade
   tolerances derive from the chosen interpolation and are recorded in metres,
   radians and slope. No sampled station may jump or use the pitlane. The
   builder runs off the frame path with a bounded job and atomic publication.
3. Produce visible road and contact geometry from that alignment, retaining
   route-edge/station provenance. Headless and rendered queries return the
   same centerline. A route segment omitted by the old MVT provider is visibly
   paved; moving focus and render-tile eviction keep route identity while
   geometry residency changes. Inspect PNGs and measured p50/p95/p99.

WI 2260 consumes this alignment for the kinematic camera lap. WI 2261 later
uses the same alignment and contact product for a dynamic vehicle. Do not
declare success from a centerline alone or from a road mesh without edge IDs.
