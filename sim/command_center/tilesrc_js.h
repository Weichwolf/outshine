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
        /* Cap concurrency, but not so tightly that the centre tile starves: building ONE tile
         * needs its vector + terrain AND osmmesh pulls the 4 edge neighbours for seam stitching,
         * so a single visible tile can be ~6 requests. With the cap at 12 the tile under the
         * aircraft could sit behind the queue. The browser pipelines over one connection anyway. */
        if (T.inflight > 48) return -1;
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

#else  /* ---- native build ----
 * Same provider contract, but fetched synchronously with curl: natively we MAY block, and the
 * headless renderer is the only way to prove the streaming path (provider -> osmmesh -> mesh ->
 * pixels) without a browser. The browser's async cache is the only part this cannot exercise. */
#include <stdio.h>
#include <string.h>

#define W3_NT_CACHE 512
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
    static const char *names[] = { "vector", "terrain" };   /* must match osmmesh_tile_kind */
    char cmd[512];
    snprintf(cmd, sizeof cmd, "curl -s -f --max-time 20 '%s/t/%s/%d/%d/%d'",
             w3_nt_base, names[kind & 1], z, x, y);
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
