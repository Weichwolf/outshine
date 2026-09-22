Type: defect
State: proposed
Architecture: ready
Parent: 2230
Depends:
Priority: P1
Area: data, ground, import
Tags: provenance, streaming, determinism

# Resident tiles keep source identity and revision

## Defect

`SourceDecl::Revision` reaches `Delivery::Answer` and `TilePool::Landing`.
`ContentStore` includes it in the cache key. The resident paths discard it:
`OsmField::FetchTile` moves only decoded layers into `ParsedTile`; both
`PoolTerrain::Answered` and `GroundStream::Held::Oracle::Take` pass only bytes
and tile address to `TerrainBytes`. A structure bake therefore cannot identify
the exact DEM/vector source revision it consumed. Runtime handles or arrival
order are not source identities.

## Decision

Define one value-owned `TileSourceIdentity` at the data boundary: provider ID,
declared revision, kind and canonical tile address. Carry it with decoded
resident vector and elevation tiles, including cache hits and generated/declared
fixtures. Empty revision is an explicit value, never an implicit wildcard.
Do not store a view into `TilePool::Landing` or mutable cache memory. Preserve
the existing payload and cache identity; do not derive identity from bytes.

Expose a stable, sorted identity set for the exact source tiles used by a
ground/structure candidate. Record the identity at consumption, not by later
enumerating all currently resident tiles. A monotonically increasing product
generation distinguishes rebakes from the same source identity. Keep format
metadata outside the native geometry model.

## Implementation and acceptance

1. Thread identity through `TerrainBytes`, the terrain decoder/resident slots,
   `OsmField::ParsedTile` and published vector tiles. Preserve it through
   replacement, eviction and cache reload. Keep allocations out of lookup and
   frame hot paths; account for owned storage.
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
