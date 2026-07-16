#ifndef FB_TILESRC_H
#define FB_TILESRC_H
#include <stddef.h>

typedef enum {
    FB_TILE_TERRAIN = 0,
    FB_TILE_VECTOR,
    FB_TILE_IMAGERY,
    FB_TILE_KIND_COUNT
} fb_tile_kind;

const char *fb_src_kind_name(fb_tile_kind k);

int         fb_src_kind_parse(const char *s, fb_tile_kind *out);

const char *fb_src_content_type(fb_tile_kind k);

const char *fb_src_ext(fb_tile_kind k);

int         fb_src_max_zoom(fb_tile_kind k);

int         fb_src_url(fb_tile_kind k, int z, long x, long y, char *buf, size_t bufsz);

#endif
