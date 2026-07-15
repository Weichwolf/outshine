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

/* z13 ~ 12 m/px at 52 deg N: plenty for AGL, and few enough tiles to cache cheaply. */
#define FB_DEM_Z    13
#define FB_MEM_TILES 24                 /* decoded grids kept resident (LRU) */

/* The LRU bookkeeping lives in lru.h (pure, unit-tested); the decoded grids are our payload,
 * indexed by the same slot number. */
static fb_lru_slot g_slot[FB_MEM_TILES];
static fb_lru      g_lru;
static int         g_lru_ready = 0;
static osmmesh_terrain_grid g_grid[FB_MEM_TILES];
static long        g_hits = 0, g_miss = 0, g_fail = 0;

int fb_elev_init(const char *cache_dir) {
    fb_lru_init(&g_lru, g_slot, FB_MEM_TILES); g_lru_ready = 1;
    return fb_cache_init(cache_dir);
}

/* Get a decoded grid for the tile, from the resident set or by fetching+decoding. */
static osmmesh_terrain_grid *tile_get(int z, long x, long y) {
    if (!g_lru_ready) { fb_lru_init(&g_lru, g_slot, FB_MEM_TILES); g_lru_ready = 1; }
    int i = fb_lru_find(&g_lru, z, x, y);
    if (i >= 0) { g_hits++; return &g_grid[i]; }

    /* Disk only, then queue. fb_cache_get would curl here -- inside the accept() loop, blocking
     * every other client for up to 20 s. The aircraft polls this on a background thread and keeps
     * its last good ground height on failure (see aircraft/terrain.c), so "not yet" costs it a
     * poll interval, not a wrong altitude. */
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
    /* fractional position inside the tile -> pixel coordinates in the decoded grid */
    double fx = (tx - (double)(long)tx) * (double)g->cols;
    double fy = (ty - (double)(long)ty) * (double)g->rows;
    *out = fb_bilinear(g->heights, g->cols, g->rows, fx, fy);
    return 1;
}

void fb_elev_stats(long *hits, long *misses, long *fetch_fail) {
    if (hits) *hits = g_hits;
    if (misses) *misses = g_miss;
    if (fetch_fail) *fetch_fail = g_fail;
}
