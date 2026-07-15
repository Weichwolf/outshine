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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

/* Base URL of fb-tiles, e.g. "http://localhost:8081". Set once at start-up. */
EM_JS(void, w3_tiles_init, (const char *base), {
    Module.__fbTiles = { base: UTF8ToString(base), cache: new Map(), inflight: 0 };
});

/* Byte length of a cached tile: >=0 if resident (0 = known hole), -1 if not fetched yet.
 * Starts the fetch on a miss. Never blocks. */
EM_JS(int, w3_tiles_size, (int kind, int z, int x, int y), {
    var T = Module.__fbTiles; if (!T) return -1;
    var names = ['vector', 'terrain'];                 /* must match osmmesh_tile_kind */
    var key = kind + '/' + z + '/' + x + '/' + y;
    var e = T.cache.get(key);
    if (e === undefined) {
        /* Cap concurrency: a grid shift wants ~34 tiles at once and browsers queue anyway,
         * but flooding the service makes every tile late instead of some tiles early. */
        if (T.inflight > 12) return -1;
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
});

/* Copy a resident tile into the WASM heap. Caller must have checked w3_tiles_size first. */
EM_JS(void, w3_tiles_copy, (int kind, int z, int x, int y, uint8_t *dst), {
    var T = Module.__fbTiles; if (!T) return;
    var e = T.cache.get(kind + '/' + z + '/' + x + '/' + y);
    if (e && e.length) HEAPU8.set(e, dst);
});

/* How many tiles are resident / in flight — for the progress line and for tests. */
EM_JS(int, w3_tiles_resident, (), {
    var T = Module.__fbTiles; if (!T) return 0;
    var n = 0; T.cache.forEach(function (v) { if (v) n++; }); return n;
});

#else  /* native build: no browser, no fetch — the archive path is used instead */
static void w3_tiles_init(const char *base) { (void)base; }
static int  w3_tiles_size(int kind, int z, int x, int y) { (void)kind;(void)z;(void)x;(void)y; return -1; }
static void w3_tiles_copy(int kind, int z, int x, int y, uint8_t *dst) { (void)kind;(void)z;(void)x;(void)y;(void)dst; }
static int  w3_tiles_resident(void) { return 0; }
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
