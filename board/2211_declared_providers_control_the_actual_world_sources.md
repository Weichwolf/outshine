Type: defect
State: active
Architecture: ready
Parent: 2131
Depends:
Priority: P0
Area: engine, world, scenario
Tags: architecture, providers, streaming, offline

# Declared providers control the actual world sources

## Current defect and evidence

Terrain-only snapshot and ground-only draw were repaired by `2a60e9f09` and
`03ce86e6a`. Public offline Hockenheim lap at 91.7 s now captures with
16/16 terrain tiles, zero vector requests, zero network starts and a valid PNG.
This does not close source selection: `RegisterDeclared` ignores provider input
identity for terrain/vector. Both classes use fixed URLs. Distinct declarations
with equal pin can therefore hit the same endpoint and cache key. The existing
provider test proves metadata ordering, not distinct fetched bytes.

## Contract and ownership

- `SourceProvider` owns an optional HTTPS `endpoint` template for terrain/vector
  tiles. It requires a stable `dataset` ID and exactly one each of `{z}`, `{x}`
  and `{y}`. Empty endpoint chooses the shipped Terrarium/Versatiles source.
  OSM uses its pinned local `location`; stars keep their shipped local source.
- Scenario reader/writer and public validation preserve and reject this contract.
  Invalid schemes, placeholders and attributes fail before source registration.
  Validate again at the API boundary; parser validation alone is insufficient.
- The data source owns URL expansion. Declared dataset, endpoint and pin reach
  the transport and cache identity. Changing an endpoint or dataset cannot reuse
  bytes from another source even when the declared pin is unchanged. Built-in
  source cache keys stay compatible with existing content.
- `SourceSet` retains per-kind rank, absence policy, retry and offline semantics.
  A cache miss in offline mode reports source identity without network access.
  Source revision and identity are visible in delivery/render diagnostics.
- GroundStack/ClassField own the absence-of-vector path. No vector provider
  means an empty immutable feature snapshot, no vector IO and no null GPU
  placement binding. No fabricated elevation or general readiness bypass.

## Falsifiable acceptance

- Two providers with different endpoint/dataset declarations reach different
  transport URLs. Rank chooses first, `Continue` hands over on absent, `Fail`
  terminates. Same pin but changed endpoint/dataset yields a distinct cache key.
- Complete offline cache reproduces returned bytes with zero network starts;
  a miss reports the selected source ID and does not invent elevation.
- Public Hockenheim pinned-OSM plus explicit terrain-only offline capture at
  91.7 s has `ground_arrived=ground_wanted>0`, `vector_wanted=vector_arrived=0`,
  `remote_starts=0`; empty DEM cache fails explicitly. Shipped/default vector
  source continues to request tiles. Inspect both PNGs.
- `make format`, focused provider/scenario/client tests and `make lint` pass.
