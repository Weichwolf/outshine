#include "tilesrc.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const struct {
    const char *name, *url, *ctype, *ext;
    int maxz;
} K[FB_TILE_KIND_COUNT] = {

    [FB_TILE_TERRAIN] = { "terrain",
        "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%ld/%ld.png",
        "image/png", "png", 15 },

    [FB_TILE_VECTOR]  = { "vector",
        "https://tiles.versatiles.org/tiles/osm/%d/%ld/%ld",
        "application/vnd.mapbox-vector-tile", "pbf", 14 },

    [FB_TILE_IMAGERY] = { "imagery",
        "https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%d/%ld/%ld",
        "image/jpeg", "jpg", 19 },
};

static int kind_ok(fb_tile_kind k) { return k >= 0 && k < FB_TILE_KIND_COUNT; }

const char *fb_src_kind_name(fb_tile_kind k)    { return kind_ok(k) ? K[k].name  : NULL; }
const char *fb_src_content_type(fb_tile_kind k) { return kind_ok(k) ? K[k].ctype : NULL; }
const char *fb_src_ext(fb_tile_kind k)          { return kind_ok(k) ? K[k].ext   : NULL; }
int         fb_src_max_zoom(fb_tile_kind k)     { return kind_ok(k) ? K[k].maxz  : 0; }

int fb_src_kind_parse(const char *s, fb_tile_kind *out) {
    if (!s || !out) return 0;
    for (int i = 0; i < FB_TILE_KIND_COUNT; i++)
        if (!strcmp(s, K[i].name)) { *out = (fb_tile_kind)i; return 1; }
    return 0;
}

int fb_src_url(fb_tile_kind k, int z, long x, long y, char *buf, size_t bufsz) {
    if (!kind_ok(k) || !buf || !bufsz) return 0;
    if (z < 0 || z > K[k].maxz) return 0;
    long n = (long)ldexp(1.0, z);
    if (x < 0 || x >= n || y < 0 || y >= n) return 0;
    if (k == FB_TILE_IMAGERY) snprintf(buf, bufsz, K[k].url, z, y, x);
    else                      snprintf(buf, bufsz, K[k].url, z, x, y);
    return 1;
}
