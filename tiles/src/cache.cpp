#include "cache.h"
#include "tilesrc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "http.h"
#include "tilemap_api.h"
#include <time.h>

#define FB_ABSENT_TTL_S (30L * 24 * 3600)
static char g_dir[256] = "/var/cache/fbtiles";
static long g_absent_ttl = FB_ABSENT_TTL_S;
static long g_hits = 0, g_fetch = 0, g_fail = 0, g_absent = 0;

#define FB_FETCH_TIMEOUT_S 20L

int fb_cache_init(const char *dir) {
    if (dir && *dir) snprintf(g_dir, sizeof g_dir, "%s", dir);
    mkdir(g_dir, 0755);
    for (int i = 0; i < FB_TILE_KIND_COUNT; i++) {
        char sub[320]; snprintf(sub, sizeof sub, "%s/%s", g_dir, fb_src_kind_name((fb_tile_kind)i));
        mkdir(sub, 0755);
    }
    const char *t = getenv("TILES_ABSENT_TTL_S");
    if (t && *t) g_absent_ttl = atol(t);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

static void cache_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.%s", g_dir, fb_src_kind_name(k), z, x, y, fb_src_ext(k));
}

static void absent_path(fb_tile_kind k, int z, long x, long y, char *p, size_t n) {
    snprintf(p, n, "%s/%s/%d_%ld_%ld.absent", g_dir, fb_src_kind_name(k), z, x, y);
}

static int is_absent(fb_tile_kind k, int z, long x, long y) {
    char p[400]; absent_path(k, z, x, y, p, sizeof p);
    struct stat st;
    if (stat(p, &st) != 0) return 0;
    if (time(0) - st.st_mtime < g_absent_ttl) return 1;
    remove(p);
    return 0;
}

static void mark_absent(fb_tile_kind k, int z, long x, long y) {
    char p[400]; absent_path(k, z, x, y, p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (f) fclose(f);
}

static uint8_t *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    uint8_t * b = (uint8_t *)malloc((size_t)sz);
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
    __atomic_fetch_add(&g_hits, 1, __ATOMIC_RELAXED);
    return 1;
}

int fb_cache_get(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return 0;
    if (fb_cache_ondisk(k, z, x, y, out, n)) return 1;

    if (is_absent(k, z, x, y)) return 0;

    if (k == FB_TILE_IMAGERY && fb_tm_has(z, x, y) == 0) {
        __atomic_fetch_add(&g_absent, 1, __ATOMIC_RELAXED); mark_absent(k, z, x, y);
        return 0;
    }
    char url[600];
    if (!fb_src_url(k, z, x, y, url, sizeof url)) { __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0; }

    uint8_t *body = 0; size_t bn = 0;
    long code = fb_http_get(url, &body, &bn);
    if (code != 200 || bn == 0) {
        free(body);

        if (code == 404) { __atomic_fetch_add(&g_absent, 1, __ATOMIC_RELAXED); mark_absent(k, z, x, y); }
        else __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED);
        return 0;
    }

    char path[400], tmp[460];
    cache_path(k, z, x, y, path, sizeof path);
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)pthread_self());
    FILE *f = fopen(tmp, "wb");
    if (!f || fwrite(body, 1, bn, f) != bn) {
        if (f) fclose(f);
        remove(tmp); free(body);
        __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0;
    }
    fclose(f);
    if (rename(tmp, path) != 0) {
        remove(tmp); free(body);
        __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0;
    }

    *out = body; *n = bn;
    __atomic_fetch_add(&g_fetch, 1, __ATOMIC_RELAXED);
    return 1;
}

fb_tile_state fb_cache_state(fb_tile_kind k, int z, long x, long y, uint8_t **out, size_t *n) {
    if (!fb_src_kind_name(k) || !out || !n) return FB_TILE_UNKNOWN;
    if (fb_cache_ondisk(k, z, x, y, out, n)) return FB_TILE_READY;

    if (is_absent(k, z, x, y)) return FB_TILE_ABSENT;
    return FB_TILE_UNKNOWN;
}

void fb_cache_stats(long *hits, long *fetches, long *fails) {
    if (hits) *hits = __atomic_load_n(&g_hits, __ATOMIC_RELAXED);
    if (fetches) *fetches = __atomic_load_n(&g_fetch, __ATOMIC_RELAXED);
    if (fails) *fails = __atomic_load_n(&g_fail, __ATOMIC_RELAXED);
}

long fb_cache_absent(void) { return __atomic_load_n(&g_absent, __ATOMIC_RELAXED); }
long fb_cache_absent_ttl(void) { return g_absent_ttl; }
