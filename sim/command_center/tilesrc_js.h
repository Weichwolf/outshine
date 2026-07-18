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
 *
 * Browser-only, and says so rather than pretending otherwise. There used to be a whole second
 * implementation behind #else here: ~55 lines of curl/popen that fetched the same tiles
 * synchronously, "because natively we MAY block, and the headless renderer is the only way to
 * prove the streaming path without a browser". That renderer (render_native.c) was deleted in
 * f36f147 and took the last native consumer with it -- this header only ever reaches a compiler
 * through world3d.h, and world3d.h only through cc.c. So the branch was unreachable, and worse
 * than dead: it described a portability constraint that does not exist, which is exactly the kind
 * of fossil that distorts the next person's refactor. If a native consumer is ever wanted again,
 * the #error below is where to start -- deliberately louder than a fallback that silently
 * "works" while being maintained by nobody and exercised by nothing.
 */
#ifndef W3_TILESRC_JS_H
#define W3_TILESRC_JS_H
#include <osmmesh/osmmesh.h>
#include <stdlib.h>

#ifndef __EMSCRIPTEN__
#error "tilesrc_js.h is browser-only (EM_JS/fetch). The native path died with render_native.c (f36f147); the visual check is a headless browser on the real app -- see test/shot.sh."
#endif
#include <emscripten.h>

/* Aerial photo tiles. osmmesh does not know about these -- it only needs vector + terrain to
 * build a mesh. The imagery is purely the renderer's business (it is only an albedo), so it
 * rides the same byte cache under its own kind rather than becoming an osmmesh_tile_kind. */
#define W3_TILE_IMAGERY 2

/* Two things about the EM_JS calls below look like typos and are not. Both cost a reader a
 * detour once, so: no trailing ';' after the closing ')', and (void) spelled out where a C
 * programmer would write ().
 *
 * The semicolon: _EM_JS already expands to a declaration that ends in one
 * (`char __em_js__##name[] = ...;`, then _EM_END_CDECL, which is empty in C). Ours was simply a
 * second one at file scope -- what -Wextra-semi is for. The docs write it WITH the semicolon.
 *
 * The (void): EM_JS stringifies this parameter list into the JAVASCRIPT signature
 * ("(int x)<::>{body}" -> `function name(x) {body}`). So `(void)` would read as a parameter
 * named `void` -- a reserved word, i.e. broken JS, from C that compiles clean. It only works
 * because emscripten.py's create_em_js special-cases it (`if not args or args == 'void'`).
 * Anything cleverer than a plain (void) here is worth checking against that function first,
 * because no C compiler can see the JS this produces. */
/* Base URL of fb-tiles, e.g. "http://localhost:8081". Set once at start-up.
 *
 * T.get is the ONE place that decides what a response MEANS. It used to be decided twice -- the
 * raw-tile fetch and the albedo fetch carried the same eight lines -- and so it carried the same
 * bug twice:
 *
 *     .then(function (r) { return r.ok ? r.arrayBuffer() : null; })
 *     .then(function (b) { T.cache.set(key, b ? new Uint8Array(b) : new Uint8Array(0)); })
 *
 * fb-tiles answers a miss with 404 and the body "fetching\n" / "baking\n" -- it is telling us it
 * has queued the work and we should ask again. `!r.ok` threw that away and recorded the tile as an
 * empty one, permanently: `e === undefined` is the only thing that re-fetches, so the tile was
 * never asked for again. MEASURED consequence: a cold region never loads. Over the Matterhorn the
 * streamer sat at "0 chunks drawn, 39 pending" and went silent WHILE fb-tiles served those very
 * tiles with 200 -- the browser had written them off seconds earlier. Hameln only ever worked
 * because its cache volume has been warm for months.
 *
 * The rule, and it is deliberately asymmetric:
 *
 *     ONLY 200 (here it is) and 204 (nothing here, ever) are terminal. Everything else --
 *     202, 404, 5xx, a network error, anything we have not thought of -- means ASK AGAIN.
 *
 * Unknown must fall towards the visible failure, not the invisible one. A retry loop is annoying
 * and gets noticed; a silent hole looks exactly like a working world, which is what let this
 * survive. Note the status is read, not the body: matching on "baking\n" would be a second, weaker
 * copy of the same fact, and it would break the first time someone rewords the string. */
EM_JS(void, w3_tiles_init, (const char *base), {
    var T = { base: UTF8ToString(base), cache: new Map(), inflight: 0 };
    T.get = function (url, key) {
        fetch(url).then(function (r) {
            if (r.status === 200) return r.arrayBuffer().then(function (buf) { return new Uint8Array(buf); });
            if (r.status === 204) return new Uint8Array(0);   /* terminal: the server HAS looked, there is nothing */
            return null;                                      /* 202/404/5xx/anything: not an answer yet */
        }).then(function (v) {
            if (v === null) T.cache.delete(key);              /* forget it -> the next frame asks again */
            else T.cache.set(key, v);                         /* bytes, or a real 0-length hole */
            T.inflight--;
        }).catch(function () { T.cache.delete(key); T.inflight--; });   /* network error: also just retry */
    };
    Module.__fbTiles = T;
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
         * which is nearest-first (see w3_walk). */
        if (T.inflight > 256) return -1;
        T.cache.set(key, null);                        /* null = pending */
        T.inflight++;
        T.get(T.base + '/t/' + names[kind] + '/' + z + '/' + x + '/' + y, key);
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

/* ---- baked ground albedo -------------------------------------------------------------------
 * fb-tiles rasterises the OSM cartography and stitches the aerial mosaic, then keeps the result
 * on disk. So the renderer downloads ONE finished image per tile per albedo instead of a vector
 * tile plus 4 seam neighbours (OSM) or 16 z16 children (photo) — and rasterises nothing.
 *
 * Same async contract as the raw tiles, and the same T.get: -1 means "not here yet, ask again",
 * 0 means "the server looked and there is nothing", >0 is bytes. Never blocks.
 */
EM_JS(int, w3_bake_size, (int photo, int z, int x, int y, int tex), {
    var T = Module.__fbTiles; if (!T) return -1;
    var key = 'b' + photo + '/' + tex + '/' + z + '/' + x + '/' + y;
    var e = T.cache.get(key);
    if (e === undefined) {
        if (T.inflight > 256) return -1;
        T.cache.set(key, null);
        T.inflight++;
        T.get(T.base + '/bake/' + (photo ? 'photo' : 'osm') + '/' + z + '/' + x + '/' + y + '?tex=' + tex, key);
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

/* ---- tile worker glue (main-thread side) ---------------------------------------------------
 * Spawns ONE tileworker.js and pumps messages. The worker fetches+decodes+meshes a tile off-thread
 * and posts back a finished vertex array (Transferable). This side copies it into the main heap
 * and calls the C entry points in world3d.h (w3_worker_mesh / w3_worker_fail, both KEEPALIVE).
 *
 * `_w3_worker_mesh` etc. are called directly (not via ccall): an EM_JS body runs in the module
 * scope where exported wasm functions are `_name` and `_malloc`/`HEAPF32` are in scope.
 *
 * ONE worker, not a pool. A 4-worker pool was built and measured and NOT committed: the per-tile
 * work is 72 % fetch-wait so parallelism looks right, and it does double throughput (2.05x) -- but a
 * moving camera makes fast workers build ~29 % more tiles than trim keeps, a HARDWARE-INDEPENDENT
 * churn that cancels the gain, so the naive pool is the fix on no hardware. The reported symptom
 * (load stutter) is already gone from moving the work off-thread at all (frame p95 752 -> 27 ms).
 * Full measurements and the real fix (bound the in-flight set by distance) are in CLAUDE.md Open
 * Work -- kept in ONE place so the two do not drift. */
/* ONE build in flight at a time -- a load-bearing invariant, not a throttle. tw_build SUSPENDS on
 * its synchronous fetch (ASYNCIFY unwind point), and ASYNCIFY has a SINGLE unwind buffer: a second
 * tw_build entered while the first is mid-suspend corrupts that shared state. With -sASSERTIONS it
 * trips "cannot start an async operation when one is already in flight"; in the production build
 * (assertions off) it is a silent "memory access out of bounds" that kills the worker. It was
 * invisible at Hameln (warm cache -> builds return 200 at once, little overlap) and fatal at a
 * steep/cold origin, where extreme relief makes the walk request many tiles at once and cold tiles
 * answer 202 (each build sleeps 50 ms/retry, staying suspended for seconds) -- so the burst of
 * posts drove the worker's onmessage to dispatch handle() re-entrantly, two async tw_builds
 * overlapping. The worker fetches sequentially anyway (single thread), so gating to one in flight
 * costs no throughput; it only moves the queue from the worker's message loop (where the overlap
 * lived) to here. Send one, wait for its 'built' reply, then send the next. */
EM_JS(void, w3_worker_init, (const char *base, double lat, double lon), {
    var baseStr = UTF8ToString(base);   /* NOW -- the C pointer is not valid inside the callback */
    var T = { w: new Worker('tileworker.js'), opened: false, q: [], busy: false };
    Module.__tw = T;
    T.pump = function () {                 /* at most one build outstanding with the worker */
        if (!T.opened || T.busy || T.q.length === 0) return;
        T.busy = true; T.w.postMessage(T.q.shift());
    };
    T.w.onmessage = function (e) {
        var d = e.data;
        if (d.type === 'ready') {
            T.w.postMessage({ cmd: 'open', base: baseStr, lat: lat, lon: lon });
        } else if (d.type === 'opened') {
            T.opened = true;
            T.pump();
        } else if (d.type === 'built') {
            T.busy = false;                /* worker is idle again -> the next build may go */
            if (d.ok) {
                var floats = d.nverts * 8;                 /* w3_vtx = 8 floats */
                var ptr = _malloc(floats * 4);
                HEAPF32.set(new Float32Array(d.verts), ptr >> 2);
                _w3_worker_mesh(d.z, d.x, d.y, ptr, d.nverts, d.err, d.yoff);
            } else {
                _w3_worker_fail(d.z, d.x, d.y);
            }
            T.pump();
        }
    };
})
/* Ask the worker for one tile. Queue it and pump: the request goes out only when the worker is open
 * AND not mid-build, so a build posted before tw_open (no world yet) or during another build (the
 * ASYNCIFY overlap above) waits its turn instead of racing. WAIT is the per-tile dedup (w3_cache_get
 * never re-posts a slot), so the queue is bounded by the resident set, not by frames. */
EM_JS(void, w3_worker_post, (int z, int x, int y, int grid, int want_yoff), {
    var T = Module.__tw; if (!T) return;
    T.q.push({ cmd: 'build', z: z, x: x, y: y, grid: grid, want_yoff: want_yoff });
    T.pump();
})

#endif /* W3_TILESRC_JS_H */
