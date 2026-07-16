#ifndef FB_BAKE_H
#define FB_BAKE_H
#include <stddef.h>
#include <stdint.h>
#include "raster.h"

int  fb_bake_init(const char *dir);

int  fb_bake_ondisk(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n);

int  fb_bake_get(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n);
void fb_bake_stats(long *hits, long *bakes, long *fails);

long fb_raster_scanline_overflows(void);

#endif
