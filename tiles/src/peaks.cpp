#include "peaks.h"
#include "http.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FB_PEAKS_CELL_DEG 0.5
#define FB_PEAKS_MAX_CELLS 64
#define FB_PEAKS_TIMEOUT_S 120L
#define FB_PEAKS_MAX_BODY (8u << 20)

static char g_dir[320] = "/var/cache/fbtiles/peaks";
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static long g_served = 0, g_cached = 0, g_fetched = 0, g_fail = 0;

static const char *const MIRROR[] = {
    "https://overpass-api.de/api/interpreter?data=",
    "https://overpass.kumi.systems/api/interpreter?data=",
};

int fb_peaks_init(const char *cache_dir) {
    if (cache_dir && *cache_dir) snprintf(g_dir, sizeof g_dir, "%s/peaks", cache_dir);
    mkdir(g_dir, 0755);
    return 0;
}

static void urlenc(const char *s, char *out, size_t n) {
    static const char *hex = "0123456789ABCDEF";
    size_t o = 0;
    for (; *s && o + 4 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 15];
        }
    }
    out[o] = 0;
}

static char *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return 0; }
    char * b = (char *)malloc((size_t)sz + 1);
    if (!b || (sz > 0 && fread(b, 1, (size_t)sz, f) != (size_t)sz)) { free(b); fclose(f); return 0; }
    fclose(f); b[sz] = 0; *n = (size_t)sz; return b;
}

/* Overpass answers CSV directly, so a cell is cacheable and servable verbatim -- no JSON parser in
 * the tile server, and the on-disk form is the wire form. */
static char *fetch_cell(long i, long j, size_t *n) {
    double s = (double)i * FB_PEAKS_CELL_DEG, w = (double)j * FB_PEAKS_CELL_DEG;
    char q[512];
    snprintf(q, sizeof q,
             "[out:csv(::lat,::lon,ele,name;false)][timeout:90];"
             "node(%.4f,%.4f,%.4f,%.4f)[\"natural\"=\"peak\"][\"name\"];out;",
             s, w, s + FB_PEAKS_CELL_DEG, w + FB_PEAKS_CELL_DEG);
    char enc[2048]; urlenc(q, enc, sizeof enc);

    for (size_t m = 0; m < sizeof MIRROR / sizeof *MIRROR; m++) {
        char url[2600];
        snprintf(url, sizeof url, "%s%s", MIRROR[m], enc);
        uint8_t *body = 0; size_t bn = 0;
        long code = fb_http_get_ex(url, &body, &bn, FB_PEAKS_TIMEOUT_S, FB_PEAKS_MAX_BODY);
        if (code == 200) { *n = bn; return (char *)body; }
        free(body);
    }
    return 0;
}

static char *cell(long i, long j, size_t *n) {
    char path[400];
    snprintf(path, sizeof path, "%s/%ld_%ld.tsv", g_dir, i, j);
    char *b = read_file(path, n);
    if (b) { __atomic_fetch_add(&g_cached, 1, __ATOMIC_RELAXED); return b; }

    b = fetch_cell(i, j, n);
    if (!b) { __atomic_fetch_add(&g_fail, 1, __ATOMIC_RELAXED); return 0; }

    char tmp[460];
    snprintf(tmp, sizeof tmp, "%s.%lu.tmp", path, (unsigned long)pthread_self());
    FILE *f = fopen(tmp, "wb");
    if (f) {
        if (fwrite(b, 1, *n, f) == *n && fclose(f) == 0) { if (rename(tmp, path)) remove(tmp); }
        else { remove(tmp); }
    }
    __atomic_fetch_add(&g_fetched, 1, __ATOMIC_RELAXED);
    return b;
}

typedef struct { char *b; size_t n, cap; } buf;

static int buf_add(buf *o, const char *s, size_t n) {
    if (o->n + n + 1 > o->cap) {
        size_t cap = o->cap ? o->cap : 1 << 16;
        while (cap < o->n + n + 1) cap <<= 1;
        char * t = (char *)realloc(o->b, cap);
        if (!t) return 0;
        o->b = t; o->cap = cap;
    }
    memcpy(o->b + o->n, s, n); o->n += n; o->b[o->n] = 0;
    return 1;
}

int fb_peaks_get(double lat, double lon, double radius_m, char **out, size_t *n) {
    if (!out || !n || radius_m <= 0) return 0;
    double mlat = 111132.0, mlon = 111320.0 * cos(lat * M_PI / 180.0);
    double dlat = radius_m / mlat, dlon = radius_m / (mlon > 1.0 ? mlon : 1.0);
    long i0 = (long)floor((lat - dlat) / FB_PEAKS_CELL_DEG), i1 = (long)floor((lat + dlat) / FB_PEAKS_CELL_DEG);
    long j0 = (long)floor((lon - dlon) / FB_PEAKS_CELL_DEG), j1 = (long)floor((lon + dlon) / FB_PEAKS_CELL_DEG);
    if ((i1 - i0 + 1) * (j1 - j0 + 1) > FB_PEAKS_MAX_CELLS) return 0;

    buf o = {0, 0, 0};
    int any = 0;
    pthread_mutex_lock(&g_mx);
    for (long i = i0; i <= i1; i++) {
        for (long j = j0; j <= j1; j++) {
            size_t cn = 0;
            char *c = cell(i, j, &cn);
            if (!c) continue;
            any = 1;
            for (char *p = c; p && *p; ) {
                char *e = strchr(p, '\n');
                size_t len = e ? (size_t)(e - p) : strlen(p);
                double plat, plon;
                if (len > 3 && sscanf(p, "%lf\t%lf", &plat, &plon) == 2) {
                    double dx = (plon - lon) * mlon, dy = (plat - lat) * mlat;
                    if (dx * dx + dy * dy <= radius_m * radius_m) {
                        buf_add(&o, p, len); buf_add(&o, "\n", 1);
                    }
                }
                p = e ? e + 1 : 0;
            }
            free(c);
        }
    }
    pthread_mutex_unlock(&g_mx);

    if (!any) { free(o.b); return 0; }
    if (!o.b) buf_add(&o, "", 0);
    __atomic_fetch_add(&g_served, 1, __ATOMIC_RELAXED);
    *out = o.b; *n = o.n;
    return 1;
}

void fb_peaks_stats(long *served, long *cells_cached, long *cells_fetched, long *fails) {
    if (served) *served = __atomic_load_n(&g_served, __ATOMIC_RELAXED);
    if (cells_cached) *cells_cached = __atomic_load_n(&g_cached, __ATOMIC_RELAXED);
    if (cells_fetched) *cells_fetched = __atomic_load_n(&g_fetched, __ATOMIC_RELAXED);
    if (fails) *fails = __atomic_load_n(&g_fail, __ATOMIC_RELAXED);
}
