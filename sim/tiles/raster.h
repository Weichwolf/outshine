#ifndef FB_RASTER_H
#define FB_RASTER_H
#include <stdint.h>

typedef enum { FB_ALBEDO_OSM = 0, FB_ALBEDO_PHOTO = 1 } fb_albedo_kind;

int fb_raster_bake(fb_albedo_kind kind, int z, long x, long y, int TS, uint8_t *rgb);

#endif
