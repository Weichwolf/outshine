/* FlightBox tiles — the tile SOURCES: what kinds exist, where they live, how they're addressed.
 *
 * Deliberately pure and separate from cache.c's I/O so it can be unit-tested to 100%: this is
 * where a wrong URL template or a swapped coordinate would live, and those fail silently —
 * you get a plausible-looking wrong tile, not an error. Esri in particular addresses imagery
 * as {z}/{y}/{x} while everyone else uses {z}/{x}/{y}.
 *
 * All sources are public, CORS-friendly and need no API key. Attribution is owed to each.
 */
#ifndef FB_TILESRC_H
#define FB_TILESRC_H
#include <stddef.h>

typedef enum {
    FB_TILE_TERRAIN = 0,   /* Terrarium-encoded DEM (Tilezen/AWS)  -> image/png  */
    FB_TILE_VECTOR,        /* Shortbread MVT (VersaTiles)          -> protobuf   */
    FB_TILE_IMAGERY,       /* aerial/satellite photo (Esri)        -> image/jpeg */
    FB_TILE_KIND_COUNT
} fb_tile_kind;

/* Route/kind name ("terrain"), or NULL for an invalid kind. */
const char *fb_src_kind_name(fb_tile_kind k);
/* Parse a route segment into a kind. Returns 1 on success. */
int         fb_src_kind_parse(const char *s, fb_tile_kind *out);
/* MIME type to serve this kind with, or NULL for an invalid kind. */
const char *fb_src_content_type(fb_tile_kind k);
/* File extension used in the disk cache, or NULL for an invalid kind. */
const char *fb_src_ext(fb_tile_kind k);
/* Highest zoom the upstream actually has (asking beyond it just wastes a round trip). */
int         fb_src_max_zoom(fb_tile_kind k);
/* Build the upstream URL. Returns 1 on success; 0 for an invalid kind, zoom out of range,
 * or a tile outside the 2^z grid. */
int         fb_src_url(fb_tile_kind k, int z, long x, long y, char *buf, size_t bufsz);

#endif /* FB_TILESRC_H */
