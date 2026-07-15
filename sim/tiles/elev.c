#define _GNU_SOURCE
#include "elev.h"
#include "tilemath.h"
#include <osmmesh/terrain.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Terrarium DEM from the public AWS/Tilezen mirror. Byte-identical to what the vendored
 * osmmesh decoder expects (h = R*256 + G + B/256 - 32768). Free, CORS, no key. */
#define FB_DEM_URL  "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%ld/%ld.png"
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
static char        g_dir[256] = "/var/cache/fbtiles";
static long        g_hits = 0, g_miss = 0, g_fail = 0;

int fb_elev_init(const char *cache_dir) {
    if (cache_dir && *cache_dir) snprintf(g_dir, sizeof g_dir, "%s", cache_dir);
    char sub[300]; snprintf(sub, sizeof sub, "%s/terrarium", g_dir);
    mkdir(g_dir, 0755); mkdir(sub, 0755);
    return 0;
}

/* Read the whole file. Caller frees. */
static uint8_t *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    uint8_t *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
}

/* Fetch a DEM tile to the disk cache if it isn't there yet. Returns 0 on success.
 * curl writes to a .tmp and we rename, so a killed fetch never leaves a truncated tile
 * in the cache to be served forever. */
static int disk_fetch(int z, long x, long y, char *path, size_t pathsz) {
    snprintf(path, pathsz, "%s/terrarium/%d_%ld_%ld.png", g_dir, z, x, y);
    struct stat st;
    if (stat(path, &st) == 0 && st.st_size > 0) return 0;          /* cache hit */
    char url[512], cmd[1200], tmp[300];
    snprintf(url, sizeof url, FB_DEM_URL, z, x, y);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    snprintf(cmd, sizeof cmd, "curl -s -f --max-time 15 -o '%s' '%s'", tmp, url);
    int rc = system(cmd);
    if (rc != 0) { remove(tmp); return -1; }
    if (stat(tmp, &st) != 0 || st.st_size <= 0) { remove(tmp); return -1; }
    if (rename(tmp, path) != 0) { remove(tmp); return -1; }
    return 0;
}

/* Get a decoded grid for the tile, from the resident set or by fetching+decoding. */
static fb_dem_tile *tile_get(int z, long x, long y) {
    for (int i = 0; i < FB_MEM_TILES; i++)
        if (g_mem[i].valid && g_mem[i].z == z && g_mem[i].x == x && g_mem[i].y == y) {
            g_mem[i].touch = ++g_touch; g_hits++; return &g_mem[i];
        }
    char path[300];
    if (disk_fetch(z, x, y, path, sizeof path) != 0) { g_fail++; return 0; }
    size_t n = 0; uint8_t *png = read_file(path, &n);
    if (!png) { g_fail++; return 0; }
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
