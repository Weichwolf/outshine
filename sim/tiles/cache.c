#define _GNU_SOURCE
#include "cache.h"
#include "tilesrc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


static char g_dir[256] = "/var/cache/fbtiles";
static long g_hits = 0, g_fetch = 0, g_fail = 0;


int fb_cache_init(const char *dir) {
    if (dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    mkdir(g_dir, 0755);
    for (int i = 0; i < FB_TILE_KIND_COUNT; i++) {
        char sub[320]; snprintf(sub, sizeof sub, "%s/%s", g_dir, fb_src_kind_name((fb_tile_kind)i));
        mkdir(sub, 0755);
    }
    return 0;
}

static void cache_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.%s", g_dir, fb_src_kind_name(k), z, x, y, fb_src_ext(k));
}

static uint8_t *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    uint8_t *b = malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) { free(b); fclose(f); return 0; }
    fclose(f); *n = (size_t)sz; return b;
}

int fb_cache_ondisk(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return 0;
    char path[400]; cache_path(k, z, x, y, path, sizeof path);
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= 0) return 0;
    *out = read_file(path, n);
    if (!*out) return 0;
    g_hits++;
    return 1;
}

/* BLOCKS on a miss (curl, up to 20 s). Only the prefetch worker may call this -- never the
 * accept() loop, which serves every client including the live flight view. */
int fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return 0;
    if (fb_cache_ondisk(k, z, x, y, out, n)) return 1;
    char path[400]; cache_path(k, z, x, y, path, sizeof path);
    struct stat st;
    char url[600];
    if (!fb_src_url(k, z, x, y, url, sizeof url)) { g_fail++; return 0; }

    /* --compressed: VersaTiles gzips as transfer encoding; curl un-gzips it for us, so the
     * cache holds exactly what a decoder expects. Write to .tmp then rename, so a killed
     * fetch can never leave a truncated tile to be served forever. */
    char tmp[420], cmd[1400];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    snprintf(cmd, sizeof cmd, "curl -s -f --compressed --max-time 20 -o '%s' '%s'", tmp, url);
    if (system(cmd) != 0 || stat(tmp, &st) != 0 || st.st_size <= 0) { remove(tmp); g_fail++; return 0; }
    if (rename(tmp, path) != 0) { remove(tmp); g_fail++; return 0; }
    *out = read_file(path, n);
    if (!*out) { g_fail++; return 0; }
    g_fetch++;
    return 1;
}

void fb_cache_stats(long *hits, long *fetches, long *fails) {
    if (hits) *hits = g_hits;
    if (fetches) *fetches = g_fetch;
    if (fails) *fails = g_fail;
}
