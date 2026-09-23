Type: defect
State: active
Architecture: ready
Parent: 2173
Depends:
Priority: P0
Area: world, ground
Tags: osm, identity, streaming

# Vector feature identity survives decoding and tile publication

## Evidence and contract

The MVT 2.1 `Feature.id` field is an optional uint64. `OsmVector::ReadFeature`
currently skips it, and `OsmField::AppendLayer` therefore publishes no source
identity. In 25 cached VersaTiles z14 Hockenheim tiles, all 1,162 street
features carry an ID; 98 IDs repeat across tiles. These are provider feature
IDs, not yet proven to be original OSM way IDs. Do not derive OSM meaning from
their numeric pattern or substitute tile coordinates/geometry hashes.

`OsmVector::Feature` and `OsmField::Feature` carry an optional `SourceFeatureId`
through decode, staged tile publication and rebuilt snapshots. The optional
state distinguishes absent from the valid ID zero; preserve the full 64 bits.
Repeated IDs may identify fragments across tiles, but no consumer may merge on
ID alone: provider namespace, layer, revision and source topology still need
verification under 2173/2133. Directly declared features have no source ID.

## Implementation and rejection

- Own MVT wire validation in `src/world/ground/OsmVector.*`; propagate only
  native optional identity through `src/world/ground/OsmField.*`. Reject a
  duplicated or wrong-wire ID field as an ambiguous source identity. Keep
  unknown unrelated protobuf fields forward-compatible.
- Preserve the all-or-nothing tile publication contract. A malformed second
  tile must not alter the preceding snapshot; a corrected retry must publish
  its ID exactly once. Arrival order cannot change IDs or feature order.
- Test absent, zero, above-32-bit and maximum uint64 IDs, repeated IDs in
  adjacent tiles, malformed identity and corrected retry through both parser
  and field. Run focused suites, `make format`, `make lint`, and `make doc` if
  the public API changes. This slice does not claim a usable Hockenheim route.
