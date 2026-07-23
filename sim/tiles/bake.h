#ifndef FB_BAKE_H
#define FB_BAKE_H
#include <stddef.h>
#include <stdint.h>
#include "raster.h"

int  fb_bake_init(const char *dir);

int  fb_bake_ondisk(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n);

int  fb_bake_get(fb_albedo_kind k, int z, long x, long y, int TS, uint8_t **out, size_t *n);
void fb_bake_stats(long *hits, long *bakes, long *fails);
/* native_bakes: direct 1x rasterisations (PHOTO always; OSM only at TS>=2048, no supersample
 * headroom above that). super_bakes: OSM TS<2048, rasterised at 2x native then box-downscaled once. */
void fb_bake_stats2(long *native_bakes, long *super_bakes);

long fb_raster_scanline_overflows(void);
long fb_style_unknown_count(void);   /* # of land "kind" hits with no style.h entry (should stay 0) */

#endif
