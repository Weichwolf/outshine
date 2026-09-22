Type: defect
State: active
Architecture: ready
Parent: 2230
Depends:
Priority: P1
Area: data, ground, import
Tags: provenance, streaming, determinism

# Resident tiles keep source identity and revision

## Defect

`SourceDecl::Revision` reaches `Delivery::Answer` and `TilePool::Landing`.
`ContentStore` includes it in the cache key. Native OSM tiles now retain this
identity (3cfac6f2d). Both `PoolTerrain::Answered` and
`GroundStream::Held::Oracle::Take` still pass only bytes and tile address to
`TerrainBytes`; `TerrainField`, `DecodedCache` and stitched fields have no
provenance. A structure bake cannot yet identify its exact DEM source set.
Runtime handles or arrival order are not source identities.

## Decision

Define one value-owned `TileSourceIdentity` at the data boundary: provider ID,
declared revision, kind and canonical tile address. Carry it with decoded
resident vector and elevation tiles, including cache hits and generated/declared
fixtures. Empty revision is an explicit value, never an implicit wildcard.
Do not store a view into `TilePool::Landing` or mutable cache memory. Preserve
payload semantics; do not derive identity from bytes.

Expose a stable, sorted identity set for the exact source tiles used by a
ground/structure candidate. Record the identity at consumption, not by later
enumerating all currently resident tiles. A monotonically increasing product
generation distinguishes rebakes from the same source identity. Keep format
metadata outside the native geometry model.

`TerrainTiles::StitchedGrid` can consume the centre, four edges and four
corners. Its identity is their sorted source set, not the centre's identity.
`DecodedCache`, stitched entries and TilePool's byte cache key by tile address.
Freeze provider declarations/revisions within one SourceSet generation; an
accepted registration change must invalidate or namespace all three caches
and stale in-flight work. Actual provider selection remains part of each raw
tile identity, including fallback/ancestor results.
Cache hits must return the same provenance as fresh decoding. Shaped terrain
uses an explicit identity from its declared parameters and seed.

## Implementation and acceptance

1. `TerrainBytes::Take` returns identity with bytes and address. Store the
   sorted source set in `TerrainField` so decoded-cache hits preserve it;
   stitch only identities of raw fields actually consulted. Carry the set to
   resident slots and structure height blocks. Preserve it through replacement,
   eviction and reload. Keep allocations out of lookup/frame hot paths and
   account for owned storage.
2. A candidate's height snapshot reports identities for every DEM block used
   by one structure tile. Pinned blocks remain valid until that bake completes;
   a later source revision creates a new input generation, not an in-place
   mutation of an accepted product.
3. Test two sources with equal bytes but different declared revisions, cache
   hit/miss, reverse arrival, missing source, eviction/reload and cancellation.
   Equal identities and bytes yield equal accepted input sets; changed revision
   invalidates the old product even when payload bytes match.
4. Use independent fixtures and public capture diagnostics. `make format`,
   relevant suites and `make lint` pass. WI 2247 consumes this contract.
