/* FlightBox — tile worker: fetch, decode and mesh a chunk OFF the main thread.
 *
 * WHY THIS EXISTS. Measured (rAF callback, swiftshader): p95 = 752 ms while the world streams
 * against 2.6 ms once it is done -- single frames stood 850 ms. The cause was never the download
 * (the bytes arrive async and never block); it was that `world3d_stream_at` ran inside frame()
 * and did the whole tile there. Per tile, measured, split by outcome:
 *
 *     MVT (fetch + decode)      0.75 ms    3 %
 *     DEM (fetch + decode)      8.87 ms   36 %
 *     mesh build               14.85 ms   61 %
 *     ---------------------------------
 *                              24.47 ms   x ~160 tiles = ~3.9 s of frame time
 *
 * plus the albedo decode (3.9 ms x 320). 752 ms / 24.5 ms is ~30 tiles in ONE frame: the walk
 * baked everything that had arrived, synchronously, with no ceiling.
 *
 * WHY A PLAIN WORKER AND NOT -pthread. The obvious answer was pthreads + SharedArrayBuffer: no
 * marshalling, the mesh lands straight in the main heap. That argument is FALSE here. The tile
 * bytes live in a JS Map on the MAIN thread (tilesrc_js.h), and EM_JS runs on the CALLING thread
 * -- which is why emscripten has MAIN_THREAD_EM_ASM and emscripten_asm_const_*_on_main_thread at
 * all. A pthread's provider callback would find no cache, so every one of the ~1800 byte requests
 * per load would have to be proxied synchronously INTO the main thread -- the very thread we are
 * emptying, while it sits in requestAnimationFrame. SAB would buy nothing and cost COOP/COEP plus
 * a growable-SAB heap.
 *
 * So the bytes cross a boundary either way. This worker owns its own fetching and hands back one
 * finished vertex array per tile, transferred (zero-copy). And because a worker MAY block,
 * emscripten_fetch with EMSCRIPTEN_FETCH_SYNCHRONOUS removes the whole retry/pending machine that
 * the main thread needs: here, osmmesh_fetch_tile simply runs to completion.
 *
 * THE CUT WAS ALREADY THERE. w3_chunk_build (chunkmesh.h) was split out of world3d.h yesterday so
 * that `err` -- the number the entire LOD runs on -- could be unit-tested without a GL context.
 * That same purity is what lets it run here: "what IS a chunk" needs no main thread, "how does it
 * reach the GPU" cannot leave it. One cut, two reasons, nobody planned the second.
 */
#include <emscripten.h>
#include <emscripten/fetch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osmmesh/osmmesh.h"
#include "chunkmesh.h"

static osmmesh_ctx *tw_osm = 0;
static char tw_base[160] = "";

/* Byte provider: SYNCHRONOUS on purpose.
 *
 * This is the whole reason a worker is simpler than a pthread. On the main thread a fetch may
 * never block, so tilesrc_js.h needs a cache, a pending marker, a retry rule and the 200/204
 * contract. Here we are allowed to wait -- so "get me these bytes" is a function call again, and
 * osmmesh_fetch_tile runs start to finish instead of being poked once per frame until it works.
 *
 * The 202 ("queued, ask again") case still exists: fb-tiles answers it while it bakes. We spin
 * with a bounded retry rather than returning a hole, because a hole here is a permanently missing
 * chunk -- the exact bug (404 cached as empty forever) that cost a whole day. Only 200 gives
 * bytes; 204 is a real hole; anything else means ask again. */
/* How much the polling actually costs us. The 202 is not ours: fb-tiles' accept() loop is single
 * threaded, so it may not block, so it says "queued, ask again", so we retry, so we sleep, so we
 * pay 35 KB of ASYNCIFY. That is one team's architectural constraint arriving in another's binary
 * size -- and nobody has measured how often it actually fires. If `?wait=1` ever lands (a
 * blocking, opt-in route for callers that MAY wait -- this worker may, the main thread never),
 * these counters say whether removing the retry is worth the server rebuild. */
static int tw_n_202 = 0, tw_n_200 = 0, tw_n_204 = 0;
static double tw_ms_slept = 0;
EMSCRIPTEN_KEEPALIVE int    tw_stat_202(void)  { return tw_n_202; }
EMSCRIPTEN_KEEPALIVE int    tw_stat_200(void)  { return tw_n_200; }
EMSCRIPTEN_KEEPALIVE int    tw_stat_204(void)  { return tw_n_204; }
EMSCRIPTEN_KEEPALIVE double tw_stat_slept(void){ return tw_ms_slept; }

static int tw_get(const char *url, uint8_t **out, size_t *len)
{
    for (int attempt = 0; attempt < 60; attempt++) {
        emscripten_fetch_attr_t a;
        emscripten_fetch_attr_init(&a);
        strcpy(a.requestMethod, "GET");
        a.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS
                     | EMSCRIPTEN_FETCH_REPLACE;
        emscripten_fetch_t *f = emscripten_fetch(&a, url);
        if (!f) return 0;
        unsigned short st = f->status;
        if (st == 200 && f->numBytes > 0) {
            uint8_t *b = (uint8_t *)malloc((size_t)f->numBytes);
            if (!b) { emscripten_fetch_close(f); return 0; }
            memcpy(b, f->data, (size_t)f->numBytes);
            *out = b; *len = (size_t)f->numBytes;
            emscripten_fetch_close(f);
            tw_n_200++;
            return 1;
        }
        emscripten_fetch_close(f);
        if (st == 204) { tw_n_204++; return 0; }  /* the server looked: nothing here, ever */
        /* 202/404/5xx: queued or transient. Wait for the bake and ask again. */
        tw_n_202++;
        emscripten_sleep(50);
        tw_ms_slept += 50;
    }
    return 0;                              /* gave up: the caller treats it as absent for now */
}

static int tw_provider(void *user, osmmesh_tile_kind kind,
                       uint32_t z, uint32_t x, uint32_t y,
                       uint8_t **out, size_t *len)
{
    (void)user;
    static const char *names[] = { "vector", "terrain", "imagery" };
    if ((int)kind < 0 || (int)kind >= 3) return 0;
    char url[256];
    snprintf(url, sizeof url, "%s/t/%s/%u/%u/%u", tw_base, names[kind], z, x, y);
    return tw_get(url, out, len);
}

/* Open the world. Same config as the main thread's world3d_tiles_open -- the ENU origin matters:
 * the mesh this worker builds is relative to it, which is also why a server could not build it
 * for us (it would be origin-specific and the tile cache would fall apart per origin). */
EMSCRIPTEN_KEEPALIVE
int tw_open(const char *base, double lat, double lon)
{
    snprintf(tw_base, sizeof tw_base, "%s", base ? base : "");
    osmmesh_config cfg = { .origin_lat = lat, .origin_lon = lon,
        .tile_provider = tw_provider, .tile_provider_user = 0,
        .provider_terrain_max_zoom = 15,
        .enable_terrain = 1, .enable_buildings = 0, .enable_linears = 0 };
    if (osmmesh_create(&cfg, &tw_osm) != OSMMESH_OK) { tw_osm = 0; return 0; }
    return 1;
}

/* The result of one tile, read back by JS after tw_build. Kept as globals rather than returned in
 * a struct because the JS side reads them through HEAP views; one tile is in flight at a time. */
static w3_chunk tw_out;
static float tw_yoff = 0.f;

EMSCRIPTEN_KEEPALIVE float *tw_verts(void)  { return (float *)tw_out.verts; }
EMSCRIPTEN_KEEPALIVE int    tw_nverts(void) { return tw_out.nverts; }
EMSCRIPTEN_KEEPALIVE float  tw_err(void)    { return tw_out.err; }
EMSCRIPTEN_KEEPALIVE float  tw_yoff_get(void) { return tw_yoff; }
EMSCRIPTEN_KEEPALIVE void   tw_release(void) { w3_chunk_free(&tw_out); }

/* Build one chunk: fetch (blocking), decode, mesh. Returns 1 on success.
 * `want_yoff` asks for the origin's ground elevation, which only the centre tile can answer. */
EMSCRIPTEN_KEEPALIVE
int tw_build(int z, int x, int y, int grid, int want_yoff)
{
    if (!tw_osm) return 0;
    w3_chunk_free(&tw_out);

    osmmesh_tile t = {0};
    if (osmmesh_fetch_tile(tw_osm, (uint8_t)z, (uint32_t)x, (uint32_t)y, &t) != OSMMESH_OK
        || !t.terrain) { osmmesh_free_tile(&t); return 0; }

    if (want_yoff) {
        float best = 1e30f;
        for (uint32_t i = 0; i < t.terrain->n_vertices; i++) {
            const float *P = t.terrain->positions + i * 3;
            float dd = P[0]*P[0] + P[1]*P[1];
            if (dd < best) { best = dd; tw_yoff = P[2]; }
        }
    }

    int ok = w3_chunk_build(t.terrain, grid, &tw_out);
    osmmesh_free_tile(&t);
    return ok;
}
