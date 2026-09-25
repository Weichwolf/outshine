Type: defect
State: active
Architecture: ready
Parent: 2224
Depends:
Priority: P0
Area: world, engine, streaming, simulation
Tags: ownership, osm, publication, hockenheim

# Published region retains semantic inputs until atomic replacement

## Problem and evidence

`GroundStack::Restand` resets `Footprints_`, `Ways_` and `WaterBodies_` when its
mutable vector generation changes. `GroundWorldCandidate` retains only a
footprint snapshot and road graph; other CPU readers still borrow `Stack`.
Thus a pending or rejected candidate can leave the rendered old world paired
with cleared or new semantic data. Hockenheim's candidate 3 was canceled by a
region change; candidate 5 by class, footprint and vector revisions together.
Ignoring those revisions would publish stale data. The unused unsigned
`Cost.StreamedTiles` subtraction across the reset has been removed.

`OsmField::SnapshotQueries` pins native query arrays without parser/TilePool
state; a source-generation and source-destruction test passes. On the pinned
Hockenheim source (49 tiles, 45,297 features), the snapshot holds 10.68 MB
versus 18.52 MB in the mutable field; one measured copy took 0.418 ms.
It is not yet wired into candidate or published ownership.

Public route lookup now uses `GroundPublished` and its atomic road-alignment
product, not the mutable transport loader's current source. A new declaration
that resets publication still rejects the old route; the Hockenheim public API
route/contact fixture passes. The remaining vector, water, way and footprint
reads still need the region owner below.

`Ground::PublishedRegion` now holds the candidate's pinned vector/way/water
sources and its final accepted footprint snapshot. `Surrounds` swaps this
native owner with the ground products after renderer publication; generator
feature reads use the published owner once ground is published, and staging
only before the first publication. Water's query copy excludes in-flight
candidates. Retained candidate and published-region bytes are measured.
An empty replacement tile and accepted footprint source retain independent
query identities after the ingest owner changes and dies. Live structure-tile
updates and the remaining direct `Stack` readers still need migration.
The injected late-GPU-failure fixture retains the old semantic owner; retry
swaps it with the native scene. Hockenheim Playable still at 74.85 s is
pixel-identical to the prior snapshot build; its PNG was opened. The
4,491-frame offline camera run has zero missing contacts, 538 cache deliveries
and no provider starts. An earlier 4.125% image difference compared that
Refined motion frame to a Playable still. Explicit Refined still/motion now
differ in 130/921,600 pixels, with 120 above 1/255 (WI 2295).

## Ownership and data flow

`GroundStack` owns mutable provider, parse and ingest state. Add a native
`PublishedRegion` owner under `src/world/ground/` for immutable vector
features/points/tags, ways, water and accepted footprints with source IDs and
generation. A published snapshot contains only queryable native data, never
parser scratch, network handles or `TilePool`. Format-specific MVT state ends
before this boundary. `Surrounds` owns the current published region; all
simulation, navigation, placement and public queries that claim published
world state read it. Generators preparing the successor read a pinned candidate
snapshot. Diagnostics name which owner they report.

`GroundWorldCandidate` pins one source generation and prepares its CPU products,
road graph, geometry and GPU resources without mutating the published owner.
Validate the pinned source identities, projection and requested coverage just
before commit. On success swap the region, graph, ground, materials and render
owner in one Engine-thread publication. A late failure or stale worker result
releases candidate resources and leaves every old published query and pixel
unchanged. No rollback by copying a partially mutated world.

Independent live structure-tile publication follows the same rule: prepare a
new tile record and both wall/roof handles, validate the source key, then swap
that tile's semantic and render records together. Other published tiles retain
their IDs, handles and queries. A later whole-region candidate reuses unchanged
tile products by stable spatial/source identity, not the mutable `OsmField`
array ordinal. Resource fences govern old GPU-handle retirement. Published
source records are immutable to readers; ownership and mutation stay on the
Engine thread. Bound retained generations and bytes during camera movement.

## Implementation sequence

1. `GroundStack`, `OsmField`: add an immutable native snapshot of query data
   with stable tile identities. Retain the old published snapshot while new
   vectors parse. Cover tags, feature-point indices, empty tiles and source
   identities; exclude parser caches. Measure copy and retained bytes.
2. `Surrounds`, `GroundWorldCandidate`, `WorldPlacement` and public query paths:
   route published reads to the owner; candidate generators use only their
   pinned snapshot. Commit the complete region after fallible GPU work. Keep
   `GroundStack` access explicitly for ingest and readiness only.
3. `StructureTilePublication`, `BuildingField`, `TilePieces`: publish one tile
   atomically with stable spatial/source identity. Reject late A after B; a
   camera-only LOD change must not revise semantic source identity.
4. Reuse unchanged tiles and derived products when region or source coverage
   changes. Rebuild only products whose pinned inputs differ; source changes
   may still invalidate affected cells. Preserve cross-tile road/water joins.

## Falsifiable acceptance

- Public API fixture: publish A, start B with new vectors, fail B after CPU and
  GPU preparation. A's feature/tag query, footprint, logical route, water,
  contact, audio occlusion and pixels remain identical. Retry B succeeds once.
  A deliberately redirected read to staging must fail this fixture.
- Shift the vector ring with identical overlapping source tiles, then change
  one tile and deliver its old task late. Unchanged tiles retain stable IDs and
  handles; changed tile alone swaps. No roof/wall mismatch, hole, duplicate or
  stale query. Empty tile and absent-source controls are included.
- Warm/offline Hockenheim drive crosses the measured region boundary and the
  class/footprint/vector update. Record candidate cancellations, reused and
  rebuilt tile counts, contact gaps, p50/p95/p99 and peak CPU/GPU bytes. Open
  the transition PNGs; no black wedge or whole-region silhouette pop.
- `make format`, focused public API and source/publication suites, then
  `LINT_JOBS=2 make lint`; run the route through `outshine-client run`.
