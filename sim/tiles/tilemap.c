/* FlightBox tiles — Esri's tilemap oracle: the fetching and caching half.
 * The parse is pure and lives in tilemap.h (100% unit-tested, no network). See there for WHY.
 */
#define _GNU_SOURCE
#include "tilemap_api.h"
#include "tilemap.h"
#include "http.h"
#include <stdio.h>
#include <pthread.h>

/* One request answers a whole square. Measured against the live service: 32x32 = 1024 tiles in
 * 80 ms and ~2 KB of reply. That is the entire reason to ask an oracle instead of probing tiles:
 * one round trip replaces a thousand. */
#define FB_TM_BLOCK 32

/* Esri's tilemap URL is /tilemap/{z}/{top}/{left}/{h}/{w} -- row before column, matching the tile
 * URL's {z}/{y}/{x}. Verified against the reply's own `location` field, not assumed. */
#define FB_TM_URL "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer" \
                  "/tilemap/%d/%ld/%ld/%d/%d"

/* Known bits, keyed by (z,x,y). Open addressing, fixed size, never grows.
 *
 * Per-TILE and not per-block on purpose: the service adjusts requests (a 32x32 ask came back as
 * 32x4, `"adjusted": true`), so what we learn is whatever rectangle the reply describes -- storing
 * it under the block we ASKED for would file the right answers at the wrong coordinates. */
#define FB_TM_CAP 65536                    /* 64k tiles ~ 1 MB; a session touches far fewer */
typedef struct { int z; long x, y; unsigned char used, bit; } tm_ent;
static tm_ent          g_tab[FB_TM_CAP];
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static long g_q = 0, g_hit = 0, g_miss = 0, g_learned = 0, g_full = 0;

static size_t tm_slot(int z, long x, long y) {
    size_t h = (size_t)z * 2654435761u ^ (size_t)x * 40503u ^ (size_t)y * 2246822519u;
    return h % FB_TM_CAP;
}

/* caller holds g_mx */
static int tab_get(int z, long x, long y) {
    size_t i = tm_slot(z, x, y);
    for (int probe = 0; probe < 32; probe++, i = (i + 1) % FB_TM_CAP) {
        if (!g_tab[i].used) return -1;
        if (g_tab[i].z == z && g_tab[i].x == x && g_tab[i].y == y) return g_tab[i].bit;
    }
    return -1;
}
static void tab_put(int z, long x, long y, unsigned char bit) {
    size_t i = tm_slot(z, x, y);
    for (int probe = 0; probe < 32; probe++, i = (i + 1) % FB_TM_CAP) {
        if (!g_tab[i].used || (g_tab[i].z == z && g_tab[i].x == x && g_tab[i].y == y)) {
            g_tab[i].z = z; g_tab[i].x = x; g_tab[i].y = y;
            g_tab[i].used = 1; g_tab[i].bit = bit;
            return;
        }
    }
    g_full++;   /* the table is a cache, so dropping is fine -- but it must be VISIBLE in /health,
                 * or a full table looks exactly like a slow network. */
}

int fb_tm_has(int z, long x, long y) {
    pthread_mutex_lock(&g_mx);
    int v = tab_get(z, x, y);
    pthread_mutex_unlock(&g_mx);
    if (v >= 0) { __atomic_fetch_add(&g_hit, 1, __ATOMIC_RELAXED); return v; }
    __atomic_fetch_add(&g_miss, 1, __ATOMIC_RELAXED);

    long bx = (x / FB_TM_BLOCK) * FB_TM_BLOCK, by = (y / FB_TM_BLOCK) * FB_TM_BLOCK;
    char url[320];
    snprintf(url, sizeof url, FB_TM_URL, z, by, bx, FB_TM_BLOCK, FB_TM_BLOCK);

    uint8_t *body = 0; size_t n = 0;
    long code = fb_http_get(url, &body, &n);
    __atomic_fetch_add(&g_q, 1, __ATOMIC_RELAXED);
    if (code != 200 || !body) { free(body); return -1; }

    fb_tm_rect r;
    static _Thread_local unsigned char bits[FB_TM_BLOCK * FB_TM_BLOCK];
    int got = fb_tm_parse((const char *)body, n + 1, &r, bits, (int)sizeof bits);
    free(body);
    /* 0 means "learned nothing", never "no tiles". Returning -1 (unknown) sends the caller to
     * fetch the tile anyway -- the oracle failing must cost quality, never correctness. */
    if (got <= 0) return -1;

    pthread_mutex_lock(&g_mx);
    for (int j = 0; j < r.height; j++)
        for (int i = 0; i < r.width; i++)
            tab_put(z, r.left + i, r.top + j, bits[j * r.width + i]);
    g_learned += got;
    v = tab_get(z, x, y);
    pthread_mutex_unlock(&g_mx);
    /* v can still be -1: the reply may describe a rectangle that does not contain the tile we
     * asked about (that is what `adjusted` does). Unknown, so the caller fetches. Correct, and
     * one wasted round trip -- which is the right way round. */
    return v;
}

void fb_tm_stats(long *queries, long *hits, long *misses, long *learned, long *dropped) {
    if (queries) *queries = g_q;
    if (hits)    *hits    = g_hit;
    if (misses)  *misses  = g_miss;
    if (learned) *learned = g_learned;
    if (dropped) *dropped = g_full;
}
