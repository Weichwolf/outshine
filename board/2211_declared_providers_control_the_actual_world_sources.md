Type: defect
State: active
Architecture: ready
Parent: 2131
Depends:
Priority: P0
Area: engine, world, scenario
Tags: architecture, providers, streaming, offline

# Declared providers control the actual world sources

## Current defect

Declared terrain/vector providers already reach `SourceSet`; an empty tile-provider
list selects the shipped defaults. Offline uses the persistent source cache and
never starts network transport. Pin, rank and absence policy are parsed and
validated. Concrete endpoint choice and source diagnostics remain open.

An explicit terrain provider plus pinned OSM route, with no vector provider,
currently times out after 30 s: `vectors=absent`, `generator snapshot pending`.
`GroundStack::Restand` returns before creating `OsmField` when vector zoom is
zero. `Engine::Readiness` and world snapshot publication nevertheless require
a vector snapshot. This forbids a valid terrain-only world and makes absence of
a provider behave like missing data from a required provider.

## Contract and ownership

`GroundStack` owns vector-source capability. When no vector source exists,
publish an empty/declared-feature `OsmField` at the ground classification's
fine zoom for each focus tile; call `Declare`, never `Build` or vector IO.
Keep its generation and source identity stable on unchanged focus and advance
on a real focus/declaration change. The existing world/generator consumers
receive a valid immutable empty feature snapshot. `GroundStack::Close` retires
it. `Engine::loading()` reports zero vector source requests/arrivals; a declared
feature snapshot does not counterfeit a fetched vector tile. Footprint tile
span uses that same effective zoom. A declared vector source retains normal
fetch, parse, ingestion and missing-data behavior. Terrain/OSM failures still
block; no unconditional `settled()` bypass.

## Falsifiable acceptance

- GroundStack test: explicit terrain-only source creates one settled empty
  vector snapshot, no vector provider start; repeated focus keeps generation,
  moved focus updates it. Declared vector features remain available.
- Public `outshine-client run --offline --stats --view lap --at-seconds 91.7`
  with pinned Hockenheim OSM plus explicit terrain-only provider captures a
  PNG from the same route/DEM; `vector_wanted=vector_arrived=0`. Empty offline
  terrain cache still fails explicitly, never invents elevation.
- Shipped/default vector provider still requests MVT tiles and retains normal
  PNG/counters. Compare terrain-only and vector-enabled PNGs at the same view;
  this isolates source overlap without changing the road generator.
- `make format`, focused GroundStack/client cases and `make lint` pass.

## Remaining provider work

Two declared providers with distinct inputs must prove rank, policy, pin and
endpoint selection without rebuild. A complete offline cache must reproduce
its output; misses must report source ID with zero network calls. Record source
revision in render diagnostics. Do not treat the terrain-only fix as closure
of the full provider WI.
