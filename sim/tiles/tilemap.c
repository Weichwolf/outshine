#define _GNU_SOURCE
#include "tilemap_api.h"
#include "tilemap.h"
#include "http.h"
#include <stdio.h>
#include <pthread.h>

#define FB_TM_BLOCK 32

#define FB_TM_URL "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer" \
                  "/tilemap/%d/%ld/%ld/%d/%d"

#define FB_TM_CAP 65536
typedef struct { int z; long x, y; unsigned char used, bit; } tm_ent;
static tm_ent          g_tab[FB_TM_CAP];
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static long g_q = 0, g_hit = 0, g_miss = 0, g_learned = 0, g_full = 0;

static size_t tm_slot(int z, long x, long y) {
    size_t h = (size_t)z * 2654435761u ^ (size_t)x * 40503u ^ (size_t)y * 2246822519u;
    return h % FB_TM_CAP;
}

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
    g_full++;
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

    if (got <= 0) return -1;

    pthread_mutex_lock(&g_mx);
    for (int j = 0; j < r.height; j++)
        for (int i = 0; i < r.width; i++)
            tab_put(z, r.left + i, r.top + j, bits[j * r.width + i]);
    g_learned += got;
    v = tab_get(z, x, y);
    pthread_mutex_unlock(&g_mx);

    return v;
}

void fb_tm_stats(long *queries, long *hits, long *misses, long *learned, long *dropped) {
    if (queries) *queries = g_q;
    if (hits)    *hits    = g_hit;
    if (misses)  *misses  = g_miss;
    if (learned) *learned = g_learned;
    if (dropped) *dropped = g_full;
}
