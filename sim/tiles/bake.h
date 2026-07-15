/* FlightBox tiles — baked ground albedo, cached on disk. See bake.c for the why.
 *
 * Same URL shape as the raw tiles, so it shares the cache volume and survives restarts:
 *   /bake/osm/{z}/{x}/{y}?tex=1024    -> PNG   (cartography)
 *   /bake/photo/{z}/{x}/{y}?tex=1024  -> JPEG  (aerial mosaic)
 */
#ifndef FB_BAKE_H
#define FB_BAKE_H
#include <stddef.h>
#include <stdint.h>
#include "raster.h"

int  fb_bake_init(const char *dir);
/* Baked texture bytes for a tile, from disk or freshly baked+stored. Returns 1 and hands over a
 * malloc'd buffer the caller frees; 0 if the source data could not be obtained at all -- which
 * callers must treat as "unknown", never as "empty ground". Caching a missing tile as a plain
 * green square is exactly the bug that put one over a power station. */
int  fb_bake_get(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n);
void fb_bake_stats(long *hits, long *bakes, long *fails);
/* Scanlines refused because their crossing list overflowed or came out odd — a fill we KNOW would
 * have painted a rectangle over the map. Should stay 0; if it climbs, raise FB_XS_MAX. */
long fb_raster_scanline_overflows(void);

#endif /* FB_BAKE_H */
