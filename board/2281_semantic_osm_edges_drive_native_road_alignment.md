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
- First envelope estimates, in metres, are explicit width first, otherwise
  motor-lane count × 3.25, otherwise class fallback: motorway 7.5, trunk and
  arterial 7, local 6, service 4.5, track 3, raceway 12, walkway 2,
  cycleway 2.5, rail 3.5, water/ferry 8. Raceway keeps at least 12 unless an
  explicit width overrides it. Numeric `width` accepts metres and decimal
  feet; malformed width/lanes reject with Way ID. These are declared visual
  assumptions, not legal standards; measure and revise against independent
  geometry and images. Unknown explicit surfaces remain `Unknown`.

`OsmWaySemantics` normalizes corridor properties before topology creation;
`TangentFrame` belongs to `base/spatial`. `RoadConstraintChain` and
`RoadAlignmentBuilder` retain all 267 Hockenheim source edges with pinned DEM.
`SourcedTerrainFields` owns an immutable height-field snapshot;
`RoadTerrainPinJob` selects bounded route tiles and defers missing fields with
source/candidate provenance. The candidate captures one transport snapshot,
includes its generation in rebuild matching, and requests up to 256 route DEM
tiles at its source zoom. `RoadHeightCoverage` admits at most 512 route edges;
larger routes are reported deferred until sliding-window coverage exists.
- `RoadConstraintChain` is the bounded preparation input to the solver: an
  ordered, connected set of directed OSM edge IDs, pinned terrain samples,
  geographic/local node positions, width, material class and structure intent.
  It reports source IDs for missing/disconnected/duplicate/unusable edges and
  missing DEM. Its chord length is an estimate, never vehicle chainage; bridge
  and tunnel points are terrain constraints, not fabricated deck geometry.
- First solve grounded chains with a C1 cubic through source nodes and periodic
  tangents for closed circuits. Integrate arc length into bounded per-edge
  tables; publish monotonic edge intervals and pose queries by source ID or
  global station. Reject cusps and unresolved bridge/tunnel edges explicitly.
  This is internal centerline geometry until terrain-interior clearance and
  render/contact products pass; a successful camera query is not a road proof.
- Candidate integration gathers unique DEM tile spots from selected source
  nodes at the candidate's declared zoom, with an explicit tile-count budget.
  `HeightSheets::CopySourcedField` pins each tile from the candidate; a missing
  tile defers publication, never invokes a scalar fallback. Copy work is paced
  off the frame path. Source identity, DEM source set/digest and candidate
  generation travel with the result; stale completion cannot replace a newer
  candidate. Test missing/changed tiles and normal Hockenheim DEM coverage.
  Selection, candidate field requests and off-frame pin/alignment with stale-result
  rejection are implemented. The normal Hockenheim client shot reports six
  requested DEM tiles, 267 aligned source edges and 20 terrain-source records.
  Its PNG still lacks the raceway: derive matched road render/contact products.
- `generators/road` owns `RoadAlignmentBuilder` and immutable `RoadAlignment`.
  Input is one `TransportNetworkSnapshot` revision, a bounded ordered set of
  source edge IDs with source identity selected by route or coverage, and pinned
  DEM samples from the ground candidate. Never borrow mutable live
  `GroundQuery` in a worker or require DEM for unselected network edges.
  Reject duplicate, absent, forbidden
  or wrong-revision selections with source IDs. Each selected edge has
  double-precision station, centerline, tangent, width, bank and height/clearance
  constraints. The output maps each requested source edge ID to its interval
  and records the exact source/DEM revisions.
- A bounded local solve enforces common endpoint position and tangent where
  edges are logically connected. Grounded portions sample DEM; bridge decks
  clear underpasses/water and stamp only supports/approaches; tunnel lanes
  retain cover and open portals. Incomplete DEM or conflicting constraints
  leave the candidate pending/failed with IDs and residuals. Never invent an
  at-grade join at an XY crossing without a graph node.
- Road render and contact products sample the same published alignment and
  native material specification. The MVT corridor path may coexist only for
  edges with no semantic source coverage; overlap suppresses duplicate roads.
  `RoadSurfaceBuilder` now emits native Geometry, edge/station/triangle spans,
  material groups and source-derived earthworks off-thread. Bounded OSM route
  corridors now ask terrain LOD for <=3 m postings before earthworks. The
  Hockenheim shot has 267 corridors, 139 virtual patches and 148 total pages.
  Six pinned z15 Terrarium
  tiles vary 100.05–118.86 m; raw offset reaches 1.527 m, then 5×3 samples
  per segment clear 0.045–0.087 m. At 3 m terrain postings a 3 m verge left
  6.4 mm penetration; shared 5 m verge leaves 24 mm in the pinned mesh test.
  Runtime PNG occlusion and MVT overlap remain unproved.

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
