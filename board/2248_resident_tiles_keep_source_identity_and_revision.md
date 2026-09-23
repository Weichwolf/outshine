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
`ContentStore` includes it in the cache key. Native OSM tiles retain it
(3cfac6f2d); `TerrainBytes`, decoded fields and stitched fields retain their
sorted source sets (3b41a5d2e). Resident `GroundStream` slots, copied
`HeightField::Block`s and `TileBuild` now retain these source sets. Structure
candidate identity, source-generation invalidation and rebake policy remain open.
Runtime handles or arrival order are not source identities.
`StructureBuildQueue::Gathers` synthesizes a 17×17 fallback block through
`HeightSource = optional<double>(LongitudeLatitude)`; this callback carries no
identity. `TilePool` now seals `SourceSet` before starting workers; late
registration is rejected while the pool retains source pointers and caches.

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

Source registration is sealed before `TilePool` starts workers; late `AddAll`
is rejected atomically. Provider ID/revision declarations are construction-time
constants: use const owned declarations in WebTileSource/StarBands, which already
have no mutation API. A different revision uses new providers and a fresh TilePool;
do not add hot-reload/cache-generation infrastructure without a supported consumer.
Do not fabricate a DEM identity for scalar fallback samples: mark their blocks
unqualified, always replace their structure products when fine fields arrive,
and exclude them from fine-input equality checks.

The next vertical slice uses `HeightField` as the pinned input owner. It forms
one sorted unique union of block sources and marks the input qualified only
when every block has source identity and no scalar fallback was used.
`StructureBuildTask` lends this immutable summary until landing. In
`BuildingField`, `PendingAcceptance` copies the summary before publication;
the accepted tile keeps a value-owned source set and qualification flag in
tile order, including empty geometry. Reservation, rejection and cancellation
do not mutate the accepted record. This does not yet schedule replacements;
WI 2247 compares these records against fresh fine input.

## Implementation and acceptance

1. Resident slots, structure height blocks and `TerrainTiles::NodesOf` now
   carry the exact stitched source set. Verify replacement, eviction and reload
   without later resident-tile enumeration. Account for retained capacity.
2. A candidate's height snapshot reports identities for every DEM block used
   by one structure tile; scalar fallback blocks report unqualified input.
   Pinned blocks remain valid until that bake completes;
   a later source revision creates a new input generation, not an in-place
   mutation of an accepted product.
3. Test two sources with equal bytes but different declared revisions, cache
   hit/miss, reverse arrival, missing source, eviction/reload and cancellation.
   Equal identities and bytes yield equal accepted input sets; changed revision
   invalidates the old product even when payload bytes match.
4. Use independent fixtures and public capture diagnostics. `make format`,
   relevant suites and `make lint` pass. WI 2247 consumes this contract.
