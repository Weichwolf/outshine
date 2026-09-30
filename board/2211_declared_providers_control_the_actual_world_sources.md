Type: defect
State: done
Architecture: ready
Parent: 2131
Depends:
Priority: P0
Area: engine, world, scenario
Tags: architecture, providers, streaming, offline

# Declared providers control the actual world sources

## Result and evidence

Terrain-only snapshot and ground-only draw were repaired by `2a60e9f09` and
`03ce86e6a`. `b7ec9a19a` carries declared tile endpoints/datasets through
validation, transport, fallback and cache identity; focused tests and both
offline Hockenheim PNGs pass. `5d692d8de` adds source/pin/tile to refusal
logs; an isolated empty offline DEM cache fails with zero remote starts and no
PNG. `58d76edee` counts actual source deliveries by kind, ID, revision and
priority; `--stats` also prints an endpoint-sensitive source key. SourceSet
tests cover rank, handover, fail, cache hit, changed endpoint and retry.
Default Hockenheim reports DEM plus vector; terrain-only reports DEM only.
Both captures were visually inspected and each matches its previous PNG
pixel-for-pixel (0/921600 changed). `make lint` passes.

This WI proves provider selection and attribution. A successful online
Hockenheim capture did **not** close its offline route-data demand: an
immediate offline replay missed 46 tiles and failed. WI 2306 owns that P0
streaming/cache-closure defect; do not infer offline replay from this result.

## Contract and ownership

- `SourceProvider` owns an optional HTTPS `endpoint` template for terrain/vector
  tiles. It requires a stable `dataset` ID and exactly one each of `{z}`, `{x}`
  and `{y}`. Empty terrain endpoint historically selected Terrarium. WI 2330 removes
  the implicit vector source and rejects reduced map tiles at the client boundary.
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

## Verified provider acceptance

- `--stats` identifies each DEM/vector provider actually used, with revision,
  endpoint-sensitive key and delivery count. Mixed-source fallback reports
  both; an unused declaration reports none. Memory is bounded by registrations.
- Changing endpoint under the same dataset/pin misses previous cached bytes
  and changes the diagnostic key. Empty offline cache refuses with exact
  source/revision/tile and zero network starts.
- SourceSet and TilePool tests prove attribution across retry/fallback/cache.
  Public client captures, pixel comparison, `make format` and `make lint` pass.
