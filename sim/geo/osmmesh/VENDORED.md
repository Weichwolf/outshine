# osmmesh — vendored copy

Source: local `~/Git/wasm-osm/libosmmesh`, copied so FlightBox has **no external path
dependency**. Nothing is preloaded any more: every tile comes from the `fb-tiles` service at run
time, which is what the provider extension below exists for. The old preloaded-region machinery
(region PMTiles + the Planetiler/Copernicus build scripts) is gone.

## Local deltas vs upstream

Keep this list honest — an undocumented change here is invisible until an upstream sync silently
reverts it.

### 1. Per-tile byte provider (`osmmesh_config.tile_provider`)

**Why:** upstream can only open a whole PMTiles archive up front, so the aircraft could only ever
fly over one preloaded region. FlightBox needs tiles fetched on demand, from anywhere on earth.
Upstream's own header notes an HTTP backend was planned ("ships in T10") but it never landed, and
the one pluggable seam it does have (`osmmesh_pmtiles_io`) is *byte-offset* based — it can range-read
one remote archive, not fetch individual z/x/y tiles from a tile server.

**What changed:**
- `include/osmmesh/osmmesh.h`: added `osmmesh_tile_kind`, `osmmesh_tile_provider`, and the
  `tile_provider` / `tile_provider_user` + `provider_terrain_max_zoom` fields on `osmmesh_config`.
- `src/osmmesh.c`, `osmmesh_create`: a provider now satisfies the "need a vector source" check,
  and no archive is opened at all when one is set.
- `src/osmmesh.c`, terrain path: `!ctx->ter_pm` used to mean "no terrain", which silently
  skipped terrain on the provider path (`fetch_tile` returned OK with `terrain=NULL`). It now
  also honours a provider. The terrain max zoom came from the PMTiles **header** — a tile server
  has none, so it comes from `provider_terrain_max_zoom` instead (Tilezen: 15).
- `src/osmmesh.c`: added `om_tile_bytes()` — one seam that calls the provider when set and
  otherwise falls through to `osmmesh_pmtiles_fetch` exactly as before. The three hardcoded
  archive reads (terrain grid; neighbour vector tile for seam stitching; primary vector tile) now
  go through it. Added the two fields to `struct osmmesh_ctx`, copied in `osmmesh_create`.

**Why it's safe:** `osmmesh_pmtiles_fetch` already *allocates* and every call site already frees
with `osmmesh_pmtiles_free_tile` (plain `free()`), so a provider handing over a malloc'd buffer
needs no change at the call sites and leaks nothing. With `tile_provider == NULL` the behaviour is
the old archive path, unchanged.

**Contract the host must honour:** the provider MUST NOT BLOCK. On WASM, HTTP is asynchronous, so
the host answers from its own cache and returns 0 ("no tile yet") until the bytes arrive; osmmesh
already treats a missing tile as a hole and carries on, and the renderer retries.

**Worth upstreaming** — a small, general capability, not a FlightBox hack.

### 2. stb_image: JPEG decoder re-enabled

**Why:** `src/terrain.c` builds the only stb_image instance we ship, and upstream compiles it with
`STBI_NO_JPEG` (it only ever needed Terrarium PNG). The command center now decodes **Esri World
Imagery**, which is JPEG, for the TAB aerial-photo ground. A second stb_image implementation in the
same link would collide on every symbol, so the renderer shares this one.

**What changed:** one line — `#define STBI_NO_JPEG` removed from `src/terrain.c`.

**Cost:** the JPEG decoder adds to every binary that links osmmesh, including `fb-tiles`, which
does not need it. Measured, not guessed — see the commit. Cheap enough that a second decoder or a
second stb TU would be the worse trade.

**Why it's safe:** purely additive. `stbi_load_from_memory` gains a format; the PNG path,
`osmmesh_terrain_decode_png` and every existing caller are untouched.

### 3. stb_image_write.h added (3rdparty)

**Why:** `fb-tiles` now bakes ground albedo textures and stores them on disk, so it needs to
*encode* images, not just decode them. Upstream osmmesh only ever decoded (`stb_image.h`), so there
was no encoder anywhere in the tree.

**What:** `src/3rdparty/stb_image_write.h` v1.16, public domain / MIT — same author and repository
as the `stb_image.h` already vendored here. PNG for the OSM cartography (flat colours, compresses
hard) and JPEG for the aerial mosaic (a photo as PNG is ~2 MB — bigger than the 16 source JPEGs it
replaces, which would make the "cache" a pessimisation).

**Cost:** brings its own deflate, so no zlib and no new package in any container image. Nothing
links it except `tiles/`.

**Why it's safe:** a header-only addition. It defines no symbol `stb_image.h` uses, and nothing
outside `tiles/` includes it.

## Notes carried from upstream

- The MVT and terrain decoders take **raw bytes** and are independent of PMTiles:
  `osmmesh_mvt_decode` (uncompressed Shortbread `.pbf`) and `osmmesh_terrain_decode_png`
  (Terrarium PNG — `h = R*256 + G + B/256 − 32768`; **not** Mapbox terrain-RGB).
- Neither decoder gunzips. Our tile service fetches with `curl --compressed`, so what it caches
  and serves is already raw — verified: 12 Shortbread layers decode straight from the cache.
- The archive path (`vector_url` / `vector_data`) still works and is untouched; we simply no
  longer use it.
- `osmmesh_fetch_tile(z,x,y)` internally also reads the **west/north neighbour** tiles for seam
  stitching, so a provider must be able to serve adjacent tiles, not just the requested one.
