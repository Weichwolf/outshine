/* Command center — tile bytes from the fb-tiles world-data service.
 *
 * Bridges an async world to a synchronous one. osmmesh asks for a tile's bytes and expects an
 * answer NOW; in the browser, HTTP only ever answers later. So:
 *
 *   - a JS-side cache holds bytes per (kind,z,x,y), plus a "pending" marker
 *   - the provider callback checks the cache. Hit -> hand over a copy. Miss -> kick off the
 *     fetch and answer "no tile yet" (0). osmmesh treats that as a hole and carries on.
 *   - the tile streamer retries on later frames, so tiles simply appear as they land. Nothing
 *     ever blocks the frame loop — which also means no "the world froze for 300 ms" hitch.
 *
 * This is what replaces the preloaded region: with it, any origin on earth works.
 */
#ifndef W3_TILESRC_JS_H
#define W3_TILESRC_JS_H
#include <osmmesh/osmmesh.h>
#include <stdlib.h>

/* Aerial photo tiles. osmmesh does not know about these -- it only needs vector + terrain to
 * build a mesh. The imagery is purely the renderer's business (it is only an albedo), so it
 * rides the same byte cache under its own kind rather than becoming an osmmesh_tile_kind. */
#define W3_TILE_IMAGERY 2

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/* Base URL of fb-tiles, e.g. "http://localhost:8081". Set once at start-up. */
EM_JS(void, w3_tiles_init, (const char *base), {
    Module.__fbTiles = { base: UTF8ToString(base), cache: new Map(), inflight: 0 };
})

/* Byte length of a cached tile: >=0 if resident (0 = known hole), -1 if not fetched yet.
 * Starts the fetch on a miss. Never blocks. */
EM_JS(int, w3_tiles_size, (int kind, int z, int x, int y), {
    var T = Module.__fbTiles; if (!T) return -1;
    var names = ['vector', 'terrain', 'imagery'];      /* 0,1 = osmmesh_tile_kind; 2 = W3_TILE_IMAGERY */
    if (kind < 0 || kind >= names.length) return 0;    /* unknown kind = a hole, not a wrong tile */
    var key = kind + '/' + z + '/' + x + '/' + y;
    var e = T.cache.get(key);
    if (e === undefined) {
        /* Cap concurrency, but not so tightly that the centre tile starves. Building ONE tile in
         * OSM mode is ~6 requests (vector + terrain + the 4 edge neighbours osmmesh pulls for seam
         * stitching); in photo mode it is ~22, because the albedo is 16 z16 children on top. A cap
         * of 48 was under three tiles' worth and made the ground switch crawl. The browser only
         * opens ~6 sockets per host anyway, so a high cap here just lets them queue in OUR order --
         * which is nearest-first (see w3_stream_grid). */
        if (T.inflight > 256) return -1;
        T.cache.set(key, null);                        /* null = pending */
        T.inflight++;
        fetch(T.base + '/t/' + names[kind] + '/' + z + '/' + x + '/' + y)
            .then(function (r) { return r.ok ? r.arrayBuffer() : null; })
            .then(function (b) {
                T.cache.set(key, b ? new Uint8Array(b) : new Uint8Array(0));  /* empty = hole */
                T.inflight--;
            })
            .catch(function () { T.cache.delete(key); T.inflight--; });       /* allow a retry */
        return -1;
    }
    if (e === null) return -1;                          /* still in flight */
    return e.length;
})

/* Copy a resident tile into the WASM heap. Caller must have checked w3_tiles_size first. */
EM_JS(void, w3_tiles_copy, (int kind, int z, int x, int y, uint8_t *dst), {
    var T = Module.__fbTiles; if (!T) return;
    var e = T.cache.get(kind + '/' + z + '/' + x + '/' + y);
    if (e && e.length) HEAPU8.set(e, dst);
})

/* How many tiles are resident / in flight — for the progress line and for tests. */
EM_JS(int, w3_tiles_resident, (void), {
    var T = Module.__fbTiles; if (!T) return 0;
    var n = 0; T.cache.forEach(function (v) { if (v) n++; }); return n;
})

#else  /* ---- native build ----
 * Same provider contract, but fetched synchronously with curl: natively we MAY block, and the
 * headless renderer is the only way to prove the streaming path (provider -> osmmesh -> mesh ->
 * pixels) without a browser. The browser's async cache is the only part this cannot exercise. */
#include <stdio.h>
#include <string.h>

#define W3_NT_CACHE 4096   /* imagery: 16 z16 children per z14 tile, x50 tiles */
static struct { int kind, z, x, y; uint8_t *b; int n; } w3_nt[W3_NT_CACHE];
static int  w3_nt_n = 0;
static char w3_nt_base[160] = "";

static void w3_tiles_init(const char *base) { snprintf(w3_nt_base, sizeof w3_nt_base, "%s", base ? base : ""); }

static int w3_nt_find(int kind, int z, int x, int y) {
    for (int i = 0; i < w3_nt_n; i++)
        if (w3_nt[i].kind == kind && w3_nt[i].z == z && w3_nt[i].x == x && w3_nt[i].y == y) return i;
    return -1;
}
static int w3_tiles_size(int kind, int z, int x, int y) {
    int i = w3_nt_find(kind, z, x, y);
    if (i >= 0) return w3_nt[i].n;
    if (!w3_nt_base[0] || w3_nt_n >= W3_NT_CACHE) return -1;
    /* 0,1 = osmmesh_tile_kind; 2 = W3_TILE_IMAGERY. This used to index with names[kind & 1],
     * which masks kind 2 down to 0 -- the headless renderer would then have silently fetched
     * VECTOR tiles and called them aerial photos. A wrong tile is worse than no tile. */
    static const char *names[] = { "vector", "terrain", "imagery" };
    if (kind < 0 || kind >= (int)(sizeof names / sizeof names[0])) return 0;
    char cmd[512];
    snprintf(cmd, sizeof cmd, "curl -s -f --max-time 20 '%s/t/%s/%d/%d/%d'",
             w3_nt_base, names[kind], z, x, y);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    size_t cap = 1 << 16, n = 0; uint8_t *b = (uint8_t *)malloc(cap);
    for (;;) {
        if (n == cap) { cap *= 2; uint8_t *t = (uint8_t *)realloc(b, cap); if (!t) break; b = t; }
        size_t r = fread(b + n, 1, cap - n, f);
        if (!r) break;
        n += r;
    }
    pclose(f);
    w3_nt[w3_nt_n].kind = kind; w3_nt[w3_nt_n].z = z; w3_nt[w3_nt_n].x = x; w3_nt[w3_nt_n].y = y;
    w3_nt[w3_nt_n].b = b; w3_nt[w3_nt_n].n = (int)n;        /* n == 0 records a hole, so we ask once */
    return w3_nt[w3_nt_n++].n;
}
static void w3_tiles_copy(int kind, int z, int x, int y, uint8_t *dst) {
    int i = w3_nt_find(kind, z, x, y);
    if (i >= 0 && w3_nt[i].n > 0) memcpy(dst, w3_nt[i].b, (size_t)w3_nt[i].n);
}
static int w3_tiles_resident(void) { return w3_nt_n; }
#endif

/* ---- baked ground albedo -------------------------------------------------------------------
 * fb-tiles rasterises the OSM cartography and stitches the aerial mosaic, then keeps the result
 * on disk. So the renderer downloads ONE finished image per tile per albedo instead of a vector
 * tile plus 4 seam neighbours (OSM) or 16 z16 children (photo) — and rasterises nothing.
 *
 * Same async contract as the raw tiles: -1 means "not here yet, a fetch is now running", and the
 * caller retries on a later frame. Never blocks.
 */
#ifdef __EMSCRIPTEN__
EM_JS(int, w3_bake_size, (int photo, int z, int x, int y, int tex), {
    var T = Module.__fbTiles; if (!T) return -1;
    var key = 'b' + photo + '/' + tex + '/' + z + '/' + x + '/' + y;
    var e = T.cache.get(key);
    if (e === undefined) {
        if (T.inflight > 256) return -1;
        T.cache.set(key, null);
        T.inflight++;
        fetch(T.base + '/bake/' + (photo ? 'photo' : 'osm') + '/' + z + '/' + x + '/' + y + '?tex=' + tex)
            .then(function (r) { return r.ok ? r.arrayBuffer() : null; })
            .then(function (b) { T.cache.set(key, b ? new Uint8Array(b) : new Uint8Array(0)); T.inflight--; })
            .catch(function () { T.cache.delete(key); T.inflight--; });
        return -1;
    }
    if (e === null) return -1;
    return e.length;
})
EM_JS(void, w3_bake_copy, (int photo, int z, int x, int y, int tex, uint8_t *dst), {
    var T = Module.__fbTiles; if (!T) return;
    var e = T.cache.get('b' + photo + '/' + tex + '/' + z + '/' + x + '/' + y);
    if (e && e.length) HEAPU8.set(e, dst);
})
#else
static int w3_bake_size(int photo, int z, int x, int y, int tex){
    int i = w3_nt_find(100 + photo, z, x, y);          /* 100+ = a kind space of our own */
    if (i >= 0) return w3_nt[i].n;
    if (!w3_nt_base[0] || w3_nt_n >= W3_NT_CACHE) return -1;
    char cmd[512];
    snprintf(cmd, sizeof cmd, "curl -s -f --max-time 60 '%s/bake/%s/%d/%d/%d?tex=%d'",
             w3_nt_base, photo ? "photo" : "osm", z, x, y, tex);
    FILE *f = popen(cmd, "r");
    if (!f) return -1;
    size_t cap = 1 << 16, n = 0; uint8_t *b = (uint8_t *)malloc(cap);
    for (;;) {
        if (n == cap) { cap *= 2; uint8_t *t = (uint8_t *)realloc(b, cap); if (!t) break; b = t; }
        size_t r = fread(b + n, 1, cap - n, f);
        if (!r) break;
        n += r;
    }
    pclose(f);
    w3_nt[w3_nt_n].kind = 100 + photo; w3_nt[w3_nt_n].z = z; w3_nt[w3_nt_n].x = x; w3_nt[w3_nt_n].y = y;
    w3_nt[w3_nt_n].b = b; w3_nt[w3_nt_n].n = (int)n;
    return w3_nt[w3_nt_n++].n;
}
static void w3_bake_copy(int photo, int z, int x, int y, int tex, uint8_t *dst){
    (void)tex;
    int i = w3_nt_find(100 + photo, z, x, y);
    if (i >= 0 && w3_nt[i].n > 0) memcpy(dst, w3_nt[i].b, (size_t)w3_nt[i].n);
}
#endif

/* The osmmesh provider. Contract: hand over a malloc'd buffer (osmmesh frees it), or return 0
 * for "no tile" — which covers both a genuine hole and "not fetched yet". */
static int w3_tile_provider(void *user, osmmesh_tile_kind kind,
                            uint32_t z, uint32_t x, uint32_t y,
                            uint8_t **out, size_t *len) {
    (void)user;
    int n = w3_tiles_size((int)kind, (int)z, (int)x, (int)y);
    if (n <= 0) return 0;                    /* pending (-1) or a known hole (0) */
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) return 0;
    w3_tiles_copy((int)kind, (int)z, (int)x, (int)y, b);
    *out = b; *len = (size_t)n;
    return 1;
}

#endif /* W3_TILESRC_JS_H */
