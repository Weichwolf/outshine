#define _GNU_SOURCE
#include "elev.h"
#include "cache.h"
#include "tilemath.h"
#include <osmmesh/terrain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* z13 ~ 12 m/px at 52 deg N: plenty for AGL, and few enough tiles to cache cheaply. */
#define FB_DEM_Z    13
#define FB_MEM_TILES 24                 /* decoded grids kept resident (LRU) */

typedef struct {
    int   z; long x, y;
    osmmesh_terrain_grid grid;
    unsigned touch;
    int   valid;
} fb_dem_tile;

static fb_dem_tile g_mem[FB_MEM_TILES];
static unsigned    g_touch = 0;
static long        g_hits = 0, g_miss = 0, g_fail = 0;

int fb_elev_init(const char *cache_dir) { return fb_cache_init(cache_dir); }

/* Get a decoded grid for the tile, from the resident set or by fetching+decoding. */
static fb_dem_tile *tile_get(int z, long x, long y) {
    for (int i = 0; i < FB_MEM_TILES; i++)
        if (g_mem[i].valid && g_mem[i].z == z && g_mem[i].x == x && g_mem[i].y == y) {
            g_mem[i].touch = ++g_touch; g_hits++; return &g_mem[i];
        }
    uint8_t *png = 0; size_t n = 0;
    if (!fb_cache_get(FB_TILE_TERRAIN, z, x, y, &png, &n)) { g_fail++; return 0; }
    osmmesh_terrain_grid grid = {0};
    int rc = osmmesh_terrain_decode_png(png, n, &grid);
    free(png);
    if (rc != OSMMESH_TERRAIN_OK || !grid.heights) { g_fail++; return 0; }

    int slot = -1; unsigned oldest = ~0u;
    for (int i = 0; i < FB_MEM_TILES; i++) {
        if (!g_mem[i].valid) { slot = i; break; }
        if (g_mem[i].touch < oldest) { oldest = g_mem[i].touch; slot = i; }
    }
    if (g_mem[slot].valid) osmmesh_terrain_grid_free(&g_mem[slot].grid);
    g_mem[slot].z = z; g_mem[slot].x = x; g_mem[slot].y = y;
    g_mem[slot].grid = grid; g_mem[slot].valid = 1; g_mem[slot].touch = ++g_touch;
    g_miss++;
    return &g_mem[slot];
}

int fb_elev_at(double lat, double lon, double *out) {
    double tx, ty;
    fb_geo_to_tile(lat, lon, FB_DEM_Z, &tx, &ty);
    long x = (long)tx, y = (long)ty;
    if (!fb_tile_wrap(FB_DEM_Z, &x, &y)) return 0;
    fb_dem_tile *t = tile_get(FB_DEM_Z, x, y);
    if (!t) return 0;
    /* fractional position inside the tile -> pixel coordinates in the decoded grid */
    double fx = (tx - (double)(long)tx) * (double)t->grid.cols;
    double fy = (ty - (double)(long)ty) * (double)t->grid.rows;
    *out = fb_bilinear(t->grid.heights, t->grid.cols, t->grid.rows, fx, fy);
    return 1;
}

void fb_elev_stats(long *hits, long *misses, long *fetch_fail) {
    if (hits) *hits = g_hits;
    if (misses) *misses = g_miss;
    if (fetch_fail) *fetch_fail = g_fail;
}
