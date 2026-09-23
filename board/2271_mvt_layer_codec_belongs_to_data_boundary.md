Type: debt
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: world, data, ground
Tags: naming, ownership, vector, osm

# MVT layer codec lives at the data boundary

## Evidence and decision

`src/world/ground/OsmVector.*` reads Mapbox Vector Tile protobuf fields,
commands, tags and a named layer. It does not read OSM nodes, ways or relations.
The class name falsely suggests OSM semantics, and its codec belongs with
source-format adapters in `src/world/data/`. `OsmField` remains the native
published vector-feature store and must not inherit MVT wire concepts beyond
decoded geometry, tags and optional provider identity.

Move the codec to `src/world/data/MvtLayer.*` as `Data::MvtLayer`. Migrate
`OsmField` callers, test paths, fixture includes and the test-runner's focused
profile in one change. Remove the old class/files; no compatibility alias or
second decoder. Keep data -> base and ground -> data dependency direction.
Do not treat `ProviderFeatureId` as an OSM way ID or merge fragments by it.

## Acceptance

- Existing MVT wire and `OsmField` publication suites pass unchanged in
  meaning, including absent/zero/full-width IDs and malformed-field rollback.
- A missing layer, malformed tile and unsupported version retain distinct
  error results. `OsmField` snapshot and arrival-order behavior stay stable.
- `make format`, focused suites and `make lint` pass; no `OsmVector` symbol or
  production/test path remains. This move supplies no raceway semantics yet.
