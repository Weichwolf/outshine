#include "route.h"
#include "tilemath.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int read_num(const char **p, long *out) {
    const char *s = *p;
    if (*s < '0' || *s > '9') return 0;
    long v = 0;
    while (*s >= '0' && *s <= '9') {
        if (v > 100000000L) return 0;
        v = v * 10 + (*s++ - '0');
    }
    *out = v; *p = s; return 1;
}

int fb_route_tile(const char *path, fb_tile_kind *k, int *z, long *x, long *y) {
    if (!path || !k || !z || !x || !y) return 0;
    if (strncmp(path, "/t/", 3) != 0) return 0;
    const char *p = path + 3;

    const char *slash = strchr(p, '/');
    if (!slash || slash == p) return 0;
    char kind[24];
    size_t klen = (size_t)(slash - p);
    if (klen >= sizeof kind) return 0;
    memcpy(kind, p, klen); kind[klen] = 0;
    if (!fb_src_kind_parse(kind, k)) return 0;

    p = slash + 1;
    long zz, xx, yy;
    if (!read_num(&p, &zz) || *p++ != '/') return 0;
    if (!read_num(&p, &xx) || *p++ != '/') return 0;
    if (!read_num(&p, &yy)) return 0;
    if (*p && *p != '.') return 0;

    if (zz < 0 || zz > fb_src_max_zoom(*k)) return 0;
    long n = (long)ldexp(1.0, (int)zz);
    if (xx >= n || yy >= n) return 0;

    *z = (int)zz; *x = xx; *y = yy;
    return 1;
}

int fb_query_double(const char *qs, const char *key, double *out) {
    if (!qs || !key || !out) return 0;
    size_t klen = strlen(key);
    for (const char *p = qs; p && *p; ) {

        if (!strncmp(p, key, klen) && p[klen] == '=') {
            const char *v = p + klen + 1;
            char *end = 0;
            double d = strtod(v, &end);
            if (end == v) return 0;
            *out = d; return 1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return 0;
}
