#define _GNU_SOURCE
#include "elev.h"
#include "cache.h"
#include "tilemath.h"
#include "lru.h"
#include "prefetch_api.h"
#include <osmmesh/terrain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FB_DEM_Z    13
#define FB_MEM_TILES 24

static fb_lru_slot g_slot[FB_MEM_TILES];
static fb_lru      g_lru;
static int         g_lru_ready = 0;
static osmmesh_terrain_grid g_grid[FB_MEM_TILES];
static long        g_hits = 0, g_miss = 0, g_fail = 0;

int fb_elev_init(const char *cache_dir) {
    fb_lru_init(&g_lru, g_slot, FB_MEM_TILES); g_lru_ready = 1;
    return fb_cache_init(cache_dir);
}

static osmmesh_terrain_grid *tile_get(int z, long x, long y) {
    if (!g_lru_ready) { fb_lru_init(&g_lru, g_slot, FB_MEM_TILES); g_lru_ready = 1; }
    int i = fb_lru_find(&g_lru, z, x, y);
    if (i >= 0) { g_hits++; return &g_grid[i]; }

    uint8_t *png = 0; size_t n = 0;
    if (!fb_cache_ondisk(FB_TILE_TERRAIN, z, x, y, &png, &n)) {
        fb_pf_fetch(FB_TILE_TERRAIN, z, x, y);
        g_fail++; return 0;
    }
    osmmesh_terrain_grid grid = {0};
    int rc = osmmesh_terrain_decode_png(png, n, &grid);
    free(png);
    if (rc != OSMMESH_TERRAIN_OK || !grid.heights) { g_fail++; return 0; }

    int slot = fb_lru_victim(&g_lru);
    if (fb_lru_occupied(&g_lru, slot)) osmmesh_terrain_grid_free(&g_grid[slot]);
    g_grid[slot] = grid;
    fb_lru_claim(&g_lru, slot, z, x, y);
    g_miss++;
    return &g_grid[slot];
}

int fb_elev_at(double lat, double lon, double *out) {
    double tx, ty;
    fb_geo_to_tile(lat, lon, FB_DEM_Z, &tx, &ty);
    long x = (long)tx, y = (long)ty;
    if (!fb_tile_wrap(FB_DEM_Z, &x, &y)) return 0;
    osmmesh_terrain_grid *g = tile_get(FB_DEM_Z, x, y);
    if (!g) return 0;

    double fx = (tx - (double)(long)tx) * (double)g->cols;
    double fy = (ty - (double)(long)ty) * (double)g->rows;
    *out = fb_bilinear(g->heights, g->cols, g->rows, fx, fy);
    return 1;
}

/* /elev is a POINT query: the caller wants THE answer, and the one DEM tile lands in <1 s
 * (measured 0.27-0.79 s cold). fb_elev_at returns 503 the instant the tile is cold, which makes
 * the aircraft- and renderer-startup seeds spawn at the wrong ground. This variant enqueues the
 * fetch ONCE (via the first fb_elev_at miss) and then polls the disk until it lands or the deadline
 * expires -- never re-enqueuing, so the prefetch pool dedups it against any concurrent /elev.
 * ONLY the startup paths ask for this (?block=1); the aircraft's 250 ms AGL poller must not, or a
 * flight into a cold region would stall the single-threaded server for the whole deadline. */
int fb_elev_at_blocking(double lat, double lon, double *out, int deadline_ms) {
    if (fb_elev_at(lat, lon, out)) return 1;          /* warm: done. cold: a fetch is now queued. */
    double tx, ty;
    fb_geo_to_tile(lat, lon, FB_DEM_Z, &tx, &ty);
    long x = (long)tx, y = (long)ty;
    if (!fb_tile_wrap(FB_DEM_Z, &x, &y)) return 0;
    for (int waited = 0; waited < deadline_ms; waited += 50) {
        usleep(50 * 1000);
        uint8_t *png = 0; size_t n = 0;
        if (fb_cache_ondisk(FB_TILE_TERRAIN, FB_DEM_Z, x, y, &png, &n)) {  /* disk check, no enqueue */
            free(png);
            return fb_elev_at(lat, lon, out);         /* on disk now: decode into the LRU and sample */
        }
    }
    return 0;
}

void fb_elev_stats(long *hits, long *misses, long *fetch_fail) {
    if (hits) *hits = g_hits;
    if (misses) *misses = g_miss;
    if (fetch_fail) *fetch_fail = g_fail;
}
