# osmmesh — vendored copy

Source: local `~/Git/wasm-osm/libosmmesh`, copied so FlightBox has **no external path
dependency**. `../data/*.pmtiles` (gitignored, obtained by `../fetch-data.sh`) are the legacy
prebuilt Hameln tiles; they go away once the renderer sources everything from `fb-tiles`.

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
  `tile_provider` / `tile_provider_user` fields on `osmmesh_config`.
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

## Notes carried from upstream

- The MVT and terrain decoders take **raw bytes** and are independent of PMTiles:
  `osmmesh_mvt_decode` (uncompressed Shortbread `.pbf`) and `osmmesh_terrain_decode_png`
  (Terrarium PNG — `h = R*256 + G + B/256 − 32768`; **not** Mapbox terrain-RGB).
- Neither decoder gunzips, and the prebuilt PMTiles are therefore stored uncompressed
  (`--tile_compression=none`). Our tile service fetches with `curl --compressed`, so what it
  caches and serves is already raw — verified: 12 Shortbread layers decode straight from the cache.
- `osmmesh_fetch_tile(z,x,y)` internally also reads the **west/north neighbour** tiles for seam
  stitching, so a provider must be able to serve adjacent tiles, not just the requested one.
