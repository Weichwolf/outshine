Type: feature
State: active
Architecture: ready
Parent: 2133
Depends:
Priority: P0
Area: world, navigation
Tags: osm, graph, hockenheim, transport

# Transport topology uses OSM IDs rather than coordinate snaps

## Boundary and owner

`StreetField` is a visual candidate derived from generalized MVT and
`VegetationTemplates`; it drops tunnels and has no OSM node/way IDs.
`Path::Network::Lay` uses coordinate snapping and `WayClass::Tag`, so it cannot
prove source connectivity or stable tile-independent edge identity. Keep this
legacy renderer input separate. `src/world/navigation/` owns the logical
transport graph; it consumes `Data::OsmElements`, never MVT geometry or a
render mesh. The world-data reader owns only source parsing (2272).

Introduce `TransportTopology` with stable directed edge key
`(source namespace, OSM way ID, node-pair ordinal, direction)` and endpoint
OSM node IDs. Source namespace is the provider dataset ID, never the tile or
revision; revision belongs to the graph snapshot. Store double WGS84
coordinates, transport mode, one-way/access,
layer and bridge/tunnel flags separately from later altitude/alignment.
Connect only shared source node IDs with compatible transport modes; a
bridge/tunnel-to-ground transition at one explicit node remains connected
despite a layer change. XY crossings with distinct IDs never join. Normalize
`oneway=yes/-1/no` at input. Retain tunnels in
the logical graph. A missing node or duplicate conflicting way/edge rejects
the candidate with source-ID diagnostics; never replace the published graph
partially. Sort by stable source keys, not tile arrival or relation order.

Add a generic relation-route resolver: select circuit members by role, resolve
each member way's directed edges, and order a single cycle from node topology.
For Hockenheim relation 284588, unroled ways form the 267-edge main cycle;
`pitlane` is an alternate branch. Ambiguous fork, reversed segment, missing
member or disconnected cycle is an explicit failure. The relation ID is test
input, never a special branch in production. The result is a logical route,
not a camera spline or road mesh; WI 2260 consumes it later.

## Proof and bounded implementation

- Analytical fixtures: straight, one-way reversal, XY crossing on distinct
  source nodes/levels, tunnel with exit, bridge crossing, pit split and an
  incomplete relation. Negative controls must fail if topology is replaced
  by coordinate snapping or relation-list ordering.
- Pinned Hockenheim source: 16 main ways/267 directed edges, one closed cycle,
  one excluded pitlane; perturb a member, node order or role and reject.
  Reverse tile/chunk arrival and permute relation members: identical sorted
  edge IDs and route. No Place or source-ID constant in implementation.
- Keep first build bounded to a supplied regional source snapshot; streaming
  publication/revision work remains WI 2262. Focused suite, `make format` and
  `make lint` pass. This WI does not claim vehicle physics or visual road fit.
