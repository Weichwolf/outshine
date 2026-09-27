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
`03ce86e6a`. `b7ec9a19a` carries declared tile endpoints/datasets through
validation, transport, fallback and cache identity; focused tests and both
offline Hockenheim PNGs pass. The remaining gap is diagnostic: delivery holds
source ID/revision, but a failed tile request still loses them at `TilePool` and
the public client has only aggregate counters. A public empty-DEM-cache capture
also needs an explicit failure assertion, distinct from the SourceSet unit case.

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

- A public capture using an empty isolated DEM cache and `--offline` fails with
  provider ID, revision and tile address in diagnostics, zero network starts and
  no PNG. A complete offline cache reproduces the normal PNG.
- `outshine-client --stats` identifies the selected DEM/vector source and
  revision for a successful capture, without dumping every tile into logs.
- SourceSet, TilePool and client failure tests prove the same attribution across
  retries and fallback. `make format`, focused tests and `make lint` pass.
