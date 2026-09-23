Type: feature
State: active
Architecture: ready
Parent: 2173
Depends:
Priority: P0
Area: world, data
Tags: osm, topology, importer, hockenheim

# OSM source elements retain IDs, tags and relation roles

## Problem and owner

VersaTiles MVT z14 omits Hockenheim `highway=raceway` and the circuit
relation. `ProviderFeatureId` cannot be assumed equal to an OSM way ID. The
33,557-byte `test/outshine/integration/places/HockenheimringGrandPrix.osm`
pin contains real OSM nodes, ways and relation 284588, including the pitlane
role. `src/world/data/` owns a source-format reader; navigation and generators
must consume native elements, not XML or MVT codec objects.

Add `OsmElements` with separately keyed node/way/relation IDs, ordered node
references, member type/ref/role and owned tags. Add a bounded `OsmXmlReader`
adapter using `base/format/Xml` for compact XML source chunks. Preserve way
node order and relation member order exactly; neither order is route direction.
Read-only positive IDs are uint64. Duplicate IDs within a kind, invalid
coordinates, invalid member kinds and malformed required numbers fail as
typed `std::expected` errors without publishing partial output. A missing
referenced object is explicit at closure/query time because streamed chunks
may arrive separately. Keep source provenance outside the element IDs.

This XML adapter proves source semantics for the pinned fixture; it does not
turn `/api/0.6/map` into a worldwide realtime service. A bounded regional
PBF/PMTiles source and revision-aware stitching remain separate provider work.

## Acceptance

- Read the pin through the adapter: 319 nodes, 17 ways, one circuit relation;
  16 unroled ways plus one `pitlane`. All 16 main ways are `highway=raceway`,
  `oneway=yes`; retain all node references and tag values verbatim.
- Changed node order, duplicate ID, missing required attribute, NaN/out-of-
  range coordinate, unknown member kind, truncated XML and missing relation
  member each fail at their own contract boundary. Corrected input recovers.
- Assert the 16-way main relation closes by directed node IDs and that the
  pitlane is excluded by role, not by an ID constant or a geometric guess.
  `make format`, focused suite and `make lint` pass. No Hockenheim route is
  published by this WI; WI 2260 owns route construction and camera acceptance.
