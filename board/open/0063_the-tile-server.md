Type: feature
Area: world
Tags: instrument

**I.16 The tile server**

- [x] `fb-tiles` serving DEM, OSM vectors, imagery, weather and stars over HTTP, and nothing else
- [ ] A **collector** for the state channel, so a run is reconstructible from something other than the process's own stdout — `b83285f` deleted `SimHost.cpp`, the only implementation of `OUTSHINE_SIM`, and `web/` with it. The emitting half survives and still points at `http://localhost:8080` (the deleted server log, the deleted server telemetry, the deleted HTTP client, the deleted walk client), so every run now posts into nothing. Owed with it: `ServerLog`'s own promise that a refused POST is **visible rather than silent** — measured with :8080 closed, a run exits 0 and not one of its 674 log lines names a refusal (the bug tasks in `board/`)
- [x] Terrarium DEM tiles served, with **one** decoder in the tree — `src/world/tiles/TerrainGrid.cpp` `FromTerrariumPng`, over SDL3_image. *The line said `shared … by both the client and `tiles/`` and that was the defect rather than the requirement: the sharing was two copies (the bug tasks in `board/`). `tiles/` decodes no DEM at all — it passes upstream bytes through (the deleted tile server) — so there is one decoder because there is one consumer.*
- [x] Shortbread vector tiles from tiles.versatiles.org
- [x] Aerial imagery tiles served (the deleted tile server, Esri World Imagery)
- [ ] Imagery consumed by the engine — served and cached, nothing reads it. Measured 2026-08-12 on the running container: 68 316 imagery tiles, 1.1 GB, whose only consumer in the tree is the photo bake (the deleted tile server), which itself has no consumer (the bug tasks in `board/`)
- [x] GRIB2 weather ingest (the deleted GRIB2 decoder)
- [x] Star catalogue served
- [x] Peaks endpoint
- [ ] `pois` layer fetched — five layers are fetched today; POIs carry amenity, shop, tourism, man_made, name and housenumber and nothing uses them
- [ ] `addresses` and label layers fetched
- [ ] `boundaries` layer fetched
- [ ] Zoom above 14 for terrain — `/t/terrain/15/…` returns non-PNG, so z14 may be the finest served; unresolved
